/*
  ESP8266 + Simplepush API v1
  Exemplu simplu FARA butoane / fara raspuns.

  Trimite o notificare Simplepush si nu asteapta raspunsul
  utilizatorului.

  Completeaza datele Wi-Fi, API_TOKEN si TOPIC.
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "Simplepush.h"

// -------------------- CONFIGURARE --------------------

const char* WIFI_SSID     = "NUME_WIFI";
const char* WIFI_PASSWORD = "PAROLA_WIFI";

char API_TOKEN[] = "TOKEN_SIMPLEPUSH";
char TOPIC[]     = "esp8266-test";

char TITLE[]   = "Notificare ESP8266";
char MESSAGE[] = "Notificare fara butoane.";

// -------------------- OBIECT --------------------

Simplepush simplepush;

bool notificationSent = false;

// -------------------- WIFI --------------------

void connectWiFi()
{
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  Serial.print("Conectare Wi-Fi");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    yield();
  }

  Serial.println();
  Serial.print("Conectat. IP: ");
  Serial.println(WiFi.localIP());
}

// -------------------- TRIMITERE NOTIFICARE --------------------

void sendNotification()
{
  Serial.println();
  Serial.println("Trimit notificarea Simplepush...");

  // NULL, NULL = fara butoane.
  // In Simplepush.cpp, Action-Input si Wait:true sunt trimise
  // numai daca ambele etichete de butoane sunt completate.
  simplepush.send(
    API_TOKEN,
    TITLE,
    MESSAGE,
    TOPIC,
    NULL,
    NULL
  );

  // Nu apelam wait_response(), deoarece nu exista butoane
  // si serverul nu tine conexiunea deschisa pentru raspuns.
  Serial.println("Cererea de notificare a fost trimisa.");
}

// -------------------- SETUP / LOOP --------------------

void setup()
{
  Serial.begin(115200);
  delay(100);

  connectWiFi();
}

void loop()
{
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  // Trimite notificarea o singura data dupa pornire.
  if (!notificationSent) {
    notificationSent = true;
    sendNotification();
  }

  delay(1000);
}
