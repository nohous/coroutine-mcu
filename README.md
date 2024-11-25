# CoroCore MCU OS

## Files

`etl/` - the ETL library
`cmake/toolchain-stm32g7.cmake` - Bare-metal CMake toolchain configuration
`main.cpp` - main experiments source
`syscalls.c` - syscall stubs to make linker stop complaining, stolen from STM32Cube 

## Compilation

Make sure the toolchain is available in your PATH, e.g. with `module load gcc-arm` and module file of:
```shell
#%Module

set install_root /home/nohous/opt/arm-gnu-toolchain-13.3.rel1-x86_64-arm-none-eabi
prepend-path PATH $install_root/bin
prepend-path LD_LIBRARY_PATH $install_root/lib
prepend-path MANPATH $install_root/share/man
```

Now run:
```shell
mkdir build
cd build
cmake ..
```

To compile, run in the `build` directory
```shell
make
```

## VSCode Integration

TBD