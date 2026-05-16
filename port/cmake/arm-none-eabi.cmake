# CMake toolchain file for cross-compiling TinyA2A to Cortex-M.
#
# Usage:
#   mkdir build-cm4 && cd build-cm4
#   cmake -DCMAKE_TOOLCHAIN_FILE=../port/cmake/arm-none-eabi.cmake \
#         -DTA2A_TARGET_CPU=cortex-m4 \
#         -DTA2A_USE_STM32_HAL=ON \
#         ..
#   cmake --build .
#
# Requires:  arm-none-eabi-gcc / arm-none-eabi-ar in PATH.
# Optional:  -DTA2A_TARGET_FPU=fpv4-sp-d16  (FPU spec for the chosen core)
#            -DTA2A_TARGET_FLOAT_ABI=hard   (or "soft", "softfp")
#

set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR ARM)

# Toolchain binaries
set(TOOLCHAIN_PREFIX arm-none-eabi-)
set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_AR           ${TOOLCHAIN_PREFIX}ar)
set(CMAKE_OBJCOPY      ${TOOLCHAIN_PREFIX}objcopy)
set(CMAKE_SIZE         ${TOOLCHAIN_PREFIX}size)

# Don't try to run executables on the host
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Defaults — override at configure time
if(NOT DEFINED TA2A_TARGET_CPU)
    set(TA2A_TARGET_CPU "cortex-m4")
endif()
if(NOT DEFINED TA2A_TARGET_FLOAT_ABI)
    set(TA2A_TARGET_FLOAT_ABI "soft")
endif()

set(_cpu_flags "-mcpu=${TA2A_TARGET_CPU} -mthumb -mfloat-abi=${TA2A_TARGET_FLOAT_ABI}")
if(DEFINED TA2A_TARGET_FPU)
    set(_cpu_flags "${_cpu_flags} -mfpu=${TA2A_TARGET_FPU}")
endif()

set(CMAKE_C_FLAGS_INIT   "${_cpu_flags} -ffunction-sections -fdata-sections")
set(CMAKE_CXX_FLAGS_INIT "${_cpu_flags} -ffunction-sections -fdata-sections")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,--gc-sections")

# Library users typically want USE_HAL_DRIVER on if building under CubeMX
if(TA2A_USE_STM32_HAL)
    add_compile_definitions(USE_HAL_DRIVER=1 TA2A_USE_STM32_HAL=1)
endif()
