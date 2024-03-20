/****
 * Host/fip.cpp
 *
 * Copyright 2023 mikee47 <mike@sillyhouse.net>
 *
 * This file is part of the FlashIP Library
 *
 * This library is free software: you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation, version 3 or later.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this library.
 * If not, see <https://www.gnu.org/licenses/>.
 *
 ****/

#include "../include/FlashIP.h"
#include <esp_spi_flash.h>
#include <esp_systemapi.h>

#define FLASH_SECTOR_SIZE INTERNAL_FLASH_SECTOR_SIZE

namespace
{
uint8_t buffer[FLASH_SECTOR_SIZE];

void programBlock(int32_t dstAddress)
{
	flashmem_erase_sector(dstAddress / FLASH_SECTOR_SIZE);
	flashmem_write_internal(buffer, dstAddress, FLASH_SECTOR_SIZE);
}

void programItem(FlashIP::Item* item)
{
	uint32_t dstAddress = item->targetOffset;
	uint32_t blockOffset{0};
	unsigned blockIndex{0};

	FlashIP::Extent ext = item->ext[0];
	while(blockIndex < item->extCount) {
		size_t len = std::min(ext.length, FLASH_SECTOR_SIZE - blockOffset);
		flashmem_read(&buffer[blockOffset], ext.sourceOffset, len);
		blockOffset += len;
		ext.sourceOffset += len;
		ext.length -= len;
		if(ext.length == 0) {
			if(ext.repeat--) {
				ext.length = item->ext[blockIndex].length;
				ext.sourceOffset += ext.skip;
			} else {
				++blockIndex;
				if(blockIndex == item->extCount) {
					break;
				}
				ext = item->ext[blockIndex];
			}
		}
		if(blockOffset == FLASH_SECTOR_SIZE) {
			programBlock(dstAddress);
			dstAddress += FLASH_SECTOR_SIZE;
			blockOffset = 0;
		}
	}

	if(blockOffset) {
		memset(&buffer[blockOffset], 0xff, FLASH_SECTOR_SIZE - blockOffset);
		programBlock(dstAddress);
	}
}
} // namespace

void FlashIP::execute()
{
	noInterrupts();

	for(auto item = itemList; item; item = item->next) {
		programItem(item);
	}

#ifndef FLASHIP_RETURN
	system_restart();
	exit(0);
#endif
}
