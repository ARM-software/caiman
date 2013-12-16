# Copyright: 2010-2013 ARM Ltd
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 2.6)
project(energy_probe)

include(pb_config)

# Enable this to support auto-detection of the ARM Energy Probe on Linux. Requires udev support. If your Linux platform does not support udev, such as Red Hat 5, disable this option.
set(SUPPORT_UDEV 1)
# Enable this to support National Instruments DAQs
# Currently this is set to only enable NI-DAQ on Windows
set(SUPPORT_DAQ ${PB_TARGETING_WINDOWS})
# Enable this to use dlopen/LoadLibrary to load NI-DAQ API so that the so/dlls are not required. Disable this option if runtime errors occur when using a NI-DAQ
set(NI_RUNTIME_LINK 1)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_BINARY_DIR})
include_directories(${PROJECT_SOURCE_DIR})

set(CMAKE_BUILD_TYPE "Release")

set(src
  ./DAQmx.cpp
  ./DAQmxBase.cpp
  ./DAQmxFuncs.cpp
  ./Dll.cpp
  ./EnergyProbe.cpp
  ./Fifo.cpp
  ./main.cpp
  ./NiDaq.cpp
  ./Devices.cpp
  ./Logging.cpp
  ./OlySocket.cpp
  ./OlyUtility.cpp
  ./SessionData.cpp
)

set_source_files_properties(${src} PROPERTIES LANGUAGE CXX)
if (${PB_TARGETING_UNIX})
  add_definitions("-pthread")
  if ($(CMAKE_HOST_APPLE))
    add_definitions("-DDARWIN")
  else() # linux
    if (${SUPPORT_UDEV})
      add_definitions("-DSUPPORT_UDEV")
      include_directories(${PROJECT_SOURCE_DIR}/udev-181/src)
    endif() # udev
  endif() # linux
else() # windows
  add_definitions("-D_CRT_SECURE_NO_WARNINGS -D_CRT_DISABLE_PERFCRIT_LOCKS")
endif()

add_executable(caiman
  ${src}
)

if (${SUPPORT_DAQ})
  add_definitions("-DSUPPORT_DAQ")
endif()

# Always build 32-bit on Linux
if (${PB_TARGETING_UNIX})
  add_definitions("-m32")
  set_target_properties(caiman PROPERTIES LINK_FLAGS -m32)
endif()

if (${NI_RUNTIME_LINK})
  add_definitions("-DNI_RUNTIME_LINK")
else()
  # Modify these includes if necessary when building caiman with NI-DAQ support but without runtime linking
  if (${PB_TARGETING_WINDOWS})
    include_directories(
      "C:\Program Files\National Instruments\NI-DAQ\DAQmx ANSI C Dev\include"
      "C:\Program Files\National Instruments\NI-DAQmx Base\Include"
    )
    set(NIDAQ_LIB
      "C:\Program Files\National Instruments\NI-DAQ\DAQmx ANSI C Dev\lib\msvc\NIDAQmx.lib"
      "C:\Program Files\National Instruments\NI-DAQmx Base\Lib\nidaqmxbase.lib"
    )
  endif()
  if (${PB_TARGETING_UNIX})
    include_directories(/usr/local/natinst/nidaqmxbase/include)
    set(NIDAQ_LIB /usr/local/natinst/nidaqmxbase/lib/libnidaqmxbase.so.3.7.0)
  endif()
endif()

if(${PB_TARGETING_WINDOWS})
  target_link_libraries(caiman
    ${PB_WH_LIBS}
    ${PB_SYSLIBS_OVERRIDES}
    ${NIDAQ_LIB}
    ws2_32.lib
    Setupapi.lib
  )
else()
  target_link_libraries(caiman
    ${PB_WH_LIBS}
    ${PB_SYSLIBS_OVERRIDES}
    ${NIDAQ_LIB}
    dl
    pthread
  )
endif()

set_target_properties(caiman PROPERTIES
  SKIP_BUILD_RPATH true
)
