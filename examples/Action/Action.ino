/*
  ESP8266 + Simplepush API v1
  Exemplu Yes / No cu wait callback.

  Biblioteca Simplepush trebuie sa contina:
    void setWaitCallback(SimplepushWaitCallback callback);
    void stop();
    String wait_response();

  Callback-ul este apelat periodic (la aproximativ 10 ms)
  din interiorul wait_response(), cat timp asteapta raspunsul
  Simplepush.

  Exemplul:
    - trimite o notificare cu butoanele Yes si No;
    - continua sa execute codul din waitCallback() in timpul asteptarii;
    - un buton fizic pe D5 poate anula asteptarea.
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "Simplepush.h"

// -------------------- CONFIGURARE --------------------

const char* WIFI_SSID     = "NUME_WIFI";
const char* WIFI_PASSWORD = "PAROLA_WIFI";

char API_TOKEN[] = "TOKEN_SIMPLEPUSH";
char TOPIC[]     = "esp8266-test";

char TITLE[]   = "Confirmare";
char MESSAGE[] = "Doresti sa continui?";

char BUTTON_YES[] = "Yes";
char BUTTON_NO[]  = "No";

// Buton fizic intre D5 si GND.
// Apasarea lui anuleaza asteptarea raspunsului Simplepush.
const uint8_t CANCEL_PIN = D5;

// -------------------- OBIECT SI STARE --------------------

Simplepush simplepush;

bool notificationSent = false;
bool cancelRequested = false;
bool callbackActive = false;

// -------------------- CALLBACK --------------------

// Functia trebuie sa fie void si fara parametri,
// conform typedef-ului SimplepushWaitCallback din biblioteca.
void simplepushWaitCallback()
{
  // Aici poti executa periodic alte verificari:
  // - citire senzori;
  // - verificare intrari;
  // - actualizare stari interne;
  // - verificare comenzi locale.

  // Exemplu: anulare asteptare la apasarea butonului fizic.
  if (digitalRead(CANCEL_PIN) == LOW) {
    if (!cancelRequested) {
      cancelRequested = true;
      Serial.println("[Callback] Anulare asteptare.");

      // Opreste conexiunea HTTP aflata in asteptare.
      simplepush.stop();
    }
  }

  // Nu folosi delay() aici. Callback-ul este apelat
  // din bucla de asteptare a raspunsului.
}

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

// -------------------- NOTIFICARE --------------------

void sendYesNoNotification()
{
  cancelRequested = false;

  Serial.println();
  Serial.println("Trimit notificarea Simplepush...");

  // Inregistram callback-ul inainte de a incepe asteptarea.
  simplepush.setWaitCallback(simplepushWaitCallback);

  simplepush.send(
    API_TOKEN,
    TITLE,
    MESSAGE,
    TOPIC,
    BUTTON_YES,
    BUTTON_NO
  );

  Serial.println("Astept raspunsul...");

  String response = simplepush.wait_response();

  if (response == "Yes") {
    Serial.println("Raspuns: YES");
    // Actiunea pentru YES.
  }
  else if (response == "No") {
    Serial.println("Raspuns: NO");
    // Actiunea pentru NO.
  }
  else if (cancelRequested) {
    Serial.println("Asteptarea a fost anulata de butonul fizic.");
  }
  else {
    Serial.println("Fara raspuns: timeout sau conexiune inchisa.");
  }

  // Optional: dezactivam callback-ul dupa terminarea asteptarii.
  simplepush.setWaitCallback(NULL);
}

// -------------------- SETUP / LOOP --------------------

void setup()
{
  Serial.begin(115200);
  delay(100);

  pinMode(CANCEL_PIN, INPUT_PULLUP);

  connectWiFi();
}

void loop()
{
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  // Exemplu: trimite o singura notificare dupa pornire.
  if (!notificationSent) {
    notificationSent = true;
    sendYesNoNotification();
  }

  // Codul normal al aplicatiei continua aici.
  delay(1000);
}
