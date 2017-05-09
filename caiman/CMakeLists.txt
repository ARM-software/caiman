# Copyright (C) ARM Limited 2010-2016. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 2.6)
project(energy_probe)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_BINARY_DIR})
include(pb_config)
include_directories(${PROJECT_SOURCE_DIR})

if (NOT SUPPORT_UDEV)
    set(SUPPORT_UDEV 1 CACHE STRING
        "Enable this to support auto-detection of the ARM Energy Probe on Linux. Requires udev support. If your Linux platform does not support udev, such as Red Hat 5, disable this option.")
endif(NOT SUPPORT_UDEV)
if (NOT SUPPORT_DAQ)
    set(SUPPORT_DAQ ${PB_TARGETING_WINDOWS} CACHE STRING
        "Enable this to support National Instruments DAQs. By default NI-DAQ is only set on Windows")
endif(NOT SUPPORT_DAQ)
if (NOT NI_RUNTIME_LINK)
    set(NI_RUNTIME_LINK 1 CACHE STRING
        "Enable this to use dlopen/LoadLibrary to load NI-DAQ API so that the so/dlls are not required. Disable this option if runtime errors occur when using a NI-DAQ")
endif(NOT NI_RUNTIME_LINK)

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
    ./c++.cpp
)

set_source_files_properties(${src} PROPERTIES LANGUAGE CXX)
if (${PB_TARGETING_UNIX})
    add_definitions("-pthread")
    add_definitions("-Wall -Wextra -Wshadow -fno-exceptions -fno-rtti")
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
    if (MSVC)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /FS")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /FS")
    endif()
endif()

add_executable(caiman
    ${src}
)

if (${SUPPORT_DAQ})
    add_definitions("-DSUPPORT_DAQ")
    # DAQ only works in 32-bit mode on Linux
    if (${PB_TARGETING_UNIX})
        add_definitions("-m32")
        set_target_properties(caiman PROPERTIES LINK_FLAGS -m32)
    endif()

    if (${PB_TARGETING_WINDOWS})
        include_directories(
            "C:/Program Files/National Instruments/NI-DAQ/DAQmx ANSI C Dev/include"
            "C:/Program Files/National Instruments/NI-DAQmx Base/Include"
            "C:/Program Files (x86)/National Instruments/NI-DAQ/DAQmx ANSI C Dev/include"
            "C:/Program Files (x86)/National Instruments/NI-DAQmx Base/Include"
        )
    endif()
    if (${PB_TARGETING_UNIX})
        include_directories(/usr/local/natinst/nidaqmxbase/include)
    endif()

    # Modify these includes if necessary when building caiman with NI-DAQ support but without runtime linking
    if (${NI_RUNTIME_LINK})
        add_definitions("-DNI_RUNTIME_LINK")
    else()
        if (${PB_TARGETING_WINDOWS})
            find_library(
                NIDAQ_LIB
                NIDAQmx.lib
                "C:/Program Files/National Instruments/NI-DAQ/DAQmx ANSI C Dev/lib/msvc"
                "C:/Program Files (x86)/National Instruments/NI-DAQ/DAQmx ANSI C Dev/lib/msvc"
            )
            find_library(
                NIDAQBASE_LIB
                nidaqmxbase.lib
                "C:/Program Files/National Instruments/NI-DAQmx Base/Lib"
                "C:/Program Files (x86)/National Instruments/NI-DAQmx Base/Lib"
            )
        endif()
        if (${PB_TARGETING_UNIX})
            set(NIDAQ_LIB nidaqmxbase)
        endif()
    endif()
endif()


if(${PB_TARGETING_WINDOWS})
    target_link_libraries(caiman
        ${PB_WH_LIBS}
        ${PB_SYSLIBS_OVERRIDES}
        ${NIDAQ_LIB}
        ${NIDAQBASE_LIB}
        ws2_32.lib
        setupapi.lib
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
