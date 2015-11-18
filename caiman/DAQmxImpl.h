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

#include <stddef.h>
#include <assert.h>

#include "Dll.h"

#define CAT2(LHS, RHS) LHS ## RHS
#define CAT(LHS, RHS) CAT2(LHS, RHS)

#ifdef NI_DAQMX_SUPPORT

#include "NIDAQmx.h"

#ifdef NI_RUNTIME_LINK
#define DAQmxFuncStr(NAME) "DAQmx" NAME
#else
#define DAQmxFunc(NAME) DAQmx ## NAME
#endif

#define DAQMX DAQmx

#else

#include "NIDAQmxBase.h"

#ifdef NI_RUNTIME_LINK
#define DAQmxFuncStr(NAME) "DAQmxBase" NAME
#else
#define DAQmxFunc(NAME) DAQmxBase ## NAME
#endif

#define DAQMX DAQmxBase

#endif

#ifdef NI_RUNTIME_LINK
#define DAQmxFunc(NAME) m_ ## NAME
#endif

class DAQMX : public DAQmxFuncs {
public:
  DAQMX () : m_dllsLoaded(false), m_tH(0) {
    assert(sizeof(uint32_t) == sizeof(uInt32));
    assert(sizeof(int32_t) == sizeof(int32));
    assert(sizeof(uint32_t) == sizeof(bool32));
  }

  ~DAQMX () {
  }

  bool cfgSampClkTiming(const char arg1[], double arg2, uint64_t arg5) {
    m_lastStatus = DAQmxFunc(CfgSampClkTiming)(m_tH, arg1, arg2, DAQmx_Val_Rising, DAQmx_Val_ContSamps, arg5);
    return !DAQmxFailed(m_lastStatus);
  }

  bool clearTask() {
    m_lastStatus = DAQmxFunc(ClearTask)(m_tH);
    return !DAQmxFailed(m_lastStatus);
  }

  bool createAIVoltageChan(const char arg1[], const char arg2[], double arg4, double arg5, const char arg6[]) {
    m_lastStatus = DAQmxFunc(CreateAIVoltageChan)(m_tH, arg1, arg2, DAQmx_Val_Cfg_Default, arg4, arg5, DAQmx_Val_Volts, arg6);
    return !DAQmxFailed(m_lastStatus);
  }

  bool createTask(const char arg0[]) {
    m_lastStatus = DAQmxFunc(CreateTask)(arg0, &m_tH);
    return !DAQmxFailed(m_lastStatus);
  }

  bool getDevSerialNum(const char arg0[], uint32_t *arg1) {
    m_lastStatus = DAQmxFunc(GetDevSerialNum)(arg0, (uInt32 *)arg1);
    return !DAQmxFailed(m_lastStatus);
  }

  bool getExtendedErrorInfo(char errorString[], uint32_t bufferSize) {
    m_lastStatus = DAQmxFunc(GetExtendedErrorInfo)(errorString, bufferSize);
    return !DAQmxFailed(m_lastStatus);
  }

  bool getSysDevNames(char * arg1, uint32_t arg2) {
#if defined(NI_RUNTIME_LINK) || defined(NI_DAQMX_SUPPORT)
#ifdef NI_RUNTIME_LINK
    if (DAQmxFunc(GetSysDevNames) == NULL) {
      return false;
    }
#endif
    m_lastStatus = DAQmxFunc(GetSysDevNames)(arg1, arg2);
    return !DAQmxFailed(m_lastStatus);
#else
    return false;
#endif
  }

  bool readAnalogF64(int32_t arg1, double arg2, double arg4[], uint32_t arg5, int32_t *arg6, uint32_t *arg7) {
    m_lastStatus = DAQmxFunc(ReadAnalogF64)(m_tH, arg1, arg2, DAQmx_Val_GroupByScanNumber, arg4, arg5, (int32 *)arg6, (bool32 *)arg7);
    return !DAQmxFailed(m_lastStatus);
  }

  bool startTask() {
    m_lastStatus = DAQmxFunc(StartTask)(m_tH);
    return !DAQmxFailed(m_lastStatus);
  }

  bool stopTask() {
    m_lastStatus = DAQmxFunc(StopTask)(m_tH);
    return !DAQmxFailed(m_lastStatus);
  }

protected:
  bool loadDlls() {
#ifdef NI_RUNTIME_LINK
    if (m_dllsLoaded) {
      return true;
    }

    if ((m_dllHandle = load_dll(
#ifdef NI_DAQMX_SUPPORT
#ifdef WIN32
				 "nicaiu"
#else
#error NI-DAQmx on Linux does not support USB devices, see http://www.ni.com/white-paper/3695/en#toc2
#endif
#else
#ifdef WIN32
				 "nidaqmxbase"
#else
				 "libnidaqmxbase.so.3"
#endif
#endif
				 )) == NULL ||
	(m_CfgSampClkTiming = (DAQmxCfgSampClkTimingFunc)load_symbol(m_dllHandle, DAQmxFuncStr("CfgSampClkTiming"))) == NULL ||
	(m_ClearTask = (DAQmxClearTaskFunc)load_symbol(m_dllHandle, DAQmxFuncStr("ClearTask"))) == NULL ||
	(m_CreateAIVoltageChan = (DAQmxCreateAIVoltageChanFunc)load_symbol(m_dllHandle, DAQmxFuncStr("CreateAIVoltageChan"))) == NULL ||
	(m_CreateTask = (DAQmxCreateTaskFunc)load_symbol(m_dllHandle, DAQmxFuncStr("CreateTask"))) == NULL ||
	(m_GetDevSerialNum = (DAQmxGetDevSerialNumFunc)load_symbol(m_dllHandle, DAQmxFuncStr("GetDevSerialNum"))) == NULL ||
	(m_GetExtendedErrorInfo = (DAQmxGetExtendedErrorInfoFunc)load_symbol(m_dllHandle, DAQmxFuncStr("GetExtendedErrorInfo"))) == NULL ||
	(m_ReadAnalogF64 = (DAQmxReadAnalogF64Func)load_symbol(m_dllHandle, DAQmxFuncStr("ReadAnalogF64"))) == NULL ||
	(m_StartTask = (DAQmxStartTaskFunc)load_symbol(m_dllHandle, DAQmxFuncStr("StartTask"))) == NULL ||
	(m_StopTask = (DAQmxStopTaskFunc)load_symbol(m_dllHandle, DAQmxFuncStr("StopTask"))) == NULL ||
	false)
      {
	return false;
      }

    // Only present for NI-DAQmx
    m_GetSysDevNames = (DAQmxGetSysDevNamesFunc)load_symbol(m_dllHandle, DAQmxFuncStr("GetSysDevNames"));
#endif

    m_dllsLoaded = true;

    return true;
  }

private:
  bool m_dllsLoaded;
  TaskHandle m_tH;

#ifdef NI_RUNTIME_LINK
  DLLHANDLE m_dllHandle;

  typedef int32 (__CFUNC *DAQmxCfgSampClkTimingFunc) (TaskHandle, const char[], float64, int32, int32, uInt64);
  DAQmxCfgSampClkTimingFunc m_CfgSampClkTiming;
  typedef int32 (__CFUNC *DAQmxClearTaskFunc) (TaskHandle);
  DAQmxClearTaskFunc m_ClearTask;
  typedef int32 (__CFUNC *DAQmxCreateAIVoltageChanFunc) (TaskHandle, const char[], const char[], int32, float64, float64, int32, const char[]);
  DAQmxCreateAIVoltageChanFunc m_CreateAIVoltageChan;
  typedef int32 (__CFUNC *DAQmxCreateTaskFunc) (const char[], TaskHandle *);
  DAQmxCreateTaskFunc m_CreateTask;
  typedef int32 (__CFUNC *DAQmxGetDevSerialNumFunc) (const char[], uInt32 *);
  DAQmxGetDevSerialNumFunc m_GetDevSerialNum;
  typedef int32 (__CFUNC *DAQmxGetExtendedErrorInfoFunc) (char[], uInt32);
  DAQmxGetExtendedErrorInfoFunc m_GetExtendedErrorInfo;
  typedef int32 (__CFUNC *DAQmxGetSysDevNamesFunc) (char *, uInt32);
  DAQmxGetSysDevNamesFunc m_GetSysDevNames;
  typedef int32 (__CFUNC *DAQmxReadAnalogF64Func) (TaskHandle, int32, float64, bool32, float64[], uInt32, int32 *, bool32 *);
  DAQmxReadAnalogF64Func m_ReadAnalogF64;
  typedef int32 (__CFUNC *DAQmxStartTaskFunc) (TaskHandle);
  DAQmxStartTaskFunc m_StartTask;
  typedef int32 (__CFUNC *DAQmxStopTaskFunc) (TaskHandle);
  DAQmxStopTaskFunc m_StopTask;
#endif

  // Intentionally unimplemented
  DAQMX(const DAQMX &);
  DAQMX &operator=(const DAQMX &);
};

static DAQMX daqMx;

DAQmxFuncs * DAQmxFuncs::CAT(get, DAQMX) () {
  return &daqMx;
}
