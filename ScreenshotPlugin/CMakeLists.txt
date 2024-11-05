cmake_minimum_required(VERSION 3.10)
project(ScreenshotPlugin)

set(CMAKE_CXX_STANDARD 11)  # or 11, 14, etc.
find_package(CURL REQUIRED)

# Specify the MinGW 64-bit toolchain if needed
set(CMAKE_CXX_COMPILER "C:/msys64/mingw64/bin/g++.exe")

# Include directories for curl and nlohmann/json
include_directories(C:/msys64/mingw64/include)
include_directories(${CMAKE_SOURCE_DIR}/include)


# Create a shared library (DLL) from plugin.cpp
add_library(plugin SHARED plugin.cpp)

# Link against libcurl
 target_link_libraries(plugin CURL::libcurl)

# Specify the output name for the DLL
set_target_properties(plugin PROPERTIES OUTPUT_NAME "plugin")

# Handle RPATH for DLLs on Windows
if(WIN32)
    set(CMAKE_INSTALL_RPATH "$ORIGIN")
endif()
