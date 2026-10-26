#ifndef PSP_EEPROM_STORE_H
#define PSP_EEPROM_STORE_H

#define PSP_EEPROM_BYTES 512

typedef struct {
    void* (*open)(const char* path, int mode);
    int (*read)(void* file, void* data, unsigned size);
    int (*write)(void* file, const void* data, unsigned size);
    int (*close)(void* file);
    int (*rename)(const char* from, const char* to);
    int (*remove)(const char* path);
    int (*exists)(const char* path);
} PspEepromFileOps;

typedef struct {
    const PspEepromFileOps* ops;
    char path[256];
    unsigned char image[PSP_EEPROM_BYTES];
    unsigned char before[PSP_EEPROM_BYTES];
    int initialized;
    int transaction;
    int dirty;
} PspEepromStore;

int PspEepromStore_Init(PspEepromStore* store, const PspEepromFileOps* ops, const char* path);
int PspEepromStore_Read(PspEepromStore* store, unsigned address, void* buffer, int length);
int PspEepromStore_Write(PspEepromStore* store, unsigned address, const void* buffer, int length);
int PspEepromStore_Begin(PspEepromStore* store);
int PspEepromStore_Commit(PspEepromStore* store);
void PspEepromStore_Abort(PspEepromStore* store);
void PspEeprom_SetExecutablePath(const char* executablePath);
int PspEeprom_BeginTransaction(void);
int PspEeprom_CommitTransaction(void);
void PspEeprom_AbortTransaction(void);

#endif
