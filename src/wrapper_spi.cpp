#include "stdafx.h"
#include "wrapper_spi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <chrono>
#include <string>

#ifdef LIBFTDI
#include <ftdi.h> // libftdi1 link with -lftdi1
#ifndef FALSE
#define FALSE false
#endif
#ifndef TRUE
#define TRUE true
#endif
#endif

#define MAX_NUM_BYTES_USB_WRITE 16384

#define MAX_READ_DATA_WORDS_BUFFER_SIZE 65536 // 64k bytes
#define MAX_FREQ_CLOCK_DIVISOR 0

#define CHIP_SELECT_PIN '\x08'

const BYTE SET_LOW_BYTE_DATA_BITS_CMD = '\x80';
const BYTE SET_HIGH_BYTE_DATA_BITS_CMD = '\x82';
const BYTE SEND_ANSWER_BACK_IMMEDIATELY_CMD = '\x87';

const BYTE CLK_DATA_BYTES_OUT_ON_NEG_CLK_LSB_FIRST_CMD = '\x19';
const BYTE CLK_DATA_BITS_OUT_ON_NEG_CLK_LSB_FIRST_CMD = '\x1B';
const BYTE CLK_DATA_BYTES_IN_ON_NEG_CLK_LSB_FIRST_CMD = '\x2D';
const BYTE CLK_DATA_BITS_IN_ON_NEG_CLK_LSB_FIRST_CMD = '\x2F';

typedef WORD ReadDataWordBuffer[MAX_READ_DATA_WORDS_BUFFER_SIZE];
typedef ReadDataWordBuffer *PReadDataWordBuffer;

#ifdef LIBFTDI
static struct ftdi_context *ftdi = NULL;
#else
FT_HANDLE ftHandle = NULL;
#endif

BYTE byOutputBuffer[OUTPUT_BUFFER_SIZE];
BYTE dwLowPinsValue = 0;
DWORD dwNumBytesToSend = 0; // Index to the output buffer
DWORD dwNumBytesSent = 0;   // Count of actual bytes sent - used with FT_Write
DWORD dwNumBytesToRead = 0; // Number of bytes available to read

namespace {
constexpr unsigned int TRANSFER_TIMEOUT_MS = 5000;
using Clock = std::chrono::steady_clock;

[[noreturn]] void TransferFailure(const char* operation) {
  std::string message(operation);
#ifdef LIBFTDI
  if (ftdi) {
    const char* detail = ftdi_get_error_string(ftdi);
    if (detail && *detail) message += std::string(": ") + detail;
  }
#endif
  // A failed transfer may have reached the device partially. Close it rather
  // than permit a retry of an ambiguous MPSSE stream.
  closeDevice();
  throw SpiTransportError(message);
}

void Check(bool success, const char* operation) {
  if (!success) TransferFailure(operation);
}

void RequireDevice() {
#ifdef LIBFTDI
  Check(ftdi != nullptr, "FTDI device is not open");
#else
  Check(ftHandle != nullptr, "FTDI device is not open");
#endif
}

void SetTimeout(Clock::time_point deadline) {
  const auto now = Clock::now();
  Check(now < deadline, "FTDI transfer timed out");
  const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
  const unsigned int timeout = static_cast<unsigned int>(std::max<long long>(1, remaining));
#ifdef LIBFTDI
  ftdi->usb_read_timeout = timeout;
  ftdi->usb_write_timeout = timeout;
#else
  Check(FT_SetTimeouts(ftHandle, timeout, timeout) == FT_OK, "FT_SetTimeouts failed");
#endif
}

void WriteAll(const unsigned char* data, unsigned int size) {
  if (size == 0) return;
  RequireDevice();
  const auto deadline = Clock::now() + std::chrono::milliseconds(TRANSFER_TIMEOUT_MS);
  unsigned int offset = 0;
  while (offset < size) {
    SetTimeout(deadline);
    const unsigned int chunk = std::min<unsigned int>(size - offset, MAX_NUM_BYTES_USB_WRITE);
#ifdef LIBFTDI
    const int sent = ftdi_write_data(ftdi, data + offset, chunk);
    Check(sent >= 0, "ftdi_write_data failed");
#else
    DWORD sent = 0;
    Check(FT_Write(ftHandle, const_cast<unsigned char*>(data + offset), chunk, &sent) == FT_OK,
          "FT_Write failed");
#endif
    Check(sent > 0 && static_cast<unsigned int>(sent) <= chunk, "FTDI write made no progress or returned an invalid count");
    offset += static_cast<unsigned int>(sent);
  }
}
} // namespace

bool spi_init(void) {
  closeDevice();
  try {
#ifdef LIBFTDI
    ftdi = ftdi_new();
    if (!ftdi) return false;
    Check(ftdi_set_interface(ftdi, INTERFACE_B) == 0, "ftdi_set_interface failed");
    Check(ftdi_usb_open(ftdi, 0x0403, 0x6010) == 0, "ftdi_usb_open failed");
    ftdi->usb_read_timeout = TRANSFER_TIMEOUT_MS;
    ftdi->usb_write_timeout = TRANSFER_TIMEOUT_MS;
    Check(ftdi_set_bitmode(ftdi, 0x00, BITMODE_RESET) == 0, "FTDI reset failed");
    Sleep(50);
    Check(ftdi_set_bitmode(ftdi, 0x00, BITMODE_MPSSE) == 0, "FTDI MPSSE mode failed");
    Sleep(50);
    Check(ftdi_usb_purge_buffers(ftdi) == 0, "FTDI purge failed");
    Check(ftdi_read_data_set_chunksize(ftdi, 64 * 1024) == 0, "FTDI read chunk size failed");
    Check(ftdi_write_data_set_chunksize(ftdi, 64 * 1024) == 0, "FTDI write chunk size failed");
    Check(ftdi_set_latency_timer(ftdi, 1) == 0, "FTDI latency timer failed");
#else
    DWORD numDevs = 0;
    char targetDesc[64] = {};
    if (FT_CreateDeviceInfoList(&numDevs) != FT_OK || numDevs == 0) return false;
    auto* devInfo = static_cast<FT_DEVICE_LIST_INFO_NODE*>(malloc(sizeof(FT_DEVICE_LIST_INFO_NODE) * numDevs));
    if (!devInfo) return false;
    bool found = false;
    if (FT_GetDeviceInfoList(devInfo, &numDevs) == FT_OK) {
      for (DWORD i = 0; i < numDevs; ++i) {
        const char* desc = devInfo[i].Description;
        const size_t len = strlen(desc);
        if (len >= 2 && desc[len - 2] == ' ' && desc[len - 1] == 'B') {
          found = strncpy_s(targetDesc, sizeof(targetDesc), desc, _TRUNCATE) == 0;
          break;
        }
      }
    }
    free(devInfo);
    if (!found) return false;
    Check(FT_OpenEx(targetDesc, FT_OPEN_BY_DESCRIPTION, &ftHandle) == FT_OK, "FT_OpenEx failed");
    Check(FT_SetTimeouts(ftHandle, TRANSFER_TIMEOUT_MS, TRANSFER_TIMEOUT_MS) == FT_OK, "FT_SetTimeouts failed");
    Check(FT_SetBitMode(ftHandle, 0x00, 0x00) == FT_OK, "FTDI reset failed");
    Sleep(50);
    Check(FT_SetBitMode(ftHandle, 0x00, 0x02) == FT_OK, "FTDI MPSSE mode failed");
    Sleep(50);
    Check(FT_Purge(ftHandle, FT_PURGE_RX | FT_PURGE_TX) == FT_OK, "FTDI purge failed");
    Check(FT_SetUSBParameters(ftHandle, 64 * 1024, 64 * 1024) == FT_OK, "FTDI USB parameters failed");
    Check(FT_SetLatencyTimer(ftHandle, 1) == FT_OK, "FTDI latency timer failed");
#endif
    BYTE cmd[3];
    // Disable loopback (MPSSE command 0x85)
    cmd[0] = 0x85;
    WriteAll(cmd, 1);

    // Enable divide-by-5 clocking (mirrors
    // SPI_TurnOnDivideByFiveClockingHiSpeedDevice) 60 MHz / 5 / ((divisor + 1) *
    // 2) = 6 MHz at divisor=0
    cmd[0] = 0x8B;
    WriteAll(cmd, 1);

    // Set clock divisor = MAX_FREQ_CLOCK_DIVISOR (0)
    cmd[0] = 0x86;
    cmd[1] = (MAX_FREQ_CLOCK_DIVISOR) & 0xFF;
    cmd[2] = ((MAX_FREQ_CLOCK_DIVISOR) >> 8) & 0xFF;
    WriteAll(cmd, 3);

    // Disable divide-by-5 clocking (mirrors
    // SPI_TurnOffDivideByFiveClockingHiSpeedDevice) 60 MHz / ((divisor + 1) * 2)
    // = 30 MHz at divisor=0
    cmd[0] = 0x8A;
    WriteAll(cmd, 1);

    // Set clock divisor again with divide-by-5 off
    cmd[0] = 0x86;
    cmd[1] = (MAX_FREQ_CLOCK_DIVISOR) & 0xFF;
    cmd[2] = ((MAX_FREQ_CLOCK_DIVISOR) >> 8) & 0xFF;
    WriteAll(cmd, 3);

    // Configure low byte (ADBUS) pins:
    //   ADBUS0 = SK  (output)
    //   ADBUS1 = DO  (output)
    //   ADBUS2 = DI  (input)
    //   ADBUS3 = CS  (output, deasserted high)
    //   ADBUS4-7 = GPIOL1-4 (output, low)
    // Direction byte: 0xFB = 1111_1011 (all output except DI/ADBUS2)
    dwLowPinsValue = 0x08; // CS deasserted (high), SK/DO/GPIOL1-4 low
    cmd[0] = SET_LOW_BYTE_DATA_BITS_CMD; // 0x80
    cmd[1] = dwLowPinsValue;
    cmd[2] = 0xFB;
    WriteAll(cmd, 3);

    // Configure high byte (ACBUS) pins:
    //   ACBUS0 (Pin1) = output, low
    //   ACBUS1 (Pin2) = output, high
    //   ACBUS2-7    = input, low
    // Direction byte: 0x03 (ACBUS0+ACBUS1 output), value: 0x02 (ACBUS1 high)
    cmd[0] = SET_HIGH_BYTE_DATA_BITS_CMD; // 0x82
    cmd[1] = 0x02;
    cmd[2] = 0x03;
    WriteAll(cmd, 3);

    return true;
  } catch (const SpiTransportError&) {
    // TransferFailure has already released the device and pending commands.
    return false;
  }
}

void SendBytesToDevice(void) {
  WriteAll(byOutputBuffer, dwNumBytesToSend);
  dwNumBytesToSend = 0;
}

void ClearOutputBuffer(void) { dwNumBytesToSend = 0; }

void AddByteToOutputBuffer(BYTE DataByte, bool bClearOutputBuffer) {
  if (bClearOutputBuffer)
    dwNumBytesToSend = 0;

  // Guard the (previously unchecked) staging buffer. With a correctly sized
  // OUTPUT_BUFFER_SIZE this never trips; it turns a batch sized too large for
  // the buffer into a dropped byte instead of a heap overrun.
  if (dwNumBytesToSend < OUTPUT_BUFFER_SIZE)
    byOutputBuffer[dwNumBytesToSend++] = DataByte;
}

void SetAnswerFast(void) {
  AddByteToOutputBuffer(SEND_ANSWER_BACK_IMMEDIATELY_CMD, false);
}

void GetDataFromDevice(unsigned int size, unsigned char data[]) {
  if (size == 0) return;
  RequireDevice();
  Check(data != nullptr, "FTDI read buffer is null");
  const auto deadline = Clock::now() + std::chrono::milliseconds(TRANSFER_TIMEOUT_MS);
  unsigned int offset = 0;
  while (offset < size) {
    SetTimeout(deadline);
    const unsigned int chunk = std::min<unsigned int>(size - offset, 64 * 1024);
#ifdef LIBFTDI
    const int received = ftdi_read_data(ftdi, data + offset, chunk);
    Check(received >= 0, "ftdi_read_data failed");
#else
    DWORD received = 0;
    Check(FT_Read(ftHandle, data + offset, chunk, &received) == FT_OK, "FT_Read failed");
#endif
    Check(static_cast<unsigned int>(received) <= chunk, "FTDI read returned an invalid count");
    offset += static_cast<unsigned int>(received);
    if (received == 0) Sleep(1);
  }
}

void DisableSPIChip(void) {
  AddByteToOutputBuffer(SET_LOW_BYTE_DATA_BITS_CMD, false);
  dwLowPinsValue = (dwLowPinsValue | CHIP_SELECT_PIN); // set CS to high
  // set SK, DO, CS and GPIOL1-4 as output, set D1 as input
  AddByteToOutputBuffer(dwLowPinsValue, FALSE);
  AddByteToOutputBuffer('\xFB', false);
}

void EnableSPIChip(void) {
  AddByteToOutputBuffer(SET_LOW_BYTE_DATA_BITS_CMD, false);
  dwLowPinsValue = (dwLowPinsValue & ~CHIP_SELECT_PIN); // set CS to low
  // set SK, DO, CS and GPIOL1-4 as output, set D1 as input
  AddByteToOutputBuffer(dwLowPinsValue, FALSE);
  AddByteToOutputBuffer('\xFB', false);
}

void AddWriteOutBuffer(DWORD dwNumControlBitsToWrite,
                       unsigned char pWriteControlBuffer[]) {
  DWORD dwModNumControlBitsToWrite = 0;
  DWORD dwControlBufferIndex = 0;
  DWORD dwNumControlBytes = 0;
  DWORD dwNumRemainingControlBits = 0;
  DWORD dwModNumDataBitsToWrite = 0;
  DWORD dwDataBufferIndex = 0;
  DWORD dwNumDataBytes = 0;
  DWORD dwNumRemainingDataBits = 0;

  // kra - 040608, added test for number of control bits to write, because for
  // SPI only, the number of control bits to write can be 0 on some SPI devices,
  // before a read operation is performed
  if (dwNumControlBitsToWrite > 1) {
    // adjust for bit count of 1 less than no of bits
    dwModNumControlBitsToWrite = (dwNumControlBitsToWrite - 1);

    // Number of control bytes is greater than 0, only if the minimum number of
    // control bits is 8
    dwNumControlBytes = (dwModNumControlBitsToWrite / 8);

    if (dwNumControlBytes > 0) {
      // Number of whole bytes
      dwNumControlBytes = (dwNumControlBytes - 1);

      // clk data bytes out
      AddByteToOutputBuffer(CLK_DATA_BYTES_OUT_ON_NEG_CLK_LSB_FIRST_CMD, FALSE);
      AddByteToOutputBuffer((dwNumControlBytes & '\xFF'), FALSE);
      AddByteToOutputBuffer(((dwNumControlBytes / 256) & '\xFF'), FALSE);

      // now add the data bytes to go out
      do {
        AddByteToOutputBuffer(pWriteControlBuffer[dwControlBufferIndex], FALSE);
        dwControlBufferIndex = (dwControlBufferIndex + 1);
      } while (dwControlBufferIndex < (dwNumControlBytes + 1));
    }

    dwNumRemainingControlBits = (dwModNumControlBitsToWrite % 8);

    // do remaining bits
    if (dwNumRemainingControlBits > 0) {
      // clk data bits out
      //*lpdwDataWriteBytesCommand =
      //CLK_DATA_BYTES_OUT_ON_NEG_CLK_LSB_FIRST_CMD; *lpdwDataWriteBitsCommand =
      //CLK_DATA_BITS_OUT_ON_NEG_CLK_LSB_FIRST_CMD;
      AddByteToOutputBuffer(CLK_DATA_BITS_OUT_ON_NEG_CLK_LSB_FIRST_CMD, FALSE);
      AddByteToOutputBuffer((dwNumRemainingControlBits & '\xFF'), FALSE);
      AddByteToOutputBuffer(pWriteControlBuffer[dwControlBufferIndex], FALSE);
    }
  }
}

void AddReadOutBuffer(DWORD dwNumDataBitsToRead) {
  DWORD dwModNumBitsToRead = 0;
  DWORD dwNumDataBytes = 0;
  DWORD dwNumRemainingDataBits = 0;

  // adjust for bit count of 1 less than no of bits
  dwModNumBitsToRead = (dwNumDataBitsToRead - 1);

  dwNumDataBytes = (dwModNumBitsToRead / 8);

  if (dwNumDataBytes > 0) {
    // Number of whole bytes
    dwNumDataBytes = (dwNumDataBytes - 1);

    // clk data bytes out
    AddByteToOutputBuffer(CLK_DATA_BYTES_IN_ON_NEG_CLK_LSB_FIRST_CMD, FALSE);
    AddByteToOutputBuffer((dwNumDataBytes & '\xFF'), FALSE);
    AddByteToOutputBuffer(((dwNumDataBytes / 256) & '\xFF'), FALSE);
  }

  // number of remaining bits
  dwNumRemainingDataBits = (dwModNumBitsToRead % 8);

  if (dwNumRemainingDataBits > 0) {
    // clk data bits out
    AddByteToOutputBuffer(CLK_DATA_BITS_IN_ON_NEG_CLK_LSB_FIRST_CMD, FALSE);
    AddByteToOutputBuffer((dwNumRemainingDataBits & '\xFF'), FALSE);
  }
}

void spi_SetCS(bool ChipSelect) {
  dwNumBytesToSend = 0; // Index to the output buffer
  dwNumBytesSent = 0;   // Count of actual bytes sent - used with FT_Write
  dwNumBytesToRead = 0; // Number of bytes available to read

  byOutputBuffer[dwNumBytesToSend++] = 0x80;
  dwLowPinsValue &= ~0x08;
  dwLowPinsValue |= ChipSelect ? 0x08 : 0x00;
  byOutputBuffer[dwNumBytesToSend++] = dwLowPinsValue;
  byOutputBuffer[dwNumBytesToSend++] = 0x3E; // byDirection

  SendBytesToDevice();

  dwNumBytesToSend = 0;
  dwNumBytesToRead = 0;
}

void spi_setGPIO(bool XXLo, bool EJLo) {
  dwNumBytesToSend = 0; // Index to the output buffer
  dwNumBytesSent = 0;   // Count of actual bytes sent - used with FT_Write
  dwNumBytesToRead = 0; // Number of bytes available to read

  byOutputBuffer[dwNumBytesToSend++] = 0x80;
  dwLowPinsValue &= ~0x30;
  dwLowPinsValue |= (XXLo ? 0x10 : 0x00) | (EJLo ? 0x20 : 0x00);
  byOutputBuffer[dwNumBytesToSend++] = dwLowPinsValue;
  byOutputBuffer[dwNumBytesToSend++] = 0x3E; // byDirection

  SendBytesToDevice();

  dwNumBytesToSend = 0;
  dwNumBytesToRead = 0;
}

void spi_QueueClockDelay(unsigned int numBytes) {
  if (numBytes == 0)
    return;

  unsigned int n = numBytes - 1;
  AddByteToOutputBuffer(CLK_DATA_BYTES_OUT_ON_NEG_CLK_LSB_FIRST_CMD, FALSE);
  AddByteToOutputBuffer((BYTE)(n & 0xFF), FALSE);
  AddByteToOutputBuffer((BYTE)((n >> 8) & 0xFF), FALSE);
  for (unsigned int i = 0; i < numBytes; i++)
    AddByteToOutputBuffer((BYTE)0x00, FALSE);
}

void closeDevice() {
  dwNumBytesToSend = 0;
  dwNumBytesSent = 0;
  dwNumBytesToRead = 0;
  dwLowPinsValue = 0;
#ifdef LIBFTDI

  if (ftdi != NULL) {
    // Reset from MPSSE back to serial mode, then flush and close
    ftdi_set_bitmode(ftdi, 0x00, BITMODE_RESET);
    ftdi_usb_purge_buffers(ftdi);
    ftdi_usb_close(ftdi);
    ftdi_free(ftdi);
    ftdi = NULL;
  }

#else // ftd2xx

  if (ftHandle != NULL) {
    // Deassert CS, then reset from MPSSE back to serial mode
    FT_SetBitMode(ftHandle, 0x00, 0x00);

    // Flush any remaining data in the USB buffers
    FT_Purge(ftHandle, FT_PURGE_RX | FT_PURGE_TX);

    FT_Close(ftHandle);
    ftHandle = NULL;
  }

#endif // LIBFTDI
}
