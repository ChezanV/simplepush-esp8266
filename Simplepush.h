#ifndef Simplepush_h
#define Simplepush_h

#include <Arduino.h>
#include <Hash.h>

#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

#include "AES.h"
#include "base64.h"

class Simplepush {
	public:
		Simplepush();
		~Simplepush();
		String wait_response();
		void send(char *key, char *title, char *message, char *event, char *buta, char *butb);
		void sendEncrypted(char *key, char *password, char *salt, char *title, char *message, char *event, char *buta, char *butb);
	private:
		String fdb;
		AES aes;
		void sendHttpPost(char *key, char* title, char* message, char *event, char *ivHex, char *buta, char *butb);
		int getHttpPostBodyLen(char *key, char *title, char *message, char *event, char *ivHex);
		void buildHttpPostBody(char *key, char *title, char *message, char *event, char *ivHex, char *body);
		void genIV(byte *iv);
		uint8_t getRandom();
		void encrypt(char *data, int dataSize, uint8_t *key, uint8_t *iv, char *cipherUrlSafeBase64);
		void genEncryptionKey(char *password, char *salt, uint8_t *key);
		int getBase64CipherLen(int clearDataLen);
		int hexify(uint8_t *in, size_t in_size, char *out, size_t out_size);
		int getCipherLen(int clearDataLen);
		void makeUrlSafe(char *base64Str);
};

#endif
