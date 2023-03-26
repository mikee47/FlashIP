Flash In-Place
==============

.. highlight:: c++

This library is intended to support in-place application updates
where there is insufficient available flash storage for more than one application
partition.

This is aimed at the (Rp2040) Pico (W) which has only 2MBytes of flash.

This library takes a firmware image stored elsewhere in flash, then runs a simple piece of code
running from RAM. This copies the new firmware into the application partition then reboots.

This is similar to the `Arduino Pico <https://arduino-pico.readthedocs.io/en/latest/ota.html>`__ approach,
but it doesn't require any bootloader changes and the critical copier code (in RAM) is much simpler as it doesn't need
to know anything about filing systems; all it does is copy blocks from one region of flash to another.

Operation
---------

1. Verify the source image
    The application handles this e.g. CRC32 checksum.

2. Build a block list

    Most file systems will store larger files (especially of 2-300KBytes or more) in multiple,
    non-contiguous regions of flash storage. These regions are known as 'extents', and this
    information is obtained via the :cpp:func:`IFS::IFilelSystem::fgetextents` call.

    The :cpp:class:`FlashIP` class uses this to build a list, which the copier code then executes from RAM.
    This keeps the copier code very simple.

    Example::

        // Get the target partition
        auto part = *Storage::findPartition(Storage::Partition::Type::app);

        // Build the block list
        FlashIP fip;
        if(fip.addFile(part, 0, "firmware.bin")) {
            // Copy the image and reboot
            fip.execute();
        }


3. Write the block list to flash
    This code runs exclusively in RAM. All interrupts and DMA are disabled first.
    The block list is then iterated with a read/erase/program procedure.

4. Reboot

Issues
------

Recovery
    There is no recovery available if the update operation is interrupted part-way.
    If physical device access is available then firmware can be re-flashed via USB.

    Writing a second-stage bootloader and storing the block list in FLASH would mitigate this issue.
    For example, we'd reserve the first two flash sectors for this, with the application itself relocated.
    Each block-list entry will be invalidated once complete so the bootloader need only write incomplete
    blocks.

Compression
    Using a compressed firmware image would require e.g. uzlib to be located in RAM.
    We can add a build variable to make that happen.

    A quick test on the release ``HttpServer_ConfigNetwork`` image using gzip reduces the size
    from 341136 to 280084 bytes (82% of the original size).

Encryption
    Note that where updating via local network, encryption may not be necessary.

    If encryption is used, the application would need to decrypt that first.
    Typically this is done as the file is received, before writing the data to the local file.
