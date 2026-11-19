#ifndef Simplepush_h
#define Simplepush_h

#include <Arduino.h>

typedef void (*SimplepushWaitCallback)();

class Simplepush {
public:
    Simplepush();
    ~Simplepush();
	 bool timedOut;
    String wait_response();
	void setWaitCallback(SimplepushWaitCallback callback);
	void stop();
    void send(
        char *key,
        char *title,
        char *message,
        char *event,
        char *buta,
        char *butb
    );

private:
    String fdb;
	SimplepushWaitCallback waitCallback;
    void sendHttpPost(
        char *key,
        char *title,
        char *message,
        char *event,
        char *buta,
        char *butb
    );
};

#endif
