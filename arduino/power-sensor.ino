#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WIFIConfigurator.h>
#include <WiFiClientSecureBearSSL.h>
#include <EEPROM.h>
#include <ArduinoOTA.h>
#include <math.h>
#include <SHA256.h>

#define HASH_SIZE 32
#define BLOCK_SIZE 64
#define READ_INTERVAL 10000
#define NUM_SAMPLES 400

unsigned int lastRead;

//web form stuff
const String configLabels = "Account Number|Appliance Name";
char configData[300];
WIFIConfigurator configurator(configLabels);
int minReading = 1025;
int maxReading = -1;
String account;
String appliance;
SHA256 sha256;

std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
HTTPClient https;

void setup() {

  Serial.begin(115200);
  Serial.println();

  configurator.begin();
  
  EEPROM.begin(300);
  EEPROM.get(0, configData);
  
  Serial.println("reading config from EEPROM...");
  while (true) {
    char* ssid = strtok(configData, "|");
    if (ssid == NULL) break;
    char* wifipassword = strtok(NULL, "|");
    if (wifipassword == NULL) break;
    char* myhostname = strtok(NULL, "|");
    if (myhostname == NULL) break;
    char* account = strtok(NULL, "|");
    if (account == NULL) break;
    char* appliance = strtok(NULL, "|");
    if (appliance == NULL) break;
  }

  client->setInsecure();

  ArduinoOTA.setPassword((const char *)"alaskasensor"); 
  ArduinoOTA.begin();
}

void loop() {
  configurator.handleClient();  
  ArduinoOTA.handle();
  // wait for WiFi connection
  if ((WiFi.status() == WL_CONNECTED)) {
    unsigned int now = millis();
    if (now - lastRead > READ_INTERVAL) {
      sendData(getWatts());
      lastRead = now;
    }
  }
  //TODO: 1) report less frequently
  //      2) save data if Wifi is not connected
  //      3) modify endpoint to support list of data points


}

int getWatts() {
  unsigned int start = millis();
  minReading = 1025;
  maxReading = -1;
  for (int i=0; i<NUM_SAMPLES; i++) {
    int reading = analogRead(A0);
    if (reading < minReading) {
      minReading = reading;
    } else if (reading > maxReading) {
      maxReading = reading;
    }
  }
  Serial.print("min: ");
  Serial.print(minReading);
  Serial.print("  max: ");
  Serial.print(maxReading);
  Serial.print("  ampl: ");
  int ampl = maxReading - minReading;
  Serial.println(ampl);
  Serial.print("sample duration(ms): ");
  Serial.println(millis() - start);
  // nice resource: https://www.instructables.com/Arduino-Energy-Meter-V20/
  //sensor range: 0-1024 (acs712 module where ideally -30amp = 0, 30amp=1023)
  //Power = Irms * Vrms  (US household Vrms is 110V)
  //Irms = 1/2 peak-to-peak ampl * 1/(square root of 2)
  //therefore power = 60 * peak-to-peak ampl * 110 / (2 * 1024 * 1.4142) 
  //we are assuming load is resistance only (i.e. current is in phase with voltage)
  Serial.print("power: ");
  int power = int((ampl * 2.279) + 0.5); //rounding
  Serial.println(power);
  return power;
}


void sendData(int watts) {

  String macStr = WiFi.macAddress();
  //char macCharArr[65];
  //macStr.toCharArray(macCharArr,65);
  //Serial.print("hash of mac: ");
  //hash(macCharArr);
  //Serial.println(macCharArr);
  if ( https.begin(*client, "https://s9qiwdi5kd.execute-api.us-east-1.amazonaws.com/PowerTrackerLoad?mac="+macStr+"&wattmin="+String(watts) ) ) {  // HTTPS
    Serial.print("[HTTPS] GET...\n");
    // start connection and send HTTP header
    int httpCode = https.GET();
    
    // httpCode will be negative on error
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTPS] GET... code: %d\n", httpCode);
      // file found at server
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        Serial.println("response:");
        String payload = https.getString();
        Serial.println(payload);
      }
    } else {
      Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
    }
    https.end();
  } else {
    Serial.printf("[HTTPS] Unable to connect\n");
  }
}


void hash(char *data, uint8_t *hashval) {
  sha256.reset();
  sha256.update(data, strlen(data));
  sha256.finalize(hashval, HASH_SIZE); 
}

const char hexchars[] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F','\0'};
//hexval length should be 2*(data length) + 1
void tohex(uint8_t *data, char* hexval) {
  for (int i=0; i<HASH_SIZE; i++) {
    hexval[2*i] = hexchars[data[i] >> 4];
    hexval[2*i+1] = hexchars[data[i] & 15];
  }
  hexval[HASH_SIZE*2] = '\0';  
}
