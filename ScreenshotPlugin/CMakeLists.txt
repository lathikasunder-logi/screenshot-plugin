cmake_minimum_required(VERSION 3.0)
project(curllib_Test)

# Find libcurl package
find_package(CURL REQUIRED)

# Add the path to the nlohmann json library
include_directories(C:/json/single_include)  # Update with the correct path to nlohmann/json.hpp

# Create a shared library (DLL) instead of an executable
add_library(plugin SHARED plugin.cpp)

# Link libcurl to the shared library (DLL)
target_link_libraries(plugin CURL::libcurl)

# Set output name for the DLL if you want to customize it
set_target_properties(plugin PROPERTIES OUTPUT_NAME "plugin")
