#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

#include <Simplepush.h>
String BUTA = "Yes";
String BUTB = "No";
String simplepush_key = "KEY";
long prev_millis = 0;

#define TIMEOUT 30			//aprox  30sec
int h = TIMEOUT*2;

ESP8266WiFiMulti WiFiMulti;
Simplepush simple;

void setup() {
	Serial.begin(115200);
	//Serial.setDebugOutput(true);

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
		String rsp = "";
		if (millis() - prev_millis >= 1010) {
			prev_millis = millis();
			++h;
			if (h < TIMEOUT) {			
				rsp = simple.wait_response();
				if(rsp == BUTA){
					Serial.print("<Received>");
					Serial.println(BUTA);
					h = TIMEOUT;
				}
				else if(rsp == BUTB){
					Serial.print("<Received>");
					Serial.println(BUTB);
					h = TIMEOUT;
				}		
			}
			else if (h == TIMEOUT){
				Serial.println("<Receive timeout>");
			}
			else if (h >= (TIMEOUT*2)){
				Serial.println("<Send push>");
				simple.send(const_cast<char*>(simplepush_key.c_str()), "Wow", "This is so easy...", "Event", const_cast<char*>(BUTA.c_str()), const_cast<char*>(BUTB.c_str()));
				h = 0;
			}
		}
	}
}
