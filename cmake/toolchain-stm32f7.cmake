# toolchain-stm32f7.cmake

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR ARM)

# Specify the cross compiler
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-linux-eabi-gcc)
set(CMAKE_AR arm-none-linux-eabi-ar)
set(CMAKE_OBJCOPY arm-none-linux-eabi-objcopy)
set(CMAKE_OBJDUMP arm-none-linux-eabi-objdump)

# STM32F7 specific flags
set(CPU_FLAGS "-mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb")
set(COMMON_FLAGS "${CPU_FLAGS} -ffunction-sections -fdata-sections -fno-common -fmessage-length=0")

set(CMAKE_C_FLAGS "${COMMON_FLAGS}" CACHE INTERNAL "c compiler flags")
set(CMAKE_CXX_FLAGS "${COMMON_FLAGS}" CACHE INTERNAL "cxx compiler flags")
set(CMAKE_ASM_FLAGS "${CPU_FLAGS}" CACHE INTERNAL "asm compiler flags")
set(CMAKE_EXE_LINKER_FLAGS "-specs=nano.specs --specs=nosys.specs  -Wl,--gc-sections")

# Find programs in the host system
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
