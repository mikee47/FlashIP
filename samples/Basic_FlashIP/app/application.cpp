#include <SmingCore.h>
#include <LittleFS.h>
#include <FlashIP.h>
#include <Data/Buffer/LineBuffer.h>

#ifndef WIFI_SSID
#define WIFI_SSID "PleaseEnterSSID" // Put your SSID and password here
#define WIFI_PWD "PleaseEnterPass"
#endif

namespace
{
DEFINE_FSTR(FLASHIP_FILENAME, "flaship.bin")
HttpClient client;

void doUpgrade()
{
	Serial << _F("Updating...") << endl;

	fileDelete(FLASHIP_FILENAME);
	auto stream = new FileStream(FLASHIP_FILENAME, File::CreateNewAlways | File::WriteOnly);
	auto request = new HttpRequest(FIRMWARE_URL);
	request->setResponseStream(stream);

	request->onRequestComplete([](HttpConnection& client, bool success) -> int {
		if(!success) {
			Serial << _F("Download failed") << endl;
			return 0;
		}

		// Close response stream and therefore the flaship file object
		client.getResponse()->freeStreams();

		Serial << _F("Download complete, ") << fileGetSize(FLASHIP_FILENAME) << " bytes" << endl;
		auto part = *Storage::findPartition(Storage::Partition::Type::app);
		FlashIP fip;
		if(!fip.addFile(part, 0, FLASHIP_FILENAME)) {
			Serial << _F("ERROR: Failed to queue file for update") << endl;
			return 0;
		}

		Serial << _F("OK, updating firmware, please wait...") << endl;
		fip.execute(); // Doesn't return
	});

	if(!client.send(request)) {
		Serial << _F("ERROR: Rejected sending new request") << endl;
	}
}

void handleCommand(const String& str)
{
	if(F("connect") == str) {
		Serial << _F("Connecting to '") << WIFI_SSID << "'..." << endl;
		WifiStation.config(WIFI_SSID, WIFI_PWD);
		WifiStation.enable(true);
		WifiStation.connect();
		return;
	}

	if(F("ip") == str) {
		Serial << "ip: " << WifiStation.getIP() << ", mac: " << WifiStation.getMacAddress() << endl;
		return;
	}

	if(F("ota") == str) {
		doUpgrade();
		return;
	}

	if(F("ls") == str) {
		Directory dir;
		if(dir.open()) {
			while(dir.next()) {
				Serial << "  " << dir.stat().name << " " << dir.stat().size << endl;
			}
		}
		Serial << _F("filecount ") << dir.count() << endl;
		return;
	}

	if(F("help") == str) {
		Serial << _F("\r\n"
					 "Available commands:\r\n"
					 "  help - display this message\r\n"
					 "  ip - show current ip address\r\n"
					 "  ota - perform firmware update and reboot\r\n"
					 "  ls - list filing system content\r\n");
		return;
	}

	Serial << _F("unknown command: '") << str << '\'' << endl;
}

LineBuffer<16> commandBuffer;

void showPrompt()
{
	Serial << _F("FlashIP> ") << commandBuffer;
}

void serialCallBack(Stream& stream, char arrivedChar, unsigned short availableCharsCount)
{
	switch(commandBuffer.process(stream, Serial)) {
	case LineBufferBase::Action::submit:
		if(commandBuffer) {
			handleCommand(String(commandBuffer));
			commandBuffer.clear();
		}
		showPrompt();
		break;
	case LineBufferBase::Action::clear:
		showPrompt();
		break;
	default:;
	}
}

} // namespace

void init()
{
	Serial.begin(SERIAL_BAUD_RATE); // 115200 by default
	Serial.systemDebugOutput(true); // Debug output to serial
	Serial.clear();

	lfs_mount();

	WifiEvents.onStationGotIP([](IpAddress ip, IpAddress netmask, IpAddress gateway) { showPrompt(); });

	Serial << _F("Type 'help' and press enter for instructions.") << endl << endl;
	showPrompt();

	Serial.onDataReceived(serialCallBack);
}
