cmake_minimum_required(VERSION 3.20)
project(FTDI2SPI LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add in-tree dependencies (libusb & libftdi) from source
add_subdirectory(extern)

set(FTDI2SPI_SOURCES
    src/Flasher.cpp
    src/Main.cpp
    src/XNAND.cpp
    src/XSPI.cpp
    src/XeMMC.cpp
    src/sfc.cpp
    src/stdafx.cpp
    src/wrapper_spi.cpp
)

set(FTDI2SPI_HEADERS
    include/Flasher.h
    include/XNAND.h
    include/XSPI.h
    include/XeMMC.h
    include/sfc.h
    include/stdafx.h
    include/wrapper_spi.h
)

add_library(FTDI2SPI SHARED ${FTDI2SPI_SOURCES} ${FTDI2SPI_HEADERS})

target_compile_definitions(FTDI2SPI PRIVATE _CRT_SECURE_NO_WARNINGS)

target_include_directories(FTDI2SPI
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/libftdi/src
        ${CMAKE_CURRENT_SOURCE_DIR}/extern/libusb/libusb
        ${CMAKE_CURRENT_BINARY_DIR}/extern/libftdi/src
)

target_link_libraries(FTDI2SPI PRIVATE ftdi1-static)
