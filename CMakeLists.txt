cmake_minimum_required(VERSION 3.20)

#set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/toolchain-stm32f7.cmake)

project(corocore)

set(CMAKE_CXX_STANDARD 20) 
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # Use -std=c++17 instead of -std=gnu++17
add_subdirectory(etl)

add_subdirectory(corocore)
add_executable(corocore_test)
#add_library(syscalls.o OBJECT syscalls.c)
target_include_directories(corocore_test PRIVATE ${PROJECT_SOURCE_DIR})
target_sources(corocore_test PRIVATE 
#    syscalls.c
    main.cpp
)
#target_link_libraries(corocore_test PRIVATE syscalls.o)
target_link_libraries(corocore_test PRIVATE etl::etl)

