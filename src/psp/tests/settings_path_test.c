#include "src/psp/settings_path.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    char path[256], suffix[256], backup[256], longExe[300], sentinel[256];
    char directory[] = "/tmp/sf64-settings-test-XXXXXX";
    char executable[384], data[64];
    FILE* file;
    unsigned i;
    assert(PspSettingsPath_FromExecutable(path, sizeof(path), "ms0:/PSP/GAME/SF64/EBOOT.PBP", "sf64-settings.cfg"));
    assert(strcmp(path, "ms0:/PSP/GAME/SF64/sf64-settings.cfg") == 0);
    assert(PspSettingsPath_FromExecutable(path, sizeof(path), "ef0:/PSP/GAME/SF64/EBOOT.PBP", "sf64-settings.cfg"));
    assert(strcmp(path, "ef0:/PSP/GAME/SF64/sf64-settings.cfg") == 0);
    assert(PspSettingsPath_FromExecutable(path, sizeof(path), "ef0:\\PSP\\GAME\\SF64\\EBOOT.PBP", "sf64-settings.cfg"));
    assert(strcmp(path, "ef0:\\PSP\\GAME\\SF64\\sf64-settings.cfg") == 0);
    assert(PspSettingsPath_FromExecutable(path, sizeof(path), NULL, "sf64-settings.cfg") == 0);
    assert(PspSettingsPath_AppendSuffix(suffix, sizeof(suffix), path, ".tmp"));
    assert(strcmp(suffix, "ef0:\\PSP\\GAME\\SF64\\sf64-settings.cfg.tmp") == 0);

    for (i = 0; i < sizeof(longExe) - 1; i++) longExe[i] = 'x';
    longExe[sizeof(longExe) - 1] = 0;
    longExe[237] = '/';
    assert(PspSettingsPath_FromExecutable(path, sizeof(path), longExe, "sf64-settings.cfg"));
    assert(strlen(path) == 255);
    assert(PspSettingsPath_AppendSuffix(suffix, sizeof(suffix), path, ".tmp") == 0);
    assert(PspSettingsPath_AppendSuffix(suffix, sizeof(suffix), path, ".bak") == 0);
    strcpy(path, "preserve");
    longExe[238] = '/';
    assert(PspSettingsPath_FromExecutable(path, sizeof(path), longExe, "sf64-settings.cfg") == 0);
    assert(strcmp(path, "preserve") == 0);
    longExe[238] = 'x';
    assert(PspSettingsPath_FromExecutable(path, sizeof(path), longExe, "sf64-settings.cfg"));
    strcpy(suffix, "unchanged");
    assert(PspSettingsPath_AppendSuffix(suffix, sizeof(suffix), path, ".tmp") == 0);
    assert(strcmp(suffix, "unchanged") == 0);

    for (i = 0; i < sizeof(sentinel) - 5; i++) sentinel[i] = 'd';
    sentinel[sizeof(sentinel) - 5] = 0;
    assert(PspSettingsPath_AppendSuffix(suffix, sizeof(suffix), sentinel, ".bak"));
    assert(strlen(suffix) == strlen(sentinel) + 4);

    assert(mkdtemp(directory) != NULL);
    snprintf(executable, sizeof(executable), "%s/EBOOT.PBP", directory);
    assert(PspSettingsPath_FromExecutable(path, sizeof(path), executable, "sf64-settings.cfg"));
    assert(PspSettingsPath_AppendSuffix(suffix, sizeof(suffix), path, ".tmp"));
    assert(PspSettingsPath_AppendSuffix(backup, sizeof(backup), path, ".bak"));
    file = fopen(path, "wb");
    assert(file != NULL && fputs("version=1\ngraphics_backend=gu\n", file) >= 0 && fclose(file) == 0);
    file = fopen(suffix, "wb");
    assert(file != NULL && fputs("version=2\ngraphics_backend=pspgl\n", file) >= 0 && fclose(file) == 0);
    assert(rename(path, backup) == 0);
    assert(rename(suffix, path) == 0);
    file = fopen(path, "rb");
    assert(file != NULL && fgets(data, sizeof(data), file) != NULL);
    assert(strcmp(data, "version=2\n") == 0 && fclose(file) == 0);

    /* Model interruption after preserving the old settings file */
    assert(rename(path, suffix) == 0);
    assert(rename(backup, path) == 0);
    file = fopen(path, "rb");
    assert(file != NULL && fgets(data, sizeof(data), file) != NULL);
    assert(strcmp(data, "version=1\n") == 0 && fclose(file) == 0);
    assert(access(suffix, F_OK) == 0);
    remove(path);
    remove(suffix);
    rmdir(directory);
    return 0;
}
