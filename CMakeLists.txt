cmake_minimum_required(VERSION 3.20)
project(FTDI2SPI LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The vendor D2XX binaries are Windows-only. Other targets use native libftdi.
if(WIN32)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
        set(FTD2XX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/ftd2xx-winarm")
        set(FTD2XX_LIB "${FTD2XX_DIR}/arm64/FTD2XX.lib")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(FTD2XX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/ftd2xx-win64")
        set(FTD2XX_LIB "${FTD2XX_DIR}/amd64/ftd2xx.lib")
    else()
        # x86 (Win32 / 32-bit)
        set(FTD2XX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/ftd2xx-win32")
        set(FTD2XX_LIB "${FTD2XX_DIR}/i386/ftd2xx.lib")
    endif()
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBFTDI REQUIRED IMPORTED_TARGET libftdi1)
    find_package(Threads REQUIRED)
endif()

set(SOURCES
    src/Flasher.cpp
    src/ISD2100.cpp
    src/Main.cpp
    src/XNAND.cpp
    src/XSPI.cpp
    src/XeMMC.cpp
    src/sfc.cpp
    src/stdafx.cpp
    src/wrapper_spi.cpp
)

set(HEADERS
    include/Flasher.h
    include/ISD2100.h
    include/XNAND.h
    include/XSPI.h
    include/XeMMC.h
    include/sfc.h
    include/stdafx.h
    include/wrapper_spi.h
    include/isd_transport.h
)

add_library(FTDI2SPI SHARED ${SOURCES} ${HEADERS})

target_include_directories(FTDI2SPI PRIVATE include)

if(WIN32)
    target_compile_definitions(FTDI2SPI PRIVATE _CRT_SECURE_NO_WARNINGS _MBCS)
    target_include_directories(FTDI2SPI PRIVATE "${FTD2XX_DIR}")
    target_link_libraries(FTDI2SPI PRIVATE "${FTD2XX_LIB}")
else()
    target_compile_definitions(FTDI2SPI PRIVATE LIBFTDI)
    target_link_libraries(FTDI2SPI PRIVATE PkgConfig::LIBFTDI Threads::Threads)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(FTDI2SPI PRIVATE -Wno-c++11-narrowing)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(FTDI2SPI PRIVATE -Wno-narrowing)
endif()
