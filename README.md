# ETJump

ETJump is a Wolfenstein: Enemy Territory trickjump modification.
 
## Setup Dev-Enviroment

### Required environment variables
* `ETROOT` Path to the ET installation directory.
* `BOOST_ROOT`  Path to the Boost library installation directory.
* `GOOGLE_TEST_ROOT` Path to the Google Test framework installation directory. (optinal, for testing only)

### Setup Visual Studio  

Install Visual Studio 2015 or above (2019 is recommended).  
Make sure you have installed the following workloads in the Visual Studio Installer:

* Desktop development with C++
* Linux development with C++

Make sure you have installed the following component in the Visual Studio Installer:

* MSVC v140 - VS 2015 C++-Buildtools (v14.00)

### Setup Windows Subsystem for Linux _(Win10 only)_

#### Install the Windows Subsystem for Linux

Before installing any Linux distros for WSL, you must ensure that the "Windows Subsystem for Linux" optional feature is enabled:


1. Open PowerShell as Administrator and run:
```powershell   
Enable-WindowsOptionalFeature -Online -FeatureName Microsoft-Windows-Subsystem-Linux
```

2. Restart your computer when prompted.

3. Install Ubuntu 16.04 LTS via Microsoft Store
4. Start Ubuntu 
5. Update packages by running `sudo apt update && sudo apt upgrade`
6. Install dependencies by running `sudo apt install cmake gcc gcc-multilib g++ g++-multilib libboost-all-dev libsqlite3-dev`

## Compiling 

ETJump is compiled on Windows on MSVC toolset v140. Visual C++ Redistributable 
for Visual Studio 2015 has to be installed in order to run the binaries on 
Windows. Redistributable can be downloaded from 
`https://www.microsoft.com/en-us/download/details.aspx?id=48145`. Linux binaries 
are compiled on Ubuntu 16.06 via WSL. 

ETJump currently depends on 3 libraries that must be installed before ETJump can 
be compiled: SQLite3, Boost and Google Test. 

* SQLite3 can be installed on Ubuntu by running the command `sudo apt-get 
install libsqlite3-dev`. On Windows it works out of the box. Once we clean up 
the CMakeLists.txt, it should work out of the box on Ubuntu as well. 
* Boost version 1.60.0 can be downloaded from 
`http://www.boost.org/users/history/version_1_60_0.html`. Only header files are 
used. `BOOST_ROOT` environment variable should point to the Boost directory 
(e.g. `/Path/To/Boost/boost_1_60_0`). 
* Google Test can be downloaded from `https://github.com/google/googletest`. It 
must be compiled and the `gtest.lib` library must be moved to the 
googletest-release-*.*.*/lib directory. `GOOGLE_TEST_ROOT` environment variable 
should point to the googletest-release directory. (optional)

### Compile Linux binaries

```shell
cd /path/to/src
mkdir cmake // or any folder name
cd cmake
sudo cmake .. -DUSE_GCC4=OFF; make
```

## Building pk3 and debugging on Windows

ETJump requires 7zip `http://www.7-zip.org/` for the installation script.

Visual Studio project has a post-build event that will execute the 
build/install.bat to create the pk3 using 7zip.exe. Built pk3 will then be 
copied to $(ETROOT)/etjump directory. Starting the Debug mode will execute 
$(ETROOT)/et.exe and attach to it. 

## Unit testing

ETJump uses Google Test framework for unit testing. Some newer parts of the mod 
have unit tests. New features should have unit tests. More info on 
how to write and run tests can be found at 
`https://github.com/google/googletest`. 

