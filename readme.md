# cli11
User Manual | Api Reference

cli11 is the C++ module fork of CLI11, a command line parser for C++11 and beyond that provides a rich feature set with a simple and intuitive interface.

Project is built using CMake/Ninja and packaged via CPS. CMake 4.4 and later is required.

Build using cmake, and consume via CPS by pointing to `CMAKE_INSTALL_PREFIX` via `CMAKE_PREFIX_PATH`

```cmake
find_package(cli11)
target_link_libraries($PROJECT PRIVATE cli11::cxx_module)
```

