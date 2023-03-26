/****
 * FlashIP.cpp
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

#include "include/FlashIP.h"
#include <IFS/File.h>
#include <Storage/SpiFlash.h>
#include <debug_progmem.h>
#include <esp_spi_flash.h>
#include <memory>

FlashIP::~FlashIP()
{
	while(itemList) {
		auto next = itemList->next;
		delete itemList;
		itemList = next;
	}
}

FlashIP::Item* FlashIP::allocItem(uint32_t targetOffset, uint16_t extCount)
{
	auto bufSize = sizeof(Item) + extCount * sizeof(Extent);
	return new(malloc(bufSize)) Item{
		.targetOffset = targetOffset,
		.extCount = extCount,
	};
}

void FlashIP::addItem(Item* newItem)
{
	if(itemList) {
		auto item = itemList;
		while(item->next) {
			item = item->next;
		}
		item->next = newItem;
	} else {
		itemList = newItem;
	}
}

bool FlashIP::addItem(Storage::Partition targetPartition, uint32_t offset, Storage::Partition& sourcePartition,
					  IFS::Extent* extents, uint16_t extCount)
{
	if(targetPartition.getDeviceName() != Storage::spiFlash->getName()) {
		debug_e("[FIP] Target must be main SPI flash device");
		return false;
	}
	if(sourcePartition.getDeviceName() != Storage::spiFlash->getName()) {
		debug_e("[FIP] Source must be main SPI flash device");
		return false;
	}

	auto item = allocItem(targetPartition.address() + offset, extCount);
	if(!item) {
		debug_e("[FIP] Item[%u] alloc failed", extCount);
		return false;
	}

	uint32_t sourceBase = sourcePartition.address();
	for(unsigned i = 0; i < extCount; ++i) {
		auto& ext = extents[i];
		ext.offset += sourceBase;
		item->ext[i] = Extent{
			.sourceOffset = ext.offset,
			.length = ext.length,
			.skip = ext.skip,
			.repeat = ext.repeat,
		};
	}

	addItem(item);

	return true;
}

bool FlashIP::addFile(Storage::Partition targetPartition, uint32_t offset, const String& filename)
{
	IFS::File file;
	if(!file.open(filename)) {
		debug_e("[FIP] File open failed");
		return false;
	}
	int res = file.getExtents(nullptr, nullptr, 0);
	if(res < 0) {
		debug_e("[FIP] getExtents(%s): %d (%s)", filename.c_str(), res, file.getLastErrorString().c_str());
		return false;
	}
	uint16_t extCount = res;
	if(extCount == 0) {
		debug_w("[FIP] Empty file");
		return false;
	}
	std::unique_ptr<IFS::Extent[]> extents{new IFS::Extent[extCount]};
	if(!extents) {
		debug_e("[FIP] Alloc Extent[%u] failed", extCount);
		return false;
	}
	Storage::Partition sourcePartition;
	res = file.getExtents(&sourcePartition, extents.get(), extCount);
	if(res != int(extCount)) {
		debug_e("[FIP] getExtents() failed");
		return false;
	}
	file.close();

	debug_d("Got %u extents for '%s' on partition '%s'", extCount, filename.c_str(), sourcePartition.name().c_str());

	return addItem(targetPartition, offset, sourcePartition, extents.get(), extCount);
}

bool FlashIP::addObject(Storage::Partition targetPartition, uint32_t offset, const FSTR::ObjectBase& obj)
{
	auto item = allocItem(targetPartition.address() + offset, 1);
	if(!item) {
		return false;
	}
	item->ext[0] = Extent{
		.sourceOffset = flashmem_get_address(obj.data()),
		.length = obj.length(),
	};
	item->next = itemList;
	itemList = item;

	debug_d("item.target 0x%08x, source 0x%08x", item->targetOffset, item->ext[0].sourceOffset);

	return true;
}
