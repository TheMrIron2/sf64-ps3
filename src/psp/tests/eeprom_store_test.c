#include "src/psp/eeprom_store.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

static int fail_write;
static int fail_close;
static int fail_rename_on;
static int rename_count;
static int file_ops;
static void* open_file(const char* path, int mode) { file_ops++; return fopen(path, mode ? "wb" : "rb"); }
static int read_file(void* f, void* data, unsigned n) { file_ops++; return (int) fread(data, 1, n, f); }
static int write_file(void* f, const void* data, unsigned n) {
    file_ops++;
    if (fail_write) { fail_write = 0; return (int) fwrite(data, 1, n / 2, f); }
    return (int) fwrite(data, 1, n, f);
}
static int close_file(void* f) {
    file_ops++;
    int result = fclose(f);
    if (fail_close) { fail_close = 0; return -1; }
    return result == 0 ? 0 : -1;
}
static int rename_file(const char* a, const char* b) {
    file_ops++;
    rename_count++;
    if (rename_count == fail_rename_on) return -1;
    return rename(a, b);
}
static int remove_file(const char* p) { file_ops++; return remove(p); }
static int exists_file(const char* p) { struct stat st; file_ops++; return stat(p, &st) == 0; }

int main(void) {
    static const PspEepromFileOps ops = { open_file, read_file, write_file, close_file, rename_file, remove_file, exists_file };
    PspEepromStore store, reload;
    unsigned char input[512], output[512], block[8];
    unsigned i, long_length;
    char path[128], pathbuf[512], long_path[253];
    snprintf(path, sizeof(path), "/tmp/sf64-eeprom-test-%ld.bin", (long) getpid());
    snprintf(long_path, sizeof(long_path), "/tmp/sf64-eeprom-long-%ld-", (long) getpid());
    long_length = strlen(long_path);
    while (long_length < 251) long_path[long_length++] = 'x';
    long_path[long_length] = 0;
    assert(PspEepromStore_Init(&store, &ops, long_path));
    assert(strcmp(store.path, long_path) == 0);
    long_path[long_length++] = 'x';
    long_path[long_length] = 0;
    {
        int operations_before = file_ops;
        assert(!PspEepromStore_Init(&store, &ops, long_path));
        assert(!store.initialized && file_ops == operations_before);
    }
    unlink(path);
    snprintf(pathbuf, sizeof(pathbuf), "%s.tmp", path); unlink(pathbuf);
    snprintf(pathbuf, sizeof(pathbuf), "%s.bak", path); unlink(pathbuf);
    assert(PspEepromStore_Init(&store, &ops, path));
    memset(output, 1, sizeof(output));
    assert(PspEepromStore_Read(&store, 0, output, 512) == 0);
    for (i = 0; i < 512; i++) assert(output[i] == 0);
    for (i = 0; i < sizeof(input); i++) input[i] = (unsigned char) (i * 73 + 9);
    assert(PspEepromStore_Write(&store, 0, input, 512) == 0);
    assert(PspEepromStore_Init(&reload, &ops, path));
    assert(PspEepromStore_Read(&reload, 0, output, 512) == 0 && memcmp(input, output, 512) == 0);
    memcpy(block, input + 63 * 8, 8);
    assert(PspEepromStore_Write(&reload, 63, block, 8) == 0);
    assert(PspEepromStore_Read(&reload, 63, output, 8) == 0 && memcmp(block, output, 8) == 0);
    rename_count = 0;
    assert(PspEepromStore_Write(&reload, 63, block, 8) == 0 && rename_count == 0);
    assert(PspEepromStore_Read(&reload, 64, output, 8) < 0);
    assert(PspEepromStore_Read(&reload, 0, output, 0) < 0);
    assert(PspEepromStore_Read(&reload, 0, output, -8) < 0);
    assert(PspEepromStore_Read(&reload, 0, NULL, 8) < 0);
    assert(PspEepromStore_Write(&reload, 0, NULL, 8) < 0);
    assert(PspEepromStore_Read(&reload, 0, output, 7) < 0);
    assert(PspEepromStore_Read(&reload, 0, output, 520) < 0);
    assert(PspEepromStore_Read(&reload, 63, output, 16) < 0);
    assert(PspEepromStore_Read(&reload, 2, output, 24) == 0 && memcmp(output, input + 16, 24) == 0);
    memset(block, 0x3C, sizeof(block));
    fail_write = 1;
    assert(PspEepromStore_Write(&reload, 10, block, 8) < 0);
    assert(PspEepromStore_Read(&reload, 10, output, 8) == 0 && memcmp(output, input + 80, 8) == 0);
    assert(PspEepromStore_Write(&reload, 10, block, 8) == 0);
    assert(PspEepromStore_Begin(&reload));
    memset(block, 0xA5, sizeof(block));
    assert(PspEepromStore_Write(&reload, 2, block, 8) == 0);
    fail_write = 1;
    assert(!PspEepromStore_Commit(&reload));
    assert(PspEepromStore_Read(&reload, 2, output, 8) == 0 && memcmp(output, input + 16, 8) == 0);
    assert(PspEepromStore_Begin(&reload));
    assert(PspEepromStore_Write(&reload, 2, block, 8) == 0);
    fail_close = 1;
    assert(!PspEepromStore_Commit(&reload));
    assert(PspEepromStore_Begin(&reload));
    assert(PspEepromStore_Write(&reload, 2, block, 8) == 0);
    assert(PspEepromStore_Commit(&reload));
    assert(PspEepromStore_Begin(&reload));
    assert(PspEepromStore_Write(&reload, 2, block, 8) == 0);
    assert(PspEepromStore_Commit(&reload));
    assert(PspEepromStore_Read(&reload, 2, output, 8) == 0 && memcmp(output, block, 8) == 0);
    assert(PspEepromStore_Begin(&reload));
    block[0] ^= 0xFF;
    assert(PspEepromStore_Write(&reload, 2, block, 8) == 0);
    rename_count = 0; fail_rename_on = 2;
    assert(!PspEepromStore_Commit(&reload));
    fail_rename_on = 0;
    assert(PspEepromStore_Read(&reload, 2, output, 8) == 0 && output[0] == 0xA5);
    assert(PspEepromStore_Begin(&reload));
    assert(PspEepromStore_Write(&reload, 2, block, 8) == 0);
    assert(PspEepromStore_Commit(&reload));
    snprintf(pathbuf, sizeof(pathbuf), "%s.bak", path); unlink(pathbuf);
    snprintf(pathbuf, sizeof(pathbuf), "%s.bak", path); assert(rename(path, pathbuf) == 0);
    FILE* f = fopen(path, "wb"); assert(f); fwrite("bad", 1, 3, f); fclose(f);
    assert(PspEepromStore_Init(&reload, &ops, path));
    assert(PspEepromStore_Read(&reload, 0, output, 8) == 0);
    assert(output[0] == input[0]);
    snprintf(pathbuf, sizeof(pathbuf), "%s.tmp", path);
    {
        unsigned char copy[600];
        size_t n;
        FILE* source = fopen(path, "rb");
        FILE* target = fopen(pathbuf, "wb");
        assert(source && target);
        while ((n = fread(copy, 1, sizeof(copy), source)) != 0) assert(fwrite(copy, 1, n, target) == n);
        fclose(source); fclose(target);
    }
    unlink(path);
    snprintf(pathbuf, sizeof(pathbuf), "%s.bak", path); unlink(pathbuf);
    assert(PspEepromStore_Init(&reload, &ops, path));
    assert(PspEepromStore_Read(&reload, 2, output, 8) == 0 && output[0] == block[0]);
    snprintf(pathbuf, sizeof(pathbuf), "%s.bak", path); unlink(pathbuf);
    f = fopen(path, "wb"); assert(f); fwrite("bad", 1, 3, f); fclose(f);
    assert(!PspEepromStore_Init(&reload, &ops, path));
    unlink(path);
    snprintf(pathbuf, sizeof(pathbuf), "%s.tmp", path); unlink(pathbuf);
    snprintf(pathbuf, sizeof(pathbuf), "%s.bak", path); unlink(pathbuf);
    return 0;
}
