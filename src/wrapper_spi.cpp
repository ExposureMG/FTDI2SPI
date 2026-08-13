#include "stdafx.h"
#include <ftdi.h>
#include <stdio.h>
#include "wrapper_spi.h"

#define MAX_NUM_BYTES_USB_WRITE 16384
#define CHIP_SELECT_PIN '\x08'

const BYTE SET_LOW_BYTE_DATA_BITS_CMD = '\x80';
const BYTE SET_HIGH_BYTE_DATA_BITS_CMD = '\x82';
const BYTE SEND_ANSWER_BACK_IMMEDIATELY_CMD = '\x87';

const BYTE CLK_DATA_BYTES_OUT_ON_NEG_CLK_LSB_FIRST_CMD = '\x19';
const BYTE CLK_DATA_BITS_OUT_ON_NEG_CLK_LSB_FIRST_CMD = '\x1B';
const BYTE CLK_DATA_BYTES_IN_ON_NEG_CLK_LSB_FIRST_CMD = '\x2D';
const BYTE CLK_DATA_BITS_IN_ON_NEG_CLK_LSB_FIRST_CMD = '\x2F';

static struct ftdi_context *ftdi = NULL;

bool spi_init(void)
{
  ftdi = ftdi_new();
  if (!ftdi) {
    fprintf(stderr, "libftdi: Failed to allocate context\n");
    return false;
  }

  // Select Channel B as in original FTCSPI initialization
  ftdi_set_interface(ftdi, INTERFACE_B);

  // Auto detach ftdi_sio kernel driver on Linux
  ftdi->module_detach_mode = AUTO_DETACH_SIO_MODULE;

  // Open FT2232H (0x0403:0x6010) or FT4232H (0x0403:0x6011)
  int open_res = ftdi_usb_open(ftdi, 0x0403, 0x6010);
  if (open_res < 0) {
    open_res = ftdi_usb_open(ftdi, 0x0403, 0x6011);
  }

  if (open_res < 0) {
    fprintf(stderr, "libftdi: Failed to open FTDI device: %s (%d)\n", ftdi_get_error_string(ftdi), open_res);
    ftdi_free(ftdi);
    ftdi = NULL;
    return false;
  }

  ftdi_usb_reset(ftdi);
  ftdi_set_bitmode(ftdi, 0x00, BITMODE_RESET);
  
  // Enable MPSSE mode
  if (ftdi_set_bitmode(ftdi, 0x00, BITMODE_MPSSE) < 0) {
    fprintf(stderr, "libftdi: Failed to set MPSSE mode: %s\n", ftdi_get_error_string(ftdi));
    ftdi_usb_close(ftdi);
    ftdi_free(ftdi);
    ftdi = NULL;
    return false;
  }

  // 1ms latency timer for fast polling
  ftdi_set_latency_timer(ftdi, 1);

  // Clear buffers
  ftdi_tcioflush(ftdi);

  // Send initial MPSSE configuration sequence:
  // 0x8A: Disable divide-by-5 clocking (60MHz master clock)
  // 0x86 0x00 0x00: Set clock divisor (30MHz clock)
  // 0x80 0x08 0x3E: Set Low Data Bits (SK, DO, CS=1, GPIOL1-4) pin state and direction
  unsigned char init_buf[] = {
    0x8A,             // Disable div by 5
    0x86, 0x00, 0x00, // Set SK divisor = 0 (30MHz clock)
    0x80, 0x08, 0x3E  // CS pin high, SK/DO/CS/GPIOL output
  };
  ftdi_write_data(ftdi, init_buf, sizeof(init_buf));

  return true;
}

BYTE byOutputBuffer[OUTPUT_BUFFER_SIZE];
BYTE dwLowPinsValue = 0x08; // CS starts high
DWORD dwNumBytesToSend = 0;
DWORD dwNumBytesSent = 0;
DWORD dwNumBytesToRead = 0;

void SendBytesToDevice(void)
{
  if (!ftdi) return;

  DWORD dwNumDataBytesToSend = 0;
  DWORD dwTotalNumBytesSent = 0;

  if (dwNumBytesToSend > MAX_NUM_BYTES_USB_WRITE)
  {
    do
    {
      if ((dwTotalNumBytesSent + MAX_NUM_BYTES_USB_WRITE) <= dwNumBytesToSend)
        dwNumDataBytesToSend = MAX_NUM_BYTES_USB_WRITE;
      else
        dwNumDataBytesToSend = (dwNumBytesToSend - dwTotalNumBytesSent);

      int ret = ftdi_write_data(ftdi, &byOutputBuffer[dwTotalNumBytesSent], dwNumDataBytesToSend);
      if (ret < 0) {
        fprintf(stderr, "libftdi: ftdi_write_data error: %s\n", ftdi_get_error_string(ftdi));
        break;
      }
      dwTotalNumBytesSent += ret;
    }
    while (dwTotalNumBytesSent < dwNumBytesToSend);
  }
  else if (dwNumBytesToSend > 0)
  {
    int ret = ftdi_write_data(ftdi, byOutputBuffer, dwNumBytesToSend);
    if (ret < 0) {
      fprintf(stderr, "libftdi: ftdi_write_data error: %s\n", ftdi_get_error_string(ftdi));
    }
  }

  dwNumBytesToSend = 0;
}

void ClearOutputBuffer(void)
{
  dwNumBytesToSend = 0;
}

void AddByteToOutputBuffer(BYTE DataByte, bool bClearOutputBuffer)
{
  if (bClearOutputBuffer)
    dwNumBytesToSend = 0;

  if (dwNumBytesToSend < OUTPUT_BUFFER_SIZE)
    byOutputBuffer[dwNumBytesToSend++] = DataByte;
}

void SetAnswerFast(void)
{
  AddByteToOutputBuffer(SEND_ANSWER_BACK_IMMEDIATELY_CMD, false);
}

void GetDataFromDevice(unsigned int dwNumBytesToRead, unsigned char ReadDataBuffer[])
{
  if (!ftdi) throw "ERROR: DEVICE NOT OPEN";

  DWORD dwBytesReadIndex = 0;
  int try_count = 100;

  do {
    int ret = ftdi_read_data(ftdi, &ReadDataBuffer[dwBytesReadIndex], dwNumBytesToRead);
    if (ret > 0) {
      dwBytesReadIndex += ret;
      dwNumBytesToRead -= ret;
    }
  } while (dwNumBytesToRead > 0 && try_count-- > 0);

  if (dwNumBytesToRead > 0)
    throw "ERROR: NO DATA FROM DEVICE";
}

void DisableSPIChip(void)
{
  AddByteToOutputBuffer(SET_LOW_BYTE_DATA_BITS_CMD, false);
  dwLowPinsValue = (dwLowPinsValue | CHIP_SELECT_PIN); // CS high
  AddByteToOutputBuffer(dwLowPinsValue, false);
  AddByteToOutputBuffer(0x3E, false); // Direction
}

void EnableSPIChip(void)
{
  AddByteToOutputBuffer(SET_LOW_BYTE_DATA_BITS_CMD, false);
  dwLowPinsValue = (dwLowPinsValue & ~CHIP_SELECT_PIN); // CS low
  AddByteToOutputBuffer(dwLowPinsValue, false);
  AddByteToOutputBuffer(0x3E, false); // Direction
}

void AddWriteOutBuffer(DWORD dwNumControlBitsToWrite, unsigned char pWriteControlBuffer[])
{
  if (dwNumControlBitsToWrite > 1)
  {
    DWORD dwModNumControlBitsToWrite = (dwNumControlBitsToWrite - 1);
    DWORD dwNumControlBytes = (dwModNumControlBitsToWrite / 8);
    DWORD dwControlBufferIndex = 0;

    if (dwNumControlBytes > 0)
    {
      dwNumControlBytes = (dwNumControlBytes - 1);
      AddByteToOutputBuffer(CLK_DATA_BYTES_OUT_ON_NEG_CLK_LSB_FIRST_CMD, false);
      AddByteToOutputBuffer((dwNumControlBytes & 0xFF), false);
      AddByteToOutputBuffer(((dwNumControlBytes / 256) & 0xFF), false);

      do
      {
        AddByteToOutputBuffer(pWriteControlBuffer[dwControlBufferIndex], false);
        dwControlBufferIndex++;
      }
      while (dwControlBufferIndex < (dwNumControlBytes + 1));
    }

    DWORD dwNumRemainingControlBits = (dwModNumControlBitsToWrite % 8);
    if (dwNumRemainingControlBits > 0)
    {
      AddByteToOutputBuffer(CLK_DATA_BITS_OUT_ON_NEG_CLK_LSB_FIRST_CMD, false);
      AddByteToOutputBuffer((dwNumRemainingControlBits & 0xFF), false);
      AddByteToOutputBuffer(pWriteControlBuffer[dwControlBufferIndex], false);
    }
  }
}

void AddReadOutBuffer(DWORD dwNumDataBitsToRead)
{
  DWORD dwModNumBitsToRead = (dwNumDataBitsToRead - 1);
  DWORD dwNumDataBytes = (dwModNumBitsToRead / 8);

  if (dwNumDataBytes > 0)
  {
    dwNumDataBytes = (dwNumDataBytes - 1);
    AddByteToOutputBuffer(CLK_DATA_BYTES_IN_ON_NEG_CLK_LSB_FIRST_CMD, false);
    AddByteToOutputBuffer((dwNumDataBytes & 0xFF), false);
    AddByteToOutputBuffer(((dwNumDataBytes / 256) & 0xFF), false);
  }

  DWORD dwNumRemainingDataBits = (dwModNumBitsToRead % 8);
  if (dwNumRemainingDataBits > 0)
  {
    AddByteToOutputBuffer(CLK_DATA_BITS_IN_ON_NEG_CLK_LSB_FIRST_CMD, false);
    AddByteToOutputBuffer((dwNumRemainingDataBits & 0xFF), false);
  }
}

void spi_SetCS(bool ChipSelect)
{
  if (!ftdi) return;

  BYTE cmd[3];
  cmd[0] = 0x80;
  dwLowPinsValue &= ~0x08;
  dwLowPinsValue |= ChipSelect ? 0x08 : 0x00;
  cmd[1] = dwLowPinsValue;
  cmd[2] = 0x3E; // byDirection

  ftdi_write_data(ftdi, cmd, 3);

  dwNumBytesToSend = 0;
  dwNumBytesToRead = 0;
}

void spi_setGPIO(bool XXLo, bool EJLo)
{
  if (!ftdi) return;

  BYTE cmd[3];
  cmd[0] = 0x80;
  dwLowPinsValue &= ~0x30;
  dwLowPinsValue |= (XXLo ? 0x10 : 0x00) | (EJLo ? 0x20 : 0x00);
  cmd[1] = dwLowPinsValue;
  cmd[2] = 0x3E; // byDirection

  ftdi_write_data(ftdi, cmd, 3);

  dwNumBytesToSend = 0;
  dwNumBytesToRead = 0;
}

void closeDevice()
{
  if (ftdi) {
    ftdi_set_bitmode(ftdi, 0x00, BITMODE_RESET);
    ftdi_usb_close(ftdi);
    ftdi_free(ftdi);
    ftdi = NULL;
  }
}