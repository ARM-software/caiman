/**
 * Copyright (C) ARM Limited 2010-2014. All rights reserved.
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

#include "Logging.h"

#include <stdio.h>
#include <stdarg.h>

#ifdef WIN32
#define MUTEX_INIT()    mLoggingMutex = CreateMutex(NULL, false, NULL);
#define MUTEX_LOCK()    WaitForSingleObject(mLoggingMutex, 0xFFFFFFFF);
#define MUTEX_UNLOCK()  ReleaseMutex(mLoggingMutex);
#else
#define MUTEX_INIT()    pthread_mutex_init(&mLoggingMutex, NULL)
#define MUTEX_LOCK()    pthread_mutex_lock(&mLoggingMutex)
#define MUTEX_UNLOCK()  pthread_mutex_unlock(&mLoggingMutex)
#endif

// Global thread-safe logging
Logging* logg = NULL;

Logging::Logging(bool debug) {
  mFileCreated = false;
  mWarningXMLPath[0] = 0;
  mDebug = debug;
  mPrintMessages = true;
  MUTEX_INIT();

  strcpy(mErrBuf, "Unknown Error");
  strcpy(mLogBuf, "Unknown Message");
}

Logging::~Logging() {
  if (mFileCreated) {
    util->appendToDisk(mWarningXMLPath, "</warnings>");
  }
}

bool Logging::logWarning(const char* warning) {
  // Append the warning to the warning file if the warning file was created
  if (mWarningXMLPath[0] != 0) {
    if (!mFileCreated) {
      if (util->writeToDisk(mWarningXMLPath, "<?xml version=\"1.0\" encoding='UTF-8'?>\n<warnings version=\"1\">\n") < 0) {
	return false;
      }
      mFileCreated = true;
    }
    util->appendToDisk(mWarningXMLPath, "  <warning text=\"");
    util->appendToDisk(mWarningXMLPath, warning);
    util->appendToDisk(mWarningXMLPath, "\"/>\n");
    return true;
  }

  return false;
}

void Logging::logError(const char* file, int line, const char* fmt, ...) {
  va_list args;

  MUTEX_LOCK();
  if (mDebug) {
    snprintf(mErrBuf, sizeof(mErrBuf), "ERROR[%s:%d]: ", file, line);
  } else {
    mErrBuf[0] = 0;
  }

  va_start(args, fmt);
  vsnprintf(mErrBuf + strlen(mErrBuf), sizeof(mErrBuf) - 2 - strlen(mErrBuf), fmt, args); //  subtract 2 for \n and \0
  va_end(args);

  logWarning(mErrBuf);

  if (strlen(mErrBuf) > 0) {
    strcat(mErrBuf, "\n");
  }
  MUTEX_UNLOCK();
}

void Logging::logMessage(const char* fmt, ...) {
  if (mDebug) {
    va_list args;

    MUTEX_LOCK();
    strcpy(mLogBuf, "INFO: ");

    va_start(args, fmt);
    vsnprintf(mLogBuf + strlen(mLogBuf), sizeof(mLogBuf) - 2 - strlen(mLogBuf), fmt, args); //  subtract 2 for \n and \0
    va_end(args);

    bool warningLogged = logWarning(mLogBuf);

    if (mPrintMessages || !warningLogged) {
      fprintf(stdout, "%s\n", mLogBuf);
      fflush(stdout);
    }

    MUTEX_UNLOCK();
  }
}
