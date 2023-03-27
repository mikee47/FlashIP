#include <SmingCore.h>
#include <LittleFS.h>
#include <FlashIP.h>

namespace
{
DEFINE_FSTR(FLASHIP_FILENAME, "flaship.bin")

#define CHECK(res)                                                                                                     \
	if(!(res)) {                                                                                                       \
		Serial << F(#res " failed") << endl;                                                                           \
		exit(1);                                                                                                       \
	}

void updateTest()
{
	Serial.begin(SERIAL_BAUD_RATE);
	Serial.systemDebugOutput(true);

	lfs_mount();
	Serial << _F("Updating...") << endl;

	// Wipe partition initially
	auto part = *Storage::findPartition(Storage::Partition::Type::app);
	CHECK(part)
	part.erase_range(0, part.size());

	FlashIP fip;
	CHECK(fip.addFile(part, 0, FLASHIP_FILENAME))

	// Doesn't normally return, but we've exceptionally defined FLASHIP_RETURN for this test application
	fip.execute();

	File f;
	CHECK(f.open(FLASHIP_FILENAME))
	auto fileSize = f.getSize();
	uint8_t buf1[fileSize];
	CHECK(f.read(buf1, fileSize) == fileSize)
	uint8_t buf2[fileSize];
	CHECK(part.read(0, buf2, fileSize))
	CHECK(memcmp(buf1, buf2, fileSize) == 0)

	Serial << _F("OK, tests passed") << endl;

	System.restart();
}

} // namespace

void init()
{
	System.queueCallback(updateTest);
}
