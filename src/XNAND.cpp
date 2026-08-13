
#include "stdafx.h"
#include <windows.h>

#include "XSPI.h"
#include "wrapper_spi.h"
#include <stdio.h>

extern void ClearOutputBuffer( void );
extern void SetAnswerFast( void );
extern void SendBytesToDevice( void );
extern void GetDataFromDevice( unsigned int dwNumDataBitsToRead, unsigned char ReadDataBuffer[] );

bool XNANDWaitReady(unsigned int timeout)
{
	do {
		if (!(XSPIReadBYTE_(0x04) & 0x01))
			return true;
	} while (timeout--);

	return false;
}

unsigned int XNANDGetStatus()
{
	return XSPIReadWORD_(0x04);
}

void XNANDClearStatus()
{
	unsigned char tmp[4] = { 0,2,0,0 };

	XSPIRead_sync(4, tmp);	
	XSPIWrite_sync(4, tmp);	

}

unsigned int XNANDReadStart(unsigned int block)
{
	unsigned int res;
	unsigned int tries = 0x1000;

	XNANDClearStatus();

	ClearOutputBuffer();	

	XSPIWriteWORD_(0x0C, block << 9);
	XSPIWriteBYTE_(0x08, 0x03);

	SendBytesToDevice();

	if (!XNANDWaitReady(0x1000))
		return 0x8011;

	res = 0;

	ClearOutputBuffer();
	XSPIWrite0_(0x0C);
	SendBytesToDevice();

	return res;
}

void XNANDReadProcess(unsigned char * pData, unsigned char Words ) {
	unsigned int len = Words;

	ClearOutputBuffer();
	while (Words--) {
		XSPIWrite0_(0x08);
		XSPIRead_(0x10, pData);
	}

	SetAnswerFast();
	SendBytesToDevice();
	GetDataFromDevice( len*4, pData );
}

unsigned int XNANDErase(unsigned int block)
{
	unsigned char tmp[4];

	XNANDClearStatus();

	XSPIRead_sync(0, tmp);	
	tmp[0] |= 0x08;
	XSPIWrite_sync(0, tmp);

	ClearOutputBuffer();	

	XSPIWriteWORD_(0x0C, block << 9);
	XSPIWriteBYTE_(0x08, 0xAA);
	XSPIWriteBYTE_(0x08, 0x55);
	XSPIWriteBYTE_(0x08, 0x5);

	SendBytesToDevice();

	if (!XNANDWaitReady(0x1000))
		return 0x8003;

	return XNANDGetStatus();	
}

void XNANDWriteStart()
{

	ClearOutputBuffer();
	XSPIWrite0_(0x0C);
	SendBytesToDevice();

}

void XNANDWriteProcess(unsigned char Buffer[], unsigned char Words) 
{

	ClearOutputBuffer();	
	while (Words--) {
		XSPIWrite_(0x10, Buffer);
		XSPIWriteBYTE_(0x08, 0x01);
		Buffer += 4;
	}
	SendBytesToDevice();

}

unsigned int XNANDWriteExecute(unsigned int block) 
{
	unsigned int tries = 0x1000;

	ClearOutputBuffer();	
	XSPIWriteWORD_(0x0C, block << 9);
	XSPIWriteBYTE_(0x08, 0x55);
	XSPIWriteBYTE_(0x08, 0xAA);
	XSPIWriteBYTE_(0x08, 0x4);
	SendBytesToDevice();

	if (!XNANDWaitReady(0x1000))
		return 0x8021;

	return XNANDGetStatus();

}

// ---------------------------------------------------------------------------
// Fast batched read path (optimization)
// ---------------------------------------------------------------------------

// Queue SPI clocks with the chip de-selected, to give the NAND time to load a
// page into the SFC page buffer (tR). This replaces the per-page busy-bit poll,
// which cost a full USB read round-trip per page. numBytes * 8 clocks are issued
// at the SPI clock (~0.267 us/byte at 30 MHz). CS is high here, so the dummy
// bytes are ignored by the flash.
static void XNANDQueueReadDelay(unsigned int numBytes)
{
	if (numBytes == 0)
		return;

	unsigned int n = numBytes - 1;              // MPSSE length field is (count - 1)
	AddByteToOutputBuffer((BYTE)0x19, false);   // CLK_DATA_BYTES_OUT, -ve edge, LSB first
	AddByteToOutputBuffer((BYTE)(n & 0xFF), false);
	AddByteToOutputBuffer((BYTE)((n >> 8) & 0xFF), false);
	for (unsigned int i = 0; i < numBytes; i++)
		AddByteToOutputBuffer((BYTE)0x00, false);
}

// Read nPages consecutive physical pages in a single USB write + single USB read.
// Mirrors XNANDReadStart()+XNANDReadProcess() per page, but: status is cleared once
// for the whole batch, the busy-poll is replaced by an in-stream delay, and all the
// page commands/reads are queued before a single SendBytesToDevice()/GetDataFromDevice().
//   wordsPerPage : bytes-per-page / 4 (132 for 0x210 incl. spare, 128 for 0x200 raw)
//   delayBytes   : page-load delay, see XNANDQueueReadDelay()
// Keep nPages * wordsPerPage * 4 below the FT2232H RX FIFO (~4 KB).
void XNANDReadBatch(unsigned char* pData, unsigned int startBlock,
                    unsigned int nPages, unsigned int wordsPerPage,
                    unsigned int delayBytes)
{
	XNANDClearStatus();          // once per batch, not once per page

	ClearOutputBuffer();

	for (unsigned int p = 0; p < nPages; p++) {
		unsigned int block = startBlock + p;

		// start read of one physical page into the SFC page buffer
		XSPIQueueBytes_(0x0C, block << 9);   // set address (queue-only)
		XSPIWriteBYTE_(0x08, 0x03);          // PHY_PAGE_TO_BUF (queue-only)
		XNANDQueueReadDelay(delayBytes);     // wait tR in-stream, no USB round-trip
		XSPIWrite0_(0x0C);                   // reset page-buffer pointer

		// drain the page buffer
		for (unsigned int w = 0; w < wordsPerPage; w++) {
			XSPIWrite0_(0x08);               // PAGE_BUF_TO_REG: advance one word
			XSPIRead_(0x10, pData);          // queue read of the data register
		}
	}

	SetAnswerFast();
	SendBytesToDevice();
	GetDataFromDevice(nPages * wordsPerPage * 4, pData);
}