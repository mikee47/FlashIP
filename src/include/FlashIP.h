/****
 * FlashIP.h
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

#pragma once

#include <Storage/Partition.h>
#include <FlashString/ObjectBase.hpp>
#include <IFS/Extent.h>

#ifdef FLASHIP_RETURN
#define FLASHIP_NORETURN
#else
#define FLASHIP_NORETURN __attribute__((noreturn))
#endif

/**
 * @brief Manages in-place flash updates
 */
class FlashIP
{
public:
	/**
	 * @brief Describes contiguous run of file data
	 * @note Similar information as IFS::Extent but specific to 32-bit flash devices
	 */
	struct Extent {
		uintptr_t sourceOffset; ///< Address in flash to read from
		uint32_t length;		///< Size of block in bytes
		uint16_t skip;			///< Skip bytes to next repeat
		uint16_t repeat;		///< Number of repeats
	};

	/**
     * @brief Information about what to copy and where to
     */
	struct Item {
		uintptr_t targetOffset; ///< Starting write flash address
		uint16_t extCount;		///< Number of extents
		uint16_t reserved;
		Item* next;
		Extent ext[];
	};

	~FlashIP();

	/**
     * @brief Add a file to be written to the given target partition
     * @param targetPartition Destination for file
	 * @param offset Offset from start of partition to write
     * @param filename File to write. Must be on internal flash storage.
     * @retval bool true if operation was successfully queued
     */
	bool addFile(Storage::Partition targetPartition, uint32_t offset, const String& filename);

	/**
     * @brief Add a data chunk file to be written at a location within a partition
     * @param targetPartition Where to write
	 * @param offset Offset from start of partition to write
     * @param obj Object to write
     * @retval bool true if operation was successfully queued
     */
	bool addObject(Storage::Partition targetPartition, uint32_t offset, const FSTR::ObjectBase& obj);

	bool addItem(Storage::Partition targetPartition, uint32_t offset, Storage::Partition& sourcePartition,
				 IFS::Extent* extents, uint16_t extCount);

	/**
     * @brief Execute the queued operations then reboot
     */
	void FLASHIP_NORETURN execute();

private:
	static Item* allocItem(uint32_t targetOffset, uint16_t extCount);
	void addItem(Item* item);

	Item* itemList{};
};
