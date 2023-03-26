/****
 * Rp2040/fip.cpp
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
#include <hardware/flash.h>
#include <esp_spi_flash.h>
#include <pico/bootrom.h>
#include <hardware/structs/ssi.h>
#include <hardware/structs/ioqspi.h>
#include <hardware/structs/watchdog.h>
#include <pico/sync.h>
#include <esp_attr.h>

// Un-comment this for testing without restart; don't overwrite application though!
// #define DISABLE_RESTART
namespace
{
enum FlashCommand {
	FLASHCMD_READ_DATA = 0x03,
	FLASHCMD_BLOCK_ERASE = 0xd8,
};

template <typename T> static void lookup(T& func, uint16_t code)
{
	func = T(rom_func_lookup_inline(code));
}

// Bitbanging the chip select using IO overrides, in case RAM-resident IRQs
// are still running, and the FIFO bottoms out. (the bootrom does the same)
void IRAM_ATTR cs_force(bool high)
{
	uint32_t field_val =
		high ? IO_QSPI_GPIO_QSPI_SS_CTRL_OUTOVER_VALUE_HIGH : IO_QSPI_GPIO_QSPI_SS_CTRL_OUTOVER_VALUE_LOW;
	hw_write_masked(&ioqspi_hw->io[1].ctrl, field_val << IO_QSPI_GPIO_QSPI_SS_CTRL_OUTOVER_LSB,
					IO_QSPI_GPIO_QSPI_SS_CTRL_OUTOVER_BITS);
}

void IRAM_ATTR ssi_write(uint8_t value)
{
	ssi_hw->dr0 = value;
}

uint8_t IRAM_ATTR ssi_read()
{
	while(!(ssi_hw->sr & SSI_SR_RFNE_BITS)) {
	}
	return ssi_hw->dr0;
}

void __forceinline wdt_reset()
{
	constexpr unsigned ticksPerMs{2000};
	watchdog_hw->load = 2000 * ticksPerMs;
}

void __forceinline __attribute__((noreturn)) restart()
{
	// Force a restart
	watchdog_hw->ctrl = WATCHDOG_CTRL_TRIGGER_BITS;
	for(;;) {
	}
}

class IpFlasher
{
public:
	void __attribute__((noreturn)) execute(FlashIP::Item* item)
	{
		lookup(connect_internal_flash, ROM_FUNC_CONNECT_INTERNAL_FLASH);
		lookup(exit_xip, ROM_FUNC_FLASH_EXIT_XIP);
		lookup(range_erase, ROM_FUNC_FLASH_RANGE_ERASE);
		lookup(range_program, ROM_FUNC_FLASH_RANGE_PROGRAM);
		lookup(rom_memset, ROM_FUNC_MEMSET);

		assert(connect_internal_flash && exit_xip && range_erase && range_program && rom_memset);
		start(item);
	}

private:
	void __attribute__((noinline, noreturn)) IRAM_ATTR start(FlashIP::Item* item)
	{
#ifdef DISABLE_RESTART
		// Ensure the `flash_enable_xip_via_boot2` code is cached
		flash_range_program(0, nullptr, 0);
#endif

		// No flash accesses after this point
		__compiler_memory_barrier();

		auto save = save_and_disable_interrupts();
		(void)save;

		connect_internal_flash();
		exit_xip();

		for(; item; item = item->next) {
			programItem(item);
		}

#ifdef DISABLE_RESTART
		// Restore normal XIP operation
		flash_range_program(0, nullptr, 0);
		restore_interrupts(save);
#else
		restart();
#endif
	}

	void IRAM_ATTR programItem(FlashIP::Item* item)
	{
		uint32_t dstAddress = item->targetOffset;
		uint32_t blockOffset{0};
		unsigned blockIndex{0};

		FlashIP::Extent ext = item->ext[0];
		while(blockIndex < item->extCount) {
			size_t len = std::min(ext.length, FLASH_SECTOR_SIZE - blockOffset);
			read(ext.sourceOffset, &buffer[blockOffset], len);
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
			rom_memset(&buffer[blockOffset], 0xff, FLASH_SECTOR_SIZE - blockOffset);
			programBlock(dstAddress);
		}
	}

	void IRAM_ATTR programBlock(uint32_t dstAddress)
	{
		wdt_reset();
		// TODO: Can we erase chunks larger than one sector? A block, perhaps?
		cs_force(false);
		range_erase(dstAddress, FLASH_SECTOR_SIZE, FLASH_BLOCK_SIZE, FLASHCMD_BLOCK_ERASE);
		cs_force(true);
		cs_force(false);
		range_program(dstAddress, buffer, FLASH_SECTOR_SIZE);
		cs_force(true);
	};

	void IRAM_ATTR read(uint32_t flash_ofs, uint8_t* buffer, size_t count)
	{
		cs_force(false);

		ssi_write(FLASHCMD_READ_DATA);
		ssi_write(flash_ofs >> 16);
		ssi_write(flash_ofs >> 8);
		ssi_write(flash_ofs);
		ssi_write(0xff); // First byte

		// Skip response to cmd/address bytes
		for(unsigned skip = 4; skip--;) {
			ssi_read();
		}

		// Read the data bytes
		do {
			if(--count) {
				ssi_write(0xff); // Next byte
			}
			*buffer++ = ssi_read();
		} while(count);

		cs_force(true);
	}

	rom_connect_internal_flash_fn connect_internal_flash;
	rom_flash_exit_xip_fn exit_xip;
	rom_flash_range_erase_fn range_erase;
	rom_flash_range_program_fn range_program;
	rom_memset_fn rom_memset;
	uint8_t buffer[FLASH_SECTOR_SIZE];
};

} // namespace

void FlashIP::execute()
{
	IpFlasher().execute(itemList);
}
