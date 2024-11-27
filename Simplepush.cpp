#include "Simplepush.h"

const uint8_t N_BLOCK_IV = 16;
const char *SALT_COMPATIBILITY = "1789F0B8C4A051E5";
const char *API_ENDPOINT = "http://api.simplepush.io/send";
const char *API_FEEDBACK = "http://api.simplepush.io/1/feedback/";
AES aes;
WiFiClient Simpleclient;
JsonDocument Feedback_json;
JsonDocument Response_json;

Simplepush::Simplepush(){}

Simplepush::~Simplepush(){}

void Simplepush::send(char *key, char *title, char *message, char *event, char *buta, char *butb){
	sendHttpPost(key, title, message, event, NULL, buta, butb);
}

void Simplepush::sendEncrypted(char *key, char *password, char *salt, char *title, char *message, char *event, char *buta, char *butb){

	// Generate key
	uint8_t encKey[16];
	genEncryptionKey(password, salt, encKey);

	// Generate IV
	uint8_t iv[16];
	char ivHex[33];
	genIV(iv);
	uint8_t ivCopy[16];
	memcpy(ivCopy, iv, 16);
	hexify(iv, sizeof(iv), ivHex, sizeof(ivHex));

	// Encrypt title and message
	char messageCipher[getBase64CipherLen(strlen(message))];
	encrypt(message, strlen(message), encKey, iv, messageCipher);

	if(title) {
		char titleCipher[getBase64CipherLen(strlen(title))];
		encrypt(title, strlen(title), encKey, ivCopy, titleCipher);
		
		sendHttpPost(key, titleCipher, messageCipher, event, ivHex, buta, butb);
	} else {
		sendHttpPost(key, NULL, messageCipher, event, ivHex, buta, butb);
	}
}

void Simplepush::sendHttpPost(char *key, char* title, char* message, char *event, char *ivHex, char *buta, char *butb) {
	HTTPClient http;
	http.begin(Simpleclient, API_ENDPOINT);
	http.addHeader("Content-Type", "application/json");
	String json;
	StaticJsonDocument<300> Send_json;
	Send_json["key"] = key;
	Send_json["title"] = title;
	Send_json["msg"] = message;
	Send_json["event"] = event;
	if(ivHex) {
		Send_json["encrypted"] = "true";
		Send_json["iv"] = ivHex;
	}
	if(buta || butb){
		JsonArray arr = Send_json.createNestedArray("actions");
		if(buta) {
			arr.add(buta);
		}
		if(butb) {
			arr.add(butb);
		}
	}
	
	serializeJson(Send_json, json);
	int httpCode = http.POST(json);
	if (httpCode > 0) {
		if (httpCode == HTTP_CODE_OK) {
			const String& payload = http.getString();
			DeserializationError error = deserializeJson(Feedback_json, payload);
			if (error) {
				Serial.print(F("deserializeJson() failed: "));
				Serial.println(error.c_str());
				return;
			}
			fdb = (const char*)Feedback_json["feedbackId"];
		}
	}
	else {
		Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
	}
	http.end();
}
String Simplepush::wait_response(){
	String q;
	HTTPClient http;
	char body[strlen(API_FEEDBACK)+strlen(fdb.c_str())+1];
	strcpy(body, API_FEEDBACK);
	strcat(body, fdb.c_str());
	http.begin(Simpleclient, body);
	int httpCode = http.GET();
	if (httpCode > 0) {
		if (httpCode == HTTP_CODE_OK) {
			const String& payload = http.getString();
			DeserializationError error = deserializeJson(Response_json, payload);
			if (error) {
				Serial.print(F("deserializeJson() failed: "));
				Serial.println(error.c_str());
				return error.c_str();
			}
			q = (const char*)Response_json["action_selected"];
		}
	}
	else {
		Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
	}	
	http.end();
	return q;
}

void Simplepush::encrypt(char *data, int dataSize, uint8_t *key, uint8_t *iv, char *cipherUrlSafeBase64) {
	aes.set_key(key, 16);

	// Encrypt
	int cipherLen = getCipherLen(dataSize);
	uint8_t buffer[cipherLen + 1];
	// For some unknown reason dataSize needs to be one byte bigger
	aes.do_aes_encrypt((uint8_t *)data, dataSize + 1, buffer, key, 128, iv);

	// Encode to URL safe Base64
	base64_encode(cipherUrlSafeBase64, (char *)buffer, aes.get_size());
	makeUrlSafe(cipherUrlSafeBase64);
}

void Simplepush::genEncryptionKey(char *password, char *salt, uint8_t *key) {
	int tmpLen;

	if(salt) {
		tmpLen = strlen(password) + strlen(salt);
	} else {
		tmpLen = strlen(password) + strlen(SALT_COMPATIBILITY);
	}

	char tmp[tmpLen];

	if(salt) {
		strcpy(tmp, password);
		strcat(tmp, salt);
	} else {
		// Compatibility for older versions
		strcpy(tmp, password);
		strcat(tmp, SALT_COMPATIBILITY);
	}

	uint8_t hash[20];
	sha1(tmp, &hash[0]);

	// Only take the first 16 bytes of the hash
	memcpy(key, hash, 16);
}

// Generate a random initialization vector
void Simplepush::genIV(uint8_t *iv) {
    for (int i = 0 ; i < N_BLOCK_IV ; i++ ) {
        iv[i]= (uint8_t) getRandom();
    }
}

// https://gist.github.com/cellularmitosis/0d8c0abf7f8aa6a2dff3
int Simplepush::hexify(uint8_t *in, size_t in_size, char *out, size_t out_size)
{
	if (in_size == 0 || out_size == 0) return 0;

	char map[16+1] = "0123456789ABCDEF";

	int bytes_written = 0;
	size_t i = 0;
	while(i < in_size && (i*2 + (2+1)) <= out_size)
	{
		uint8_t high_nibble = (in[i] & 0xF0) >> 4;
		*out = map[high_nibble];
		out++;

		uint8_t low_nibble = in[i] & 0x0F;
		*out = map[low_nibble];
		out++;

		i++;

		bytes_written += 2;
	}
	*out = '\0';

	return bytes_written;
}

uint8_t Simplepush::getRandom() {
    uint8_t really_random = *(volatile uint8_t *)0x3FF20E44;
    return really_random;
}

int Simplepush::getCipherLen(int clearDataLen) {
	return (clearDataLen/16 + 1) * 16 + 1;
}

int Simplepush::getBase64CipherLen(int clearDataLen) {
	int cipherLen = getCipherLen(clearDataLen);
	int base64Len = ((4 * cipherLen / 3) + 3) & ~3;

	return base64Len;
}

void Simplepush::makeUrlSafe(char *base64Str) {
	int j = 0;

	while (base64Str[j] != '\0'){
		switch(base64Str[j]) {
			case '+':
				base64Str[j] = '-';
				break;
			case '/':
				base64Str[j] = '_';
				break;
			case '=':
				base64Str[j] = ',';
				break;
		}

		j++;
	}
}
