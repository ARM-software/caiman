/**
 * Copyright (C) 2013-2020 by Arm Limited. All rights reserved.
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

#if defined(SUPPORT_DAQ)

#include "DAQmxFuncs.h"

#include <stdio.h>

#include "Logging.h"

// Utility for formatting errors from DAQ dll. Logs
// the error, so all the caller has to do is propagate error
void DAQmxFuncs::_handleError(const char *function, const char *file, int line, const char *id) {
    static const int BUFLEN=2048;
    char buf[BUFLEN];
    buf[BUFLEN-1] = 0;
    getExtendedErrorInfo(buf, BUFLEN-2);
    int len = strlen(buf);
    int want = 80+strlen(id);
    if ((BUFLEN-len) > want) {
        snprintf(&buf[len], want, " (DAQmx code %d, in '%s.')", (int)m_lastStatus, id);
    }

    logg._logError(function, file, line, buf);
    handleException();
}

// 'Friendly' version that doesn't report the sometimes cryptic extended DAQ error info
void DAQmxFuncs::handleFriendlyError(const char *msg) {
    static const int BUFLEN=2048;
    char buf[BUFLEN];
    buf[BUFLEN-1] = 0;
    snprintf(buf, BUFLEN, "%s (DAQmx code %d)", msg, (int)m_lastStatus);
    logg.logError(buf);
    handleException();
}

DAQmxFuncs * DAQmxFuncs::getInstance() {
#ifdef NI_DAQMX_SUPPORT
    DAQmxFuncs * daqMx = getDAQmx();
    if (daqMx->loadDlls()) {
        return daqMx;
    }
#endif

    DAQmxFuncs * daqMxBase = getDAQmxBase();
    if (daqMxBase->loadDlls()) {
        return daqMxBase;
    }

    const int bitsize = 8*sizeof(void *);
    const int otherBitsize = bitsize == 32 ? 64 : 32;
    const char *const msg =
    "Unable to find a %i-bit version of "
#ifdef NI_DAQMX_SUPPORT
    "NI-DAQmx or "
#endif
    "NI-DAQmx Base from National Instruments. If it is already installed, you may need to try the %i-bit version of caiman.";
    logg.logError(msg, bitsize, otherBitsize);
    handleException();

    return NULL;
}

#endif
