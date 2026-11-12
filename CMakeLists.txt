
cmake_minimum_required(VERSION 3.25)


# Add cmake-mx modules option
set(CMAKE_MX_MODULES "" CACHE STRING "Path to user cmake modules")
if(NOT CMAKE_MX_MODULES)
    set(CMAKE_MX_MODULES "$ENV{CMAKE_MX_MODULES}")
endif()
if(NOT CMAKE_MX_MODULES)
    set(CMAKE_MX_MODULES_LOCAL "$ENV{HOME}/.local/share/cmake-mx/cmake")
    if(EXISTS "${CMAKE_MX_MODULES_LOCAL}")
        set(CMAKE_MX_MODULES "${CMAKE_MX_MODULES_LOCAL}")
    endif()
endif()
if(NOT CMAKE_MX_MODULES)
    set(CMAKE_MX_MODULES_DEFAULT "/usr/share/cmake-mx/cmake")
    if(EXISTS "${CMAKE_MX_MODULES_DEFAULT}")
        set(CMAKE_MX_MODULES "${CMAKE_MX_MODULES_DEFAULT}")
    endif()
endif()
if(NOT CMAKE_MX_MODULES)
    message(FATAL_ERROR "cmake-mx modules not found")
endif()

# Normalize modules path
cmake_path(CONVERT "${CMAKE_MX_MODULES}" TO_CMAKE_PATH_LIST CMAKE_MX_MODULES)

# Set custom modules path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_MX_MODULES}")


# Project must be defined in main CMakeLists.txt
set(CMAKE_PROJECT_NAME "main" CACHE STRING "Default main project name")
project("${CMAKE_PROJECT_NAME}")

enable_language(C CXX ASM)


if(CMAKE_BUILD_VARIANT MATCHES "release")
    set(CMAKE_BUILD_TYPE  Release)
else()
    set(CMAKE_BUILD_TYPE  Debug)
endif()

include(cmake_utils)
include_conditional(CMakeVariant)
