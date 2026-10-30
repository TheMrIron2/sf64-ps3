#ifndef PSP_SETTINGS_PATH_H
#define PSP_SETTINGS_PATH_H

static inline int PspSettingsPath_FromExecutable(char* output, unsigned capacity,
                                                 const char* executablePath, const char* filename) {
    unsigned directoryLength = 0, executableLength = 0, filenameLength = 0, i;
    if (!output || !capacity || !executablePath || !filename) return 0;
    while (executablePath[executableLength]) {
        if (executablePath[executableLength] == '/' || executablePath[executableLength] == '\\')
            directoryLength = executableLength + 1;
        executableLength++;
    }
    while (filename[filenameLength]) filenameLength++;
    if (directoryLength + filenameLength + 1 > capacity) return 0;
    for (i = 0; i < directoryLength; i++) output[i] = executablePath[i];
    for (i = 0; i <= filenameLength; i++) output[directoryLength + i] = filename[i];
    return 1;
}

static inline int PspSettingsPath_AppendSuffix(char* output, unsigned capacity,
                                               const char* path, const char* suffix) {
    unsigned pathLength = 0, suffixLength = 0, i;
    if (!output || !capacity || !path || !suffix) return 0;
    while (path[pathLength]) pathLength++;
    while (suffix[suffixLength]) suffixLength++;
    if (pathLength + suffixLength + 1 > capacity) return 0;
    for (i = 0; i < pathLength; i++) output[i] = path[i];
    for (i = 0; i <= suffixLength; i++) output[pathLength + i] = suffix[i];
    return 1;
}

#endif
