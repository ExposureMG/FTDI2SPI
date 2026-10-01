
#ifndef WRAPPER_H
#define WRAPPER_H

#include <stdexcept>

// Transfer failures invalidate the device session. The exported operations
// translate this exception into SPI_TRANSPORT_ERROR.
class SpiTransportError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};
constexpr int SPI_TRANSPORT_ERROR = -12;

#ifdef LIBFTDI
  #ifdef _WIN32
    #include "stdafx.h"
  #else
    // libftdi does not supply the Windows types used by this interface.
    #include <stdint.h>
    typedef uint8_t  BYTE;
    typedef uint16_t WORD;
    typedef uint32_t DWORD;
  #endif
#else
  #include "ftd2xx.h"
#endif

bool spi_init( void );
void spi_setGPIO( bool GPIO1, bool GPIO2 );
void spi_SetCS( bool ChipSelect );

void AddByteToOutputBuffer( BYTE DataByte, bool bClearOutputBuffer );
void ClearOutputBuffer( void );
void SendBytesToDevice( void );
void DisableSPIChip( void );
void EnableSPIChip( void );
void SetAnswerFast( void );
void AddWriteOutBuffer( DWORD dwNumControlBitsToWrite, unsigned char pWriteControlBuffer[] );
void AddReadOutBuffer( DWORD dwNumDataBitsToRead );
void GetDataFromDevice(unsigned int dwNumDataBitsToRead, unsigned char ReadDataBuffer[] );
void spi_QueueClockDelay( unsigned int numBytes );
void closeDevice();

// Host-side MPSSE command staging buffer, Enlarged from 65535 so a
// whole NAND_READ_BATCH_PAGES batch of read commands fits in one queue
#define OUTPUT_BUFFER_SIZE (512 * 1024)
extern BYTE byOutputBuffer[OUTPUT_BUFFER_SIZE];
extern DWORD dwNumBytesToSend;
#endif
