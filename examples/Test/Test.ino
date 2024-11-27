#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

#include <Simplepush.h>



ESP8266WiFiMulti WiFiMulti;
Simplepush simple;

void setup() {
	Serial.begin(115200);
	Serial.setDebugOutput(true);

	Serial.println();
	Serial.println();
	Serial.println();

	for(uint8_t t = 4; t > 0; t--) {
		Serial.printf("[SETUP] WAIT %d...\n", t);
		Serial.flush();
		delay(1000);
	}

	WiFiMulti.addAP("YourWifiSSID", "WifiPassword");
}

void loop() {
	if((WiFiMulti.run() == WL_CONNECTED)) {
		simple.send("YourSimplepushKey", "Wow", "This is so easy", "Event", NULL, NULL);
		simple.send("YourSimplepushKey", NULL, "No title and no event. Just a message.", NULL, NULL, NULL);
		simple.sendEncrypted("YourSimplepushKey", "password", "salt", "Wow", "This is so secure.", "Event", NULL, NULL);
		delay(5000);
	}
}
