cmake_minimum_required(VERSION 4.4)
project(
    cli
    VERSION 2.6.2
    DESCRIPTION
        "Command line parser that provides a rich feature set with a simple and intuitive interface."
    LANGUAGES C CXX)
set(CMAKE_EXPERIMENTAL_FIND_CPS_PACKAGES ON)
set(CMAKE_CXX_MODULE_STD ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_EXTENSIONS ON)
include(GNUInstallDirs)
include(options.cmake)
add_subdirectory(src)
if(BUILD_TESTS)
    include(CTest)
    add_subdirectory(tests)
endif()
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
