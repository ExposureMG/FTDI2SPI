#pragma once

// The subset of D2XX used by the legacy ISD2100 implementation. Keep its
// command generation independent of the host's driver API.
#ifdef _WIN32
#include "ftd2xx.h"
#else
#include <ftdi.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <thread>

using BYTE = std::uint8_t;
using DWORD = std::uint32_t;
using FT_HANDLE = ftdi_context*;
using FT_STATUS = int;
constexpr FT_STATUS FT_OK = 0;
constexpr DWORD FT_PURGE_RX = 1, FT_PURGE_TX = 2;

inline FT_STATUS FT_Write(FT_HANDLE handle, const void* data, DWORD size, DWORD* written) {
    *written = 0;
    if (!handle) return -1;
    const auto* bytes = static_cast<const unsigned char*>(data);
    while (*written < size) {
        const auto count = std::min<DWORD>(size - *written, 16384);
        const int n = ftdi_write_data(handle, bytes + *written, count);
        if (n <= 0 || static_cast<DWORD>(n) > count) return -1;
        *written += static_cast<DWORD>(n);
    }
    return FT_OK;
}

inline FT_STATUS FT_Read(FT_HANDLE handle, void* data, DWORD size, DWORD* received) {
    *received = 0;
    if (!handle) return -1;
    const int savedTimeout = handle->usb_read_timeout;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(savedTimeout);
    FT_STATUS status = FT_OK;
    while (*received < size) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) { status = -1; break; }
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
        handle->usb_read_timeout = static_cast<int>(std::max<long long>(1, ms));
        const auto count = std::min<DWORD>(size - *received, 65536);
        const int n = ftdi_read_data(handle, static_cast<unsigned char*>(data) + *received, count);
        if (n < 0 || static_cast<DWORD>(n) > count) { status = -1; break; }
        *received += static_cast<DWORD>(n);
        if (n == 0) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    handle->usb_read_timeout = savedTimeout;
    return status;
}
inline FT_STATUS FT_SetBitMode(FT_HANDLE h, BYTE mask, BYTE mode) {
    return h ? ftdi_set_bitmode(h, mask, mode) : -1;
}
inline FT_STATUS FT_Purge(FT_HANDLE h, DWORD flags) {
    if (!h) return -1;
    if ((flags & FT_PURGE_RX) && ftdi_usb_purge_rx_buffer(h) < 0) return -1;
    if ((flags & FT_PURGE_TX) && ftdi_usb_purge_tx_buffer(h) < 0) return -1;
    return FT_OK;
}
inline FT_STATUS FT_SetUSBParameters(FT_HANDLE h, DWORD inSize, DWORD outSize) {
    if (!h || ftdi_read_data_set_chunksize(h, inSize) < 0) return -1;
    return ftdi_write_data_set_chunksize(h, outSize);
}
inline FT_STATUS FT_SetLatencyTimer(FT_HANDLE h, BYTE latency) {
    return h ? ftdi_set_latency_timer(h, latency) : -1;
}
inline FT_STATUS FT_Close(FT_HANDLE h) {
    if (!h) return FT_OK;
    const int status = ftdi_usb_close(h);
    ftdi_free(h);
    return status;
}

inline FT_HANDLE ISD_OpenByDescription(const char* description) {
    if (!description) return nullptr;
    std::string product(description);
    // D2XX appends the interface letter to the USB product string; libftdi
    // takes the interface separately and matches the original product string.
    ftdi_interface channel = INTERFACE_A;
    if (product.size() >= 2 && product[product.size() - 2] == ' ' &&
        product.back() >= 'A' && product.back() <= 'D') {
        channel = static_cast<ftdi_interface>(INTERFACE_A + product.back() - 'A');
        product.resize(product.size() - 2);
    }
    auto* handle = ftdi_new();
    if (!handle) return nullptr;
    if (ftdi_set_interface(handle, channel) < 0 ||
        ftdi_usb_open_desc(handle, 0x0403, 0x6010, product.c_str(), nullptr) < 0) {
        ftdi_free(handle);
        return nullptr;
    }
    return handle;
}
#endif
