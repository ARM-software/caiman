/**
 * Copyright (C) ARM Limited 2010-2016. All rights reserved.
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

#ifndef __LOGGING_H__
#define __LOGGING_H__

#include <string.h>
#ifdef WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#include "OlyUtility.h"

class Logging
{
public:
    Logging();
    ~Logging();

    void setDebug(bool debug)
    {
        mDebug = debug;
    }
#define logError(...) _logError(__FUNCTION__, __FILE__, __LINE__, __VA_ARGS__)
    void _logError(const char *function, const char *file, int line, const char* fmt, ...);
#define logMessage(...) _logMessage(__FUNCTION__, __FILE__, __LINE__, __VA_ARGS__)
    void _logMessage(const char *function, const char *file, int line, const char* fmt, ...);
    void SetWarningFile(const char* path)
    {
        strncpy(mWarningXMLPath, path, CAIMAN_PATH_MAX);
        mWarningXMLPath[CAIMAN_PATH_MAX - 1] = 0;
    }
    const char* getLastError()
    {
        return mErrBuf;
    }
    void setPrintMessages(const bool printMessages)
    {
        mPrintMessages = printMessages;
    }

private:
    bool logWarning(const char* warning);

    char mWarningXMLPath[CAIMAN_PATH_MAX];
    char mErrBuf[4096]; // Arbitrarily large buffer to hold a string
    bool mDebug;
    bool mFileCreated;
#ifdef WIN32
    HANDLE mLoggingMutex;
#else
    pthread_mutex_t mLoggingMutex;
#endif
    bool mPrintMessages;
};

extern Logging logg;

extern void handleException();

#endif //__LOGGING_H__
