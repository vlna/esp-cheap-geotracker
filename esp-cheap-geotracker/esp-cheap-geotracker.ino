/*  Copyright (C) 2018 Vladimir Navrat

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
    
    As additional permission under GNU GPL version 3 section 7, you
    may distribute non-source (e.g., minimized or compacted) forms of
    that code without the copy of the GNU GPL normally required by
    section 4, provided you include this license notice and a URL
    through which recipients can access the Corresponding Source.
*/

#ifdef ARDUINO_ARCH_SAMD
#include <WiFi101.h>
#elif defined ARDUINO_ARCH_ESP8266
#include <ESP8266WiFi.h>
#elif defined ARDUINO_ARCH_ESP32
#include <WiFi.h>
#else
#error Wrong platform
#endif

#include <WifiLocation.h>

//known networks
const int known = 2;
const String knownSSID[] = {"network1", "wlan2"};
const String knownPassword[] = {"pass123", "password234"};

//Google API key
const char* googleApiKey = "";

WifiLocation location(googleApiKey);

#include <ESP8266HTTPClient.h>
HTTPClient http;

#include <WiFiClientSecure.h>
#include <TelegramBot.h>

// Initialize Telegram BOT
const char BotToken[] = "";
const char ChatID[] = ""; //use @my_id_bot to get this number

WiFiClientSecure net_ssl;
TelegramBot bot (BotToken, net_ssl);

char message[500];
char seznamurl[500];

void setup() {
  delay(100);
  Serial.begin(9600);
  // Serial.setDebugOutput(true);

  WiFi.persistent(false);

#ifdef ARDUINO_ARCH_ESP32
  WiFi.mode(WIFI_MODE_STA);
#endif
#ifdef ARDUINO_ARCH_ESP8266
  WiFi.mode(WIFI_STA);
#endif
}

void loop() {
  if (connectFreeWifi()) {
    if (internetAccess()) {
      Serial.println("Internet works!");

      location_t loc = location.getGeoFromWiFi();

      Serial.println("Location request data");
      Serial.println(location.getSurroundingWiFiJson());
      Serial.println("Latitude: " + String(loc.lat, 7));
      Serial.println("Longitude: " + String(loc.lon, 7));
      Serial.println("Accuracy: " + String(loc.accuracy));

      Serial.println("Reading WiFi SSID");

      String at = WiFi.SSID();

      Serial.println("Constructing Telegram message");

      sprintf(message, "%s%f%s%f +- %d @ %s", loc.lat < 0 ? "S" : "N", loc.lat < 0 ? -loc.lat : loc.lat, loc.lon < 0 ? "W" : "E", loc.lon < 0 ? -loc.lon : loc.lon, loc.accuracy, at.c_str());

      Serial.println("Constructing Telegram message - seznamurl");

      sprintf(seznamurl, "%s%s%f%s%f", "https://mapy.cz/zakladni?q=", loc.lat < 0 ? "S" : "N", loc.lat < 0 ? -loc.lat : loc.lat, loc.lon < 0 ? "W" : "E", loc.lon < 0 ? -loc.lon : loc.lon);

      Serial.println(message);
      Serial.println(seznamurl);


      bot.begin();
      //    bot.sendMessage(ChatID, String(message) + " @ " + at);
      bot.sendMessage(ChatID, message);
      bot.sendMessage(ChatID, seznamurl);
      //    bot.sendMessage(ChatID, googleurl);

      delay(30000);

    } else {
      Serial.println("Internet access blocked!");
    }
  } else {
    Serial.println("No free/known WiFi or unable to connect.");
  }

  WiFi.disconnect();
  delay(5000);
}


boolean connectFreeWifi() {
  // WiFi.scanNetworks will return the number of networks found
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0) {
    Serial.println("no networks found");
    return false;
  } else {
    Serial.print(n);
    Serial.println(" networks found");

    //find strongest

    int32_t bestRSSI = -1000;
    int best = -1;

    int32_t bestKnownRSSI = -1000;
    int bestKnown = -1;
    String password = "";

    for (int i = 0; i < n; ++i) {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");

      if (WiFi.encryptionType(i) == ENC_TYPE_NONE && WiFi.RSSI(i) > bestRSSI ) {
        best = i;
        bestRSSI = WiFi.RSSI(i);
      }

      if (WiFi.encryptionType(i) != ENC_TYPE_NONE && WiFi.RSSI(i) > bestKnownRSSI ) {
        for (int j = 0; j < known; ++j) {
          if (WiFi.SSID(i) == knownSSID[j]) {
            bestKnown = i;
            bestKnownRSSI = WiFi.RSSI(i);
            password = knownPassword[j];
          }
        }
      }
    }

    if (best >= 0) {
      Serial.print("Connecting to: ");
      Serial.println(WiFi.SSID(best));
      WiFi.begin(WiFi.SSID(best).c_str(), "");
      int timer = 50; //wait 10 second
      while (WiFi.status() != WL_CONNECTED && timer > 0) {
        delay(200);
        Serial.print(".");
        timer--;
      }
      Serial.println();
      if (WiFi.status() == WL_CONNECTED) {
        return true;
      }
    }
    if (bestKnown >= 0) {
      Serial.print("Connecting to: ");
      Serial.println(WiFi.SSID(bestKnown));
      WiFi.begin(WiFi.SSID(bestKnown).c_str(), password.c_str());
      int timer = 50; //wait 10 second
      while (WiFi.status() != WL_CONNECTED && timer > 0) {
        delay(200);
        Serial.print(".");
        timer--;
      }
      Serial.println();
      if (WiFi.status() == WL_CONNECTED) {
        return true;
      }
      return false;
    }
  }
}

//test access to internet (check captive portal)
//    NCSI performs a DNS lookup on www.msftncsi.com, then requests http://www.msftncsi.com/ncsi.txt. This file is a plain-text file and contains only the text Microsoft NCSI.
//    NCSI sends a DNS lookup request for dns.msftncsi.com. This DNS address should resolve to 131.107.255.255. If the address does not match, then it is assumed that the internet connection is not functioning correctly.

boolean internetAccess() {

  http.begin("http://www.msftncsi.com/ncsi.txt");
  int httpCode = http.GET();

  // file found at server
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    //check payload content here
    if (payload == F("Microsoft NCSI")) {
      return true;
    } else {
      return false;
    }
  } else {
    return false;
  }
}

boolean sendMessage(String location) {
  //  bot.begin();
  //  bot.sendMessage(chat_id, location);
}
