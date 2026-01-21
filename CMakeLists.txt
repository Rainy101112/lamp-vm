cmake_minimum_required(VERSION 3.20)
project(vm C)

set(CMAKE_C_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(SOURCES
        register_vm.c
        io_devices/frame/frame.c
        io.h
        io.c
        loadbin.c
        loadbin.h
        panic.c
        panic.h
        io_devices/disk/disk.c
        io_devices/disk/disk.h
        interrupt.c
        interrupt.h
        io_devices/frame/terminalin.c
        memory.h
        memory.c
        io_devices/vga_display/display.c
        io_devices/vga_display/display.h
        mmio.h
        mmio.c
        io_devices/vga_display/vga_mmio_register.c
        io_devices/vga_display/vga_mmio_register.h
        io_devices/time/time_mmio_register.c
        io_devices/time/time_mmio_register.h
        stack.h
        fetch.h
        instruction.h
        vm.h
)

add_executable(vm ${SOURCES})

find_package(SDL2 REQUIRED)
if(SDL2_FOUND)
    target_include_directories(vm PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(vm PRIVATE ${SDL2_LIBRARIES})
else()
    message(FATAL_ERROR "SDL2 not found!")
endif()

if(MSVC)
    target_compile_options(vm PRIVATE /W4 /WX)
else()
    target_compile_options(vm PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(vm PRIVATE RELEASE_BUILD)
    if(NOT MSVC)
        target_compile_options(vm PRIVATE -O3 -flto)
        target_link_options(vm PRIVATE -flto)
    endif()
else()
    target_compile_definitions(vm PRIVATE DEBUG_BUILD)
    if(NOT MSVC)
        target_compile_options(vm PRIVATE -g -O0)
    endif()
endif()

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "" FORCE)
endif()
