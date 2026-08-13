cmake_minimum_required(VERSION 3.20)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Build libusb static library from source
add_subdirectory(libusb)

# Provide variables so libftdi uses the in-tree libusb
set(LIBUSB_FOUND TRUE CACHE BOOL "" FORCE)
set(LIBUSB_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libusb/libusb" CACHE PATH "" FORCE)
set(LIBUSB_LIBRARIES usb-1.0 CACHE STRING "" FORCE)

# Configure libftdi options to build libftdi static/shared library only
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(DOCUMENTATION OFF CACHE BOOL "" FORCE)
set(EXAMPLES OFF CACHE BOOL "" FORCE)
set(FTDIPP OFF CACHE BOOL "" FORCE)
set(FTDI_EEPROM OFF CACHE BOOL "" FORCE)
set(PYTHON_BINDINGS OFF CACHE BOOL "" FORCE)

# Build libftdi from source
add_subdirectory(libftdi)
