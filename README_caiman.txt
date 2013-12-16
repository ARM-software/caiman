*** Purpose ***
Instructions on setting up the ARM Energy Probe or National Instruments DAQ and running caiman.

*** ARM Energy Probe ***
LEDs
- No LED: The firmware needs to be installed
- Flashing Red LED: The Energy Probe is not yet recognized by the OS
- Green LED: Everything is OK
- Orange LED: One or more of the probes are not correctly connected, i.e. the positive and negative leads are reversed

Caiman can usually auto-detect the Energy Probe device. But on older versions of Linux which do not have libudev.so.0, such as Red Hat, it cannot be auto-detected. In that case the device name will need to be provided, usually '/dev/ttyACM0'.

*** NI-DAQ ***
NI-DAQmx or NI-DAQmx Base drivers from National Instruments must be installed for caiman to communicate with the DAQ

Wiring
- Connect the AI1 connections on the NI-DAQ device to go across the shunt resistor on your target
- Connect AI0- to ground
- Loop AI1- to AI0+
The above can be repeated for further parings ex: AI2 and AI3.

Auto-detect is not available with the NI-DAQmx Base drivers, so the device name (usually 'Dev1') must be supplied and can be determined from the National Instrument's List Devices utility. Also, with the Ni-DAQmx Base drivers, it takes a while to initialize the NI-DAQ, so power data for the first 3-8 seconds will not be captured.

As NI only distributes 32 bit versions of their libraries, only the 32 bit version of caiman will work with the DAQ, even on 64 bit platforms. A Windows 64-bit install of DS-5 will contain a 32 bit version of caiman.

A NI-DAQ enabled version of caiman must be built from source on Linux. To build a NI-DAQ enabled version of caiman on Linux, edit CMakeLists.txt and set SUPPORT_DAQ to 1, set NI_RUNTIME_LINK to 0 and verify the NI-DAQ install paths within CMakeLists.txt.

*** Debugging ***
If you're having problems running caiman from Streamline, run it on the command line with the additional arguments -p 0, ex: '/usr/local/DS-5/bin/caiman -l -r 0:20'. You may see additional information to assist with debugging or if no messages are printed after a few seconds you can kill caiman. If everything is OK the 0000000000 file will be non-empty.

*** Building ***
Streamline is distributed with a pre-built caiman. But if you want to change some options or the pre-built caiman is insufficient, caiman can be built from source. Caiman uses CMake (http://www.cmake.org) so that both Visual Studio and Makefiles can be generated from the same configuration. After extracting the source, open CMakeLists.txt and modify the settings at the top as desired and, if necessary, modify include_directories and target_link_libraries to add other dependencies, like NI-DAQ. After the CMakeLists.txt file is customized, use CMake to generate either a Makefile or a Visual Studio project, then the project can be built normally.

*** Troubleshooting ***
-Unable to detect the energy probe
    Disconnect and reconnect the energy probe to resolve the issue and ensure the energy probe properly enumerates with the OS. This can occur on some operating systems that do not properly re-enumerate the energy probe device after rebooting or going into sleep/hibernate.

-Unable to set /dev/ttyACM0 to raw mode, please verify the device exists
    Check the permissions of /dev/ttyACM0. If you still have problems, a different program may be using the device; use 'lsof' to debug further. On Ubuntu after plugging in the Energy Probe, the modem-manager opens the device for a while.

-bash: ./caiman: No such file or directory
    Ensure /lib/ld-linux.so.2 is installed (it's in the libc6-i386 package on Ubuntu). This can occur when running the 32 bit version of caiman on x86_64.

-The power values in Streamline look incorrect
    Make sure the shunt resistor values are correct in the energy capture options.

-Missing channels
    Make sure either power, voltage, or current is checked in the energy capture options.

-Data is reporting zero or close to zero values
    Check the connections. If you're using the Energy Probe, make sure the green LED is on.
