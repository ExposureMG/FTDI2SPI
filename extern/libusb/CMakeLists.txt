cmake_minimum_required(VERSION 3.20)
project(libusb C)

set(LIBUSB_SOURCES
    libusb/core.c
    libusb/descriptor.c
    libusb/hotplug.c
    libusb/io.c
    libusb/strerror.c
    libusb/sync.c
)

if(WIN32)
    list(APPEND LIBUSB_SOURCES
        libusb/os/events_windows.c
        libusb/os/threads_windows.c
        libusb/os/windows_common.c
        libusb/os/windows_winusb.c
    )
    add_library(usb-1.0 STATIC ${LIBUSB_SOURCES})
    target_link_libraries(usb-1.0 PRIVATE cfgmgr32)
else()
    list(APPEND LIBUSB_SOURCES
        libusb/os/events_posix.c
        libusb/os/threads_posix.c
        libusb/os/linux_usbfs.c
        libusb/os/linux_netlink.c
    )
    add_library(usb-1.0 STATIC ${LIBUSB_SOURCES})
    find_package(Threads REQUIRED)
    target_link_libraries(usb-1.0 PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
endif()

set_target_properties(usb-1.0 PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(usb-1.0
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/libusb
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
