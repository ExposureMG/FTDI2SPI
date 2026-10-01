/*!
*
*/

#ifndef FLASHER_H
#define FLASHER_H

#include <stdint.h>
#include "sfc.h"
extern bool g_is_emmc;
void FlashReadStatus(unsigned char Status[]);
void FlashDataInit(unsigned char FlashConfig[]);
void FlashDataDeInit(void);
void PowerUp(void);
void Shutdown(void);
void Update(void);
void FlashDataErase( unsigned int reg );
void FlashDataRead( unsigned char * pData, unsigned int Arg, unsigned int Words );
void FlashDataWrite( unsigned char Data[], unsigned int Arg, unsigned int Words );
void fixSpare_ECC(unsigned char *blockdata, uint32_t block_num );
void fixSB(unsigned char *blockdata);

// Read nPages consecutive physical pages in batches
#define NAND_FAST_READ
#define NAND_READ_BATCH_PAGES   32
#define NAND_READ_DELAY_BYTES   375

void FlashDataReadBatch(unsigned char* pData, unsigned int Block, unsigned int nPages, unsigned int bytesPerPage);

#endif
