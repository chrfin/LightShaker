/*
 * drvNvMemory.h
 *
 *  Created on: 15.10.2019
 *      Author: ChrLin00
 */

#ifndef SRC_DRVNVMEMORY_H_
#define SRC_DRVNVMEMORY_H_

#include "stdbool.h"
#include "stdint.h"

#define ERR_NVMEM_OUTOFRANGE	 0x01
#define ERR_NVMEM_ALREADYWRITTEN 0x02
#define ERR_FLASH_ERASEFAIL		 0x03
#define ERR_FLASH_WRONGREADBACK	 0x04
#define ERR_FLASH_NOTERASED		 0x05
#define ERR_FLASH_WRPTERR		 0x06

//address Table:
#define NVMEM_AD_GLOBAL_RED		0
#define NVMEM_AD_GLOBAL_GREEN	1
#define NVMEM_AD_GLOBAL_BLUE	2
#define NVMEM_AD_ROWS_VISIBLE	3
#define NVMEM_AD_OVERSCAN		4
#define NVMEM_AD_PICTURE_START	5

#define NVMEM_WORDS_ALLOCATED	21	//really used memory (available is PageCount*Pagesize/2)



void NvMem_init();
uint16_t NvMem_read(uint16_t address);
uint8_t NvMem_write(uint16_t address, uint16_t data);
uint8_t NvMem_SaveToFlash();

#endif /* SRC_DRVNVMEMORY_H_ */
