#include "src/psp/eeprom_store.h"
#include "src/psp/settings_path.h"

static unsigned eeprom_crc32(const unsigned char* data, unsigned size) {
    unsigned crc = 0xFFFFFFFFU;
    unsigned i;
    for (i = 0; i < size; i++) {
        unsigned bit;
        crc ^= data[i];
        for (bit = 0; bit < 8; bit++) crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
    }
    return ~crc;
}

static int eeprom_suffix(const PspEepromStore* store, char* output, const char* suffix) {
    return PspSettingsPath_AppendSuffix(output, sizeof(store->path), store->path, suffix);
}

static int eeprom_read_file(PspEepromStore* store, const char* path, unsigned char* image) {
    unsigned char header[12], extra;
    void* file = store->ops->open(path, 0);
    unsigned i, crc;
    int ok;
    if (file == 0) return 0;
    ok = store->ops->read(file, header, sizeof(header)) == (int) sizeof(header) &&
         header[0] == 'S' && header[1] == 'F' && header[2] == 'E' && header[3] == 'E' &&
         header[4] == 1 && header[5] == 0 && header[6] == 0 && header[7] == 0 &&
         store->ops->read(file, image, PSP_EEPROM_BYTES) == PSP_EEPROM_BYTES &&
         store->ops->read(file, &extra, 1) == 0;
    if (store->ops->close(file) < 0) ok = 0;
    if (!ok) return 0;
    crc = 0;
    for (i = 0; i < 4; i++) crc |= (unsigned) header[8 + i] << (8 * i);
    return crc == eeprom_crc32(image, PSP_EEPROM_BYTES);
}

static int eeprom_write_file(PspEepromStore* store, const char* path) {
    unsigned char header[12] = { 'S', 'F', 'E', 'E', 1, 0, 0, 0 };
    unsigned crc = eeprom_crc32(store->image, PSP_EEPROM_BYTES), i;
    void* file;
    int ok;
    for (i = 0; i < 4; i++) header[8 + i] = (unsigned char) (crc >> (8 * i));
    store->ops->remove(path);
    file = store->ops->open(path, 1);
    if (file == 0) return 0;
    ok = store->ops->write(file, header, sizeof(header)) == (int) sizeof(header) &&
         store->ops->write(file, store->image, PSP_EEPROM_BYTES) == PSP_EEPROM_BYTES;
    if (store->ops->close(file) < 0) ok = 0;
    if (ok) {
        unsigned char check[PSP_EEPROM_BYTES];
        ok = eeprom_read_file(store, path, check);
    }
    return ok;
}

static int eeprom_commit(PspEepromStore* store) {
    char temp[256], backup[256];
    unsigned char prior[PSP_EEPROM_BYTES];
    int hadPrimary;
    if (!eeprom_suffix(store, temp, ".tmp") || !eeprom_suffix(store, backup, ".bak")) return 0;
    if (!eeprom_write_file(store, temp)) return 0;
    hadPrimary = eeprom_read_file(store, store->path, prior);
    if (hadPrimary) {
        store->ops->remove(backup);
        if (store->ops->rename(store->path, backup) < 0) return 0;
    } else {
        store->ops->remove(store->path);
    }
    if (store->ops->rename(temp, store->path) < 0) {
        if (hadPrimary) store->ops->rename(backup, store->path);
        return 0;
    }
    store->ops->remove(backup);
    return 1;
}

int PspEepromStore_Init(PspEepromStore* store, const PspEepromFileOps* ops, const char* path) {
    char backup[256], temp[256];
    unsigned i;
    if (!store) return 0;
    store->initialized = 0;
    store->transaction = store->dirty = 0;
    if (!ops || !path || !ops->open || !ops->read || !ops->write || !ops->close ||
        !ops->rename || !ops->remove || !ops->exists) return 0;
    for (i = 0; path[i] && i < sizeof(store->path) - 1; i++) store->path[i] = path[i];
    if (path[i]) return 0;
    store->path[i] = 0;
    store->ops = ops;
    if (!eeprom_suffix(store, backup, ".bak") || !eeprom_suffix(store, temp, ".tmp")) return 0;
    if (eeprom_read_file(store, store->path, store->image)) {
        store->initialized = 1;
        return 1;
    }
    if (eeprom_read_file(store, backup, store->image)) {
        store->ops->remove(store->path);
        store->ops->rename(backup, store->path);
        store->initialized = 1;
        return 1;
    }
    if (eeprom_read_file(store, temp, store->image)) {
        store->ops->remove(store->path);
        store->ops->rename(temp, store->path);
        store->initialized = 1;
        return 1;
    }
    if (store->ops->exists(store->path) || store->ops->exists(backup) || store->ops->exists(temp)) return 0;
    for (i = 0; i < PSP_EEPROM_BYTES; i++) store->image[i] = 0;
    store->initialized = 1;
    return 1;
}

int PspEepromStore_Read(PspEepromStore* store, unsigned address, void* buffer, int length) {
    unsigned i;
    if (!store || !store->initialized || !buffer || length <= 0 || (length & 7) || address >= 64 ||
        (unsigned) length > PSP_EEPROM_BYTES - address * 8) return -1;
    for (i = 0; i < (unsigned) length; i++) ((unsigned char*) buffer)[i] = store->image[address * 8 + i];
    return 0;
}

int PspEepromStore_Write(PspEepromStore* store, unsigned address, const void* buffer, int length) {
    unsigned i;
    int changed = 0;
    if (!store || !store->initialized || !buffer || length <= 0 || (length & 7) || address >= 64 ||
        (unsigned) length > PSP_EEPROM_BYTES - address * 8) return -1;
    for (i = 0; i < (unsigned) length; i++) {
        unsigned offset = address * 8 + i;
        if (store->image[offset] != ((const unsigned char*) buffer)[i]) changed = 1;
    }
    if (!changed) return 0;
    if (store->transaction) {
        for (i = 0; i < (unsigned) length; i++) store->image[address * 8 + i] = ((const unsigned char*) buffer)[i];
        store->dirty = 1;
        return 0;
    }
    for (i = 0; i < (unsigned) length; i++) {
        unsigned offset = address * 8 + i;
        store->before[i] = store->image[offset];
        store->image[offset] = ((const unsigned char*) buffer)[i];
    }
    if (!eeprom_commit(store)) {
        for (i = 0; i < (unsigned) length; i++) store->image[address * 8 + i] = store->before[i];
        return -1;
    }
    return 0;
}

int PspEepromStore_Begin(PspEepromStore* store) {
    unsigned i;
    if (!store || !store->initialized || store->transaction) return 0;
    for (i = 0; i < PSP_EEPROM_BYTES; i++) store->before[i] = store->image[i];
    store->transaction = 1;
    store->dirty = 0;
    return 1;
}

int PspEepromStore_Commit(PspEepromStore* store) {
    unsigned i;
    int ok = 1;
    if (!store || !store->transaction) return 0;
    if (store->dirty) ok = eeprom_commit(store);
    if (!ok) for (i = 0; i < PSP_EEPROM_BYTES; i++) store->image[i] = store->before[i];
    store->transaction = store->dirty = 0;
    return ok;
}

void PspEepromStore_Abort(PspEepromStore* store) {
    unsigned i;
    if (!store || !store->transaction) return;
    for (i = 0; i < PSP_EEPROM_BYTES; i++) store->image[i] = store->before[i];
    store->transaction = store->dirty = 0;
}
