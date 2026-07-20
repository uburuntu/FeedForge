set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR s390x)

set(CMAKE_CXX_COMPILER s390x-linux-gnu-g++)

# Debian and Ubuntu cross packages place the target dynamic loader and runtime
# under this prefix. CTest prepends the emulator to executable test targets.
set(
  CMAKE_CROSSCOMPILING_EMULATOR
  qemu-s390x;-L;/usr/s390x-linux-gnu
  CACHE STRING
  "Run s390x test executables with QEMU user-mode emulation"
)

# Project feature probes need compilation, not execution, during configure.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_FIND_ROOT_PATH /usr/s390x-linux-gnu)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
