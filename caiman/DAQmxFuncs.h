/**
 * Copyright (C) ARM Limited 2013-2015. All rights reserved.
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

#ifndef DAQMX_H
#define DAQMX_H

#include <stdint.h>

#if defined(WIN32)
#define NI_DAQMX_SUPPORT
#endif

class DAQmxFuncs {
public:
  static DAQmxFuncs * getInstance();

  virtual ~DAQmxFuncs () {}

  virtual bool cfgSampClkTiming(const char arg1[], double arg2, uint64_t arg5) = 0;
  virtual bool clearTask() = 0;
  virtual bool createAIVoltageChan(const char arg1[], const char arg2[], double arg4, double arg5, const char arg6[]) = 0;
  virtual bool createTask(const char arg0[]) = 0;
  virtual bool getDevSerialNum(const char arg0[], uint32_t *arg1) = 0;
  virtual bool getExtendedErrorInfo(char errorString[], uint32_t bufferSize) = 0;
  virtual bool getSysDevNames(char * arg1, uint32_t arg2) = 0;
  virtual bool readAnalogF64(int32_t arg1, double arg2, double arg4[], uint32_t arg5, int32_t *arg6, uint32_t *arg7) = 0;
  virtual bool startTask() = 0;
  virtual bool stopTask() = 0;

#define handleError(id) _handleError(__FUNCTION__, __FILE__, __LINE__, id)
  void _handleError(const char *function, const char *file, int line, const char *id);
  void handleFriendlyError(const char *msg);

protected:
  DAQmxFuncs () : m_lastStatus(0) {}

  virtual bool loadDlls() = 0;

  signed long m_lastStatus;

private:
#ifdef NI_DAQMX_SUPPORT
  static DAQmxFuncs * getDAQmx();
#endif
  static DAQmxFuncs * getDAQmxBase();
};

#endif // DAQMX_H
