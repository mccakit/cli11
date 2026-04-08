Fork of cli11 to named modules. 
Requirements

- C++26
- stdlib with filesystem and stdlib module support
- RTTI
- CMake >= 4.0

Tested with upstream LLVM and latest release LLVM. 
```
conan install . --build=missing --profile=musl -of ./conan --deployer=full_deploy --envs-generation=false
cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=/home/mccakit/dev/toolchains/static/cmake/x86_64-unknown-linux-musl.cmake
```
