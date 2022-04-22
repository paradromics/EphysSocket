# Chunked HTTP Ephys Socket plugin

This plugin follows the template see here and described in the end sections of this doc (copied from template doc)

To setup a build environment on a new host, git clone this code and the main [plugin-GUI](https://github.com/open-ephys/plugin-GUI) project within this directory structure:

```
    code
    ├─ plugin-GUI      
    ├─ OEPlugins
        ├─ EphysSocket
```

For the Windows 10 build host, use Visual Studio 2019 (v2022 is the current version, so v2019 now requires a free developer membership on your MS account).

For both codes, do

```bash
$ cd Build
$ cmake -G "Visual Studio 16 2019" -A x64 ..
```

*Other targets are found in CMAKE_README.txt, but note that the CMakeLists.txt file here points to bundled libcurl dlls for Windows.*

Now you will have the VS project files, including the main .sla solution file. 
Build the plugin-GUI target first (`ALL_BUILD`).
Then build the EphysSocket target. The `INSTALL` target will install the plugin .dll file to the main project build. 
(You still need to copy the debugger symbols by hand if you need them.)


## --- From the original template repo ---
This repository contains a template for building plugins for the [Open Ephys GUI](https://github.com/open-ephys/plugin-GUI). Information on the plugin architecture can be found on [our wiki](https://open-ephys.atlassian.net/wiki/spaces/OEW/pages/950363/Plugin+architecture).

## Creating a new plugin

1. Add source files to the Source folder. The existing files can be used as a template
2. [Edit the OpenEphysLib.cpp file accordingly](https://open-ephys.atlassian.net/wiki/spaces/OEW/pages/46596128/OpenEphysLib+file)
3. [Create the build files through CMake](https://open-ephys.atlassian.net/wiki/spaces/OEW/pages/1259110401/Plugin+CMake+Builds)

## Using external libraries

To link the plugin to external libraries, it is necessary to manually edit the Build/CMakeLists.txt file. The code for linking libraries is located in comments at the end.
For most common libraries, the `find_package` option is recommended. An example would be

```cmake
find_package(ZLIB)
target_link_libraries(${PLUGIN_NAME} ${ZLIB_LIBRARIES})
target_include_directories(${PLUGIN_NAME} PRIVATE ${ZLIB_INCLUDE_DIRS})
```

If there is no standard package finder for cmake, `find_library`and `find_path` can be used to find the library and include files respectively. The commands will search in a variety of standard locations For example

```cmake
find_library(ZMQ_LIBRARIES NAMES libzmq-v120-mt-4_0_4 zmq zmq-v120-mt-4_0_4) #the different names after names are not a list of libraries to include, but a list of possible names the library might have, useful for multiple architectures. find_library will return the first library found that matches any of the names
find_path(ZMQ_INCLUDE_DIRS zmq.h)

target_link_libraries(${PLUGIN_NAME} ${ZMQ_LIBRARIES})
target_include_directories(${PLUGIN_NAME} PRIVATE ${ZMQ_INCLUDE_DIRS})
```

### Providing libraries for Windows

Since Windows does not have standardized paths for libraries, as Linux and macOS do, it is sometimes useful to pack the appropriate Windows version of the required libraries alongside the plugin.
To do so, a _libs_ directory has to be created **at the top level** of the repository, alongside this README file, and files from all required libraries placed there. The required folder structure is:

```
    libs
    ├─ include           #library headers
    ├─ lib
        ├─ x64           #64-bit compile-time (.lib) files
        └─ x86           #32-bit compile time (.lib) files, if needed
    └─ bin
        ├─ x64           #64-bit runtime (.dll) files
        └─ x86           #32-bit runtime (.dll) files, if needed
```

DLLs in the bin directories will be copied to the open-ephys GUI _shared_ folder when installing.
