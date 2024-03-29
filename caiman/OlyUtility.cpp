/**
 * Copyright (C) 2010-2021 by Arm Limited. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "OlyUtility.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <unistd.h>
#elif defined(DARWIN)
#include <mach-o/dyld.h>
#endif

bool stringToBool(const char* string, bool defValue)
{
    char value[32];

    if (string == NULL) {
        return defValue;
    }

    strncpy(value, string, sizeof(value));
    value[sizeof(value) - 1] = 0; // strncpy does not guarantee a null-terminated string
    if (value[0] == 0) {
        return defValue;
    }

    // Convert to lowercase
    int i = 0;
    while (value[i]) {
        value[i] = tolower(value[i]);
        i++;
    }

    if (strcmp(value, "true") == 0 || strcmp(value, "yes") == 0 || strcmp(value, "1") == 0 || strcmp(value, "on") == 0) {
        return true;
    }
    else if (strcmp(value, "false") == 0 || strcmp(value, "no") == 0 || strcmp(value, "0") == 0 || strcmp(value, "off") == 0) {
        return false;
    }
    else {
        return defValue;
    }
}

void stringToLower(char* string)
{
    if (string == NULL) {
        return;
    }

    while (*string) {
        *string = tolower(*string);
        string++;
    }
}

bool stringToLongLong(long long * const value, const char *str, const int base)
{
    char *endptr;
    long long v;
    errno = 0;
    v = strtoll(str, &endptr, base);
    if (errno != 0 || *endptr != '\0') {
        return false;
    }
    *value = v;

    return true;
}

bool stringToLong(long * const value, const char *str, const int base)
{
    char *endptr;
    long v;
    errno = 0;
    v = strtol(str, &endptr, base);
    if (errno != 0 || *endptr != '\0') {
        return false;
    }
    *value = v;

    return true;
}

bool stringToInt(int * const value, const char *str, const int base)
{
    long v;
    if (!stringToLong(&v, str, base)) {
        return false;
    }
    *value = v;

    return true;
}

// Modifies fullpath with the path part including the trailing path separator
int getApplicationFullPath(char* fullpath, int sizeOfPath)
{
    memset(fullpath, 0, sizeOfPath);
#if defined(WIN32)
    int length = GetModuleFileName(NULL, fullpath, sizeOfPath);
#elif defined(__linux__)
    int length = readlink("/proc/self/exe", fullpath, sizeOfPath);
#elif defined(DARWIN)
    uint32_t length_u = (uint32_t)sizeOfPath;
    int length = sizeOfPath;
    if (_NSGetExecutablePath(fullpath, &length_u) == 0) {
        length = strlen(fullpath);
    }
#endif

    if (length == sizeOfPath) {
        return -1;
    }

    fullpath[length] = 0;
    getPathPart(fullpath);

    return 0;
}

char* readFromDisk(const char* file, unsigned int *size, bool appendNull)
{
    // Open the file
    FILE* pFile = fopen(file, "rb");
    if (pFile == NULL) {
        return NULL;
    }

    // Obtain file size
    fseek(pFile, 0, SEEK_END);
    unsigned int lSize = ftell(pFile);
    rewind(pFile);

    // Allocate memory to contain the whole file
    char* buffer = (char*) malloc(lSize + (int) appendNull);
    if (buffer == NULL) {
        fclose(pFile);
        return NULL;
    }

    // Copy the file into the buffer
    if (fread(buffer, 1, lSize, pFile) != lSize) {
        free(buffer);
        fclose(pFile);
        return NULL;
    }

    // Terminate
    fclose(pFile);

    if (appendNull) {
        buffer[lSize] = 0;
    }

    if (size) {
        *size = lSize;
    }

    return buffer;
}

int writeToDisk(const char* path, const char* data)
{
    // Open the file
    FILE* pFile = fopen(path, "wb");
    if (pFile == NULL) {
        return -1;
    }

    // Write the data to disk
    if (fwrite(data, 1, strlen(data), pFile) != strlen(data)) {
        fclose(pFile);
        return -1;
    }

    // Terminate
    fclose(pFile);
    return 0;
}

int appendToDisk(const char* path, const char* data)
{
    // Open the file
    FILE* pFile = fopen(path, "a");
    if (pFile == NULL) {
        return -1;
    }

    // Write the data to disk
    if (fwrite(data, 1, strlen(data), pFile) != strlen(data)) {
        fclose(pFile);
        return -1;
    }

    // Terminate
    fclose(pFile);
    return 0;
}

/**
 * Copies the srcFile into dstFile in 1kB chunks.
 * The dstFile will be overwritten if it exists.
 * 0 is returned on an error; otherwise 1.
 */
#define TRANSFER_SIZE 1024
int copyFile(const char* srcFile, const char* dstFile)
{
    char buffer[TRANSFER_SIZE];
    FILE * f_src = fopen(srcFile, "rb");
    if (!f_src) {
        return 0;
    }
    FILE * f_dst = fopen(dstFile, "wb");
    if (!f_dst) {
        fclose(f_src);
        return 0;
    }
    while (!feof(f_src)) {
        int num_bytes_read = fread(buffer, 1, TRANSFER_SIZE, f_src);
        if (num_bytes_read < TRANSFER_SIZE && !feof(f_src)) {
            fclose(f_src);
            fclose(f_dst);
            return 0;
        }
        int num_bytes_written = fwrite(buffer, 1, num_bytes_read, f_dst);
        if (num_bytes_written != num_bytes_read) {
            fclose(f_src);
            fclose(f_dst);
            return 0;
        }
    }
    fclose(f_src);
    fclose(f_dst);
    return 1;
}

const char* getFilePart(const char* path)
{
    const char* last_sep = strrchr(path, PATH_SEPARATOR);

    // in case path is not a full path
    if (last_sep == NULL) {
        return path;
    }

    return last_sep++;
}

// getPathPart may modify the contents of path
// returns the path including the trailing path separator
char* getPathPart(char* path)
{
    char* last_sep = strrchr(path, PATH_SEPARATOR);

    // in case path is not a full path
    if (last_sep == NULL) {
        return 0;
    }
    last_sep++;
    *last_sep = 0;

    return (path);
}
