#include "Simplepush.h"

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

// ------------------------------------------------------------
// Global HTTPS client
// Este folosit pentru POST + Wait:true.
// Nu mai exista WebSocket.
// ------------------------------------------------------------

static BearSSL::WiFiClientSecure EventClient;

// ------------------------------------------------------------
// Stare interna
// ------------------------------------------------------------

static bool EventHttpWaiting = false;

static String EventTaskId = "";
static String EventAppendToken = "";

// Textele butoanelor pentru cererea curenta.
// Sunt pastrate aici pentru a fi accesibile din wait_response().
static String EventButtonA = "";
static String EventButtonB = "";

// ------------------------------------------------------------
// Constructor / destructor
// ------------------------------------------------------------

Simplepush::Simplepush()
{
    fdb = "";
	timedOut = false;
	waitCallback = NULL;
}

Simplepush::~Simplepush()
{
    EventClient.stop();
    EventHttpWaiting = false;
}
void Simplepush::setWaitCallback(SimplepushWaitCallback callback) { waitCallback = callback; }
void Simplepush::stop()
{
    if (EventHttpWaiting) {
        Serial.println("[Simplepush] Stopped");

        EventClient.stop();
        EventHttpWaiting = false;
		timedOut = true;
    }
}
// ------------------------------------------------------------
// send()
// Pastreaza exact API-ul vechi
// ------------------------------------------------------------

void Simplepush::send(
    char *key,
    char *title,
    char *message,
    char *event,
    char *buta,
    char *butb
)
{
	timedOut = false;
    sendHttpPost(
        key,
        title,
        message,
        event,
        buta,
        butb
    );
}

// ------------------------------------------------------------
// sendHttpPost()
// API v1 Simplepush
//
// Wait: true = serverul tine conexiunea HTTP deschisa
// pana cand utilizatorul apasa unul dintre butoane.
//
// Nu mai folosim WebSocket.
// ------------------------------------------------------------

void Simplepush::sendHttpPost(
    char *key,
    char *title,
    char *message,
    char *event,
    char *buta,
    char *butb
)
{
    // Salvam etichetele butoanelor pentru wait_response().
    EventButtonA = (buta != NULL) ? String(buta) : "";
    EventButtonB = (butb != NULL) ? String(butb) : "";

    Serial.println("[Simplepush] Connecting to API v1...");
    Serial.print("[Simplepush] Free heap before POST TLS: ");
    Serial.println(ESP.getFreeHeap());

    // Inchidem orice conexiune precedenta.
    EventClient.stop();
    EventHttpWaiting = false;

    delay(100);

    // Pentru moment nu verificam certificatul.
    // Reduce memoria consumata pe ESP8266.
    EventClient.setInsecure();

    // Buffer-e mici pentru ESP8266.
    EventClient.setBufferSizes(512, 512);

    // Timeout scurt pentru operatiile de retea.
    EventClient.setTimeout(10);

    // TLS 1.2
    EventClient.setSSLVersion(BR_TLS12, BR_TLS12);

    if (!EventClient.connect("api.simplepu.sh", 443))
    {
        Serial.println("[Simplepush] TLS connection FAILED");

        EventClient.stop();
        EventHttpWaiting = false;

        return;
    }

    // --------------------------------------------------------
    // Action-Input
    // --------------------------------------------------------

String actionInput = "";

if (buta != NULL &&
    butb != NULL &&
    strlen(buta) > 0 &&
    strlen(butb) > 0)
{
    actionInput =
        String(buta) +
        "=" +
        String(buta) +
        "," +
        String(butb) +
        "=" +
        String(butb);

    Serial.print("[Simplepush] Action-Input: ");
    Serial.println(actionInput);
}

    // --------------------------------------------------------
    // HTTP POST
    // --------------------------------------------------------

    EventClient.print("POST /v1/tasks HTTP/1.1\r\n");
    EventClient.print("Host: api.simplepu.sh\r\n");

    EventClient.print("API-Token: ");
    EventClient.print(key);
    EventClient.print("\r\n");

    EventClient.print("Topic: ");
    EventClient.print(event);
    EventClient.print("\r\n");

    EventClient.print("Title: ");
    EventClient.print(title);
    EventClient.print("\r\n");

    EventClient.print("Content: ");
    EventClient.print(message);
    EventClient.print("\r\n");
	
	if (actionInput.length() > 0)
	{
		EventClient.print("Action-Input: ");
		EventClient.print(actionInput);
		EventClient.print("\r\n");
	}

    EventClient.print("Auto-Commit: true\r\n");
    EventClient.print("Shared: true\r\n");

	if (actionInput.length() > 0)
	{
    // IMPORTANT:
    // Serverul tine request-ul deschis pana apare raspunsul.
		EventClient.print("Wait: true\r\n");
	}
    // Ajuta la evitarea mentinerii unei conexiuni HTTP persistente.
    EventClient.print("Connection: close\r\n");

    EventClient.print("\r\n");

    Serial.println("[Simplepush] POST sent. Waiting for button...");

    // --------------------------------------------------------
    // Asteptam raspunsul HTTP initial.
    // Cu Wait:true serverul trimite header-ele imediat,
    // dar tine body-ul deschis pana la apasarea butonului.
    // --------------------------------------------------------

    unsigned long start = millis();

    while (!EventClient.available())
    {
        if (!EventClient.connected())
        {
            Serial.println(
                "[Simplepush] Connection closed before HTTP response"
            );

            EventClient.stop();
            EventHttpWaiting = false;

            return;
        }

        if (millis() - start > 15000)
        {
            Serial.println(
                "[Simplepush] HTTP response timeout"
            );

            EventClient.stop();
            EventHttpWaiting = false;

            return;
        }

        yield();
    }

    // --------------------------------------------------------
    // Citim prima linie HTTP
    // --------------------------------------------------------

    String statusLine = EventClient.readStringUntil('\n');
    statusLine.trim();

    Serial.print("[Simplepush] HTTP status: ");
    Serial.println(statusLine);

    if (!statusLine.startsWith("HTTP/1.1 200"))
    {
        Serial.println("[Simplepush] POST FAILED");

        // Citim eventualele header-e ramase, doar pentru debug.
        while (EventClient.connected() || EventClient.available())
        {
            if (EventClient.available())
            {
                String line = EventClient.readStringUntil('\n');
                line.trim();

                if (line.length() == 0)
                    break;
            }
            else
            {
                yield();
            }
        }

        EventClient.stop();
        EventHttpWaiting = false;

        return;
    }

    // --------------------------------------------------------
    // Citim header-ele HTTP
    // --------------------------------------------------------

    EventTaskId = "";
    EventAppendToken = "";

    while (true)
    {
        String header = EventClient.readStringUntil('\n');

        if (header.endsWith("\r"))
            header.remove(header.length() - 1);

        if (header.length() == 0)
            break;

        String lower = header;
        lower.toLowerCase();

        // x-task-id
        if (lower.startsWith("x-task-id:"))
        {
            int p = header.indexOf(':');

            if (p >= 0)
            {
                EventTaskId = header.substring(p + 1);
                EventTaskId.trim();

                Serial.print("[Simplepush] Task ID: ");
                Serial.println(EventTaskId);
            }
        }

        // x-append-token
        if (lower.startsWith("x-append-token:"))
        {
            int p = header.indexOf(':');

            if (p >= 0)
            {
                EventAppendToken = header.substring(p + 1);
                EventAppendToken.trim();

                Serial.println(
                    "[Simplepush] Append token received"
                );
            }
        }
    }

    // --------------------------------------------------------
    // IMPORTANT:
    // Nu inchidem EventClient aici.
    //
    // Body-ul HTTP ramane deschis deoarece avem Wait:true.
    // wait_response() va astepta raspunsul utilizatorului.
    // --------------------------------------------------------

    EventHttpWaiting = true;

    Serial.println(
        "[Simplepush] POST OK - waiting for answer..."
    );

    Serial.print(
        "[Simplepush] Free heap while waiting: "
    );
    Serial.println(ESP.getFreeHeap());
}

// ------------------------------------------------------------
// wait_response()
//
// Serverul Simplepush trimite raspunsul HTTP folosind
// Transfer-Encoding: chunked.
//
// Exemplu observat pe ESP:
//   chunk 1 byte
//   chunk 8 bytes
//
// Body-ul final poate ajunge fragmentat, de aceea verificam
// raspunsul incremental, folosind etichetele butoanelor configurate.
// ------------------------------------------------------------

String Simplepush::wait_response()
{
    //const unsigned long WAIT_TIMEOUT = 5UL * 60UL * 1000UL; 5min
	
	const unsigned long WAIT_TIMEOUT = 30UL * 1000UL;

	const unsigned long CALLBACK_INTERVAL = 10UL; 
	unsigned long start = millis(); 
	unsigned long lastCallback = millis();

    String response = "";

    response.reserve(32);

    Serial.println(
        "[Simplepush] Waiting for button response..."
    );

    // --------------------------------------------------------
    // Verificam daca avem o conexiune HTTP activa.
    // --------------------------------------------------------

    if (!EventHttpWaiting)
    {
        Serial.println(
            "[Simplepush] No pending request"
        );
		timedOut = true;
        return "";
    }
	auto serviceCallback = [&]() {
		if (!EventHttpWaiting) return false;
		if (waitCallback != NULL) { 
			unsigned long now = millis(); 
			if (now - lastCallback >= CALLBACK_INTERVAL) {
				lastCallback = now; 
				waitCallback(); 
				if (!EventHttpWaiting) return false;
			} 
		} 
		return true;
	};
    // --------------------------------------------------------
    // Citeste un CRLF-terminated line.
    //
    // Pentru chunk size:
    //   1\r\n
    //   8\r\n
    //   0\r\n
    // --------------------------------------------------------

    while (millis() - start < WAIT_TIMEOUT)
    {
        // ----------------------------------------------------
        // Asteptam sa apara date.
        // ----------------------------------------------------

        while (!EventClient.available())
        {
			if (!serviceCallback()) return "";
            if (!EventClient.connected())
            {
                Serial.println(
                    "[Simplepush] Connection closed"
                );

                EventClient.stop();
                EventHttpWaiting = false;
				timedOut = true;
                return "";
            }

            if (millis() - start >= WAIT_TIMEOUT)
            {
                Serial.println(
                    "[Simplepush] Button response timeout"
                );

                EventClient.stop();
                EventHttpWaiting = false;
				timedOut = true;
                return "";
            }

            yield();
        }

        // ----------------------------------------------------
        // Citim linia cu dimensiunea chunk-ului.
        // ----------------------------------------------------

        String chunkLine = EventClient.readStringUntil('\n');
        chunkLine.trim();

        if (chunkLine.length() == 0) {
			if (!serviceCallback()) return "";
            continue;
		}

        // ----------------------------------------------------
        // Chunk extensions:
        //
        // 8;foo=bar
        //
        // Ne intereseaza doar valoarea hex.
        // ----------------------------------------------------

        int semicolon = chunkLine.indexOf(';');

        if (semicolon >= 0)
            chunkLine = chunkLine.substring(0, semicolon);

        chunkLine.trim();

        // ----------------------------------------------------
        // Convertim dimensiunea din HEX.
        // ----------------------------------------------------

        unsigned long chunkSize = 0;

        for (unsigned int i = 0; i < chunkLine.length(); i++)
        {
            char c = chunkLine.charAt(i);

            chunkSize <<= 4;

            if (c >= '0' && c <= '9')
            {
                chunkSize += c - '0';
            }
            else if (c >= 'a' && c <= 'f')
            {
                chunkSize += c - 'a' + 10;
            }
            else if (c >= 'A' && c <= 'F')
            {
                chunkSize += c - 'A' + 10;
            }
            else
            {
                // Linie invalida.
                chunkSize = 0;
                break;
            }
        }

        Serial.print(
            "[Simplepush] HTTP chunk size: "
        );
        Serial.println(chunkSize);

        // ----------------------------------------------------
        // Chunk final.
        // ----------------------------------------------------

        if (chunkSize == 0)
        {
            // Consumam eventualele trailer headers.
            while (EventClient.connected())
            {
				if (!serviceCallback()) return "";
                if (!EventClient.available())
                {
                    yield();
                    break;
                }

                String trailer =
                    EventClient.readStringUntil('\n');

                trailer.trim();

                if (trailer.length() == 0)
                    break;
            }

            Serial.println(
                "[Simplepush] HTTP stream ended"
            );

            EventClient.stop();
            EventHttpWaiting = false;
			timedOut = true;
            return "";
        }

        // ----------------------------------------------------
        // Citim exact chunkSize bytes.
        // ----------------------------------------------------

        unsigned long remaining = chunkSize;

        while (remaining > 0)
        {
			if (!serviceCallback()) return "";
            if (EventClient.available())
            {
                size_t available =
                    EventClient.available();

                size_t toRead = available;

                if (toRead > remaining)
                    toRead = remaining;

                while (toRead > 0)
                {
                    char buffer[32];

                    size_t part = toRead;

                    if (part > sizeof(buffer))
                        part = sizeof(buffer);

                    size_t n =
                        EventClient.readBytes(
                            buffer,
                            part
                        );

                    if (n == 0)
                        break;

                    for (size_t i = 0; i < n; i++)
                    {
                        char c = buffer[i];

                        response += c;

                        // ------------------------------------------------
                        // Butonul poate fi impartit intre mai multe
                        // chunk-uri / packet-uri.
                        // ------------------------------------------------

                        // Verificam raspunsul fata de etichetele
                        // butoanelor trimise pentru aceasta notificare.
                        if (EventButtonA.length() > 0 &&
                            response.endsWith(EventButtonA))
                        {
                            Serial.print("[Simplepush] BUTTON: ");
                            Serial.println(EventButtonA);

                            fdb = EventButtonA;

                            EventClient.stop();
                            EventHttpWaiting = false;

                            Serial.print(
                                "[Simplepush] Free heap after response: "
                            );
                            Serial.println(ESP.getFreeHeap());

                            return fdb;
                        }

                        if (EventButtonB.length() > 0 &&
                            response.endsWith(EventButtonB))
                        {
                            Serial.print("[Simplepush] BUTTON: ");
                            Serial.println(EventButtonB);

                            fdb = EventButtonB;

                            EventClient.stop();
                            EventHttpWaiting = false;

                            Serial.print(
                                "[Simplepush] Free heap after response: "
                            );
                            Serial.println(ESP.getFreeHeap());

                            return fdb;
                        }
                    }

                    if (n > remaining)
                        remaining = 0;
                    else
                        remaining -= n;

                    if (n == 0)
                        break;

                    toRead -= n;
					if (!serviceCallback()) return "";
                }
            }
            else
            {
                if (!EventClient.connected())
                {
                    Serial.println(
                        "[Simplepush] Connection closed while reading chunk"
                    );

                    EventClient.stop();
                    EventHttpWaiting = false;
					timedOut = true;
                    return "";
                }

                if (millis() - start >= WAIT_TIMEOUT)
                {
                    Serial.println(
                        "[Simplepush] Button response timeout"
                    );

                    EventClient.stop();
                    EventHttpWaiting = false;
					timedOut = true;
                    return "";
                }
				if (!serviceCallback()) return "";
                yield();
            }
        }

        // ----------------------------------------------------
        // Dupa fiecare chunk trebuie consumat CRLF.
        // ----------------------------------------------------

        unsigned long crlfStart = millis();

        while (EventClient.available() < 2)
        {
			if (!serviceCallback()) return "";
            if (!EventClient.connected())
                break;

            if (millis() - crlfStart > 2000)
                break;

            yield();
        }

        if (EventClient.available())
        {
            char c1 = EventClient.read();

            if (EventClient.available())
            {
                char c2 = EventClient.read();

                // Normal:
                // c1 = '\r'
                // c2 = '\n'
                //
                // Nu este necesar sa raportam eroare daca serverul
                // foloseste alta fragmentare; important este ca
                // urmatoarea iteratie sa poata citi chunk-ul urmator.
                (void)c1;
                (void)c2;
            }
        }

        // ----------------------------------------------------
        // Limitam String-ul ca sa nu consumam inutil RAM.
        //
        // Nu avem nevoie de intregul raspuns; ne intereseaza
        // doar ultimele caractere.
        // ----------------------------------------------------

        if (response.length() > 32)
        {
            response.remove(
                0,
                response.length() - 16
            );
        }
		if (!serviceCallback()) return "";
        yield();
    }

    // --------------------------------------------------------
    // Timeout general
    // --------------------------------------------------------

    Serial.println(
        "[Simplepush] Button response timeout"
    );

    EventClient.stop();
    EventHttpWaiting = false;
	timedOut = true;
    return "";
}
