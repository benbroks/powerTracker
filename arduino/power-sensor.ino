#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <EEPROM.h>
#include <ArduinoOTA.h>
#include <SHA256.h>
#include <WiFiManager.h>

#define HASH_SIZE 32
#define READ_INTERVAL 10000
#define NUM_SAMPLES 400

unsigned int lastRead;

//web form stuff
ESP8266WebServer server(80);

int minReading = 1025;
int maxReading = -1;
String account;
String appliance;
String mac;
SHA256 sha256;
int noloadval;
int voltage;
uint8_t hashval[32];
int32_t deviceId = -1;

std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
HTTPClient https;

void setup() {

  Serial.begin(115200);
  Serial.println();
  delay(2000);
  EEPROM.begin(16);
  
  //get persisted values
  getNoLoadVal();
  getDeviceId();
  getVoltage();
  
  Serial.print("noloadval: ");
  Serial.println(noloadval);
  Serial.print("device ID: ");
  Serial.println(deviceId);
  Serial.print("voltage: ");
  Serial.println(voltage);
  
  
  //Setup WIFI login using WiFiManager lib
  WiFi.mode(WIFI_STA);

  mac = WiFi.macAddress();
  char devName[17] = "powersensor-";
  //set devName suffix to last four hex chars of MAC
  devName[12] = mac.charAt(12);
  devName[13] = mac.charAt(13);
  devName[14] = mac.charAt(15);
  devName[15] = mac.charAt(16);
  devName[16] = '\0';
  
  WiFi.hostname(devName);
  WiFiManager wm;
  if(!wm.autoConnect(devName)) {
    Serial.println("Failed to connect");
  } else {
    server.on("/", HTTP_GET, mainPage);
    server.on("/register", HTTP_POST, handleRegister);
    server.on("/calibrate", HTTP_GET, handleCalibrate);
    server.on("/setvoltage", HTTP_POST, handleSetVoltage);
    server.begin();                           
    Serial.println("HTTP server started");       
  }
  
  client->setInsecure();

  ArduinoOTA.setPassword((const char *)"alaskasensor"); 
  ArduinoOTA.begin();

}

void loop() {
  server.handleClient();
  ArduinoOTA.handle();
  if (deviceId > 0 && noloadval > 0 && (WiFi.status() == WL_CONNECTED)) {
    unsigned int now = millis();
    if (now - lastRead > READ_INTERVAL) {
      sendData(getWatts());
      lastRead = now;
    }
  }
  //TODO: 1) get average of samples and report less frequently?
  //      2) save data if Wifi is not connected
  //      3) modify endpoint to support list of data points

}

void mainPage() {
  String html((char *)0);
  html.reserve(1500); //to minimize allocations
  html += "<html>"
          "<head><style>"
            "p,form{font-size:20px;}"
            "hr {border-top: 3px solid #bbb;}"
          "</style></head>"
          "<body><h1>Power Sensor Setup</h1><hr/>";
  if (noloadval > 0) {
    html += "<p>Your Device is calibrated.  To recalibrate, click <a href=\"/calibrate\">here</a></p>";
  } else {
    html += "<p>Before you begin measuring power, please calibrate the device.  To do this, ensure the power sensor is plugged in and nothing is plugged into the sensor.  Then click <a href=\"/calibrate\">here</a>.</p>";
  }
  html += "<hr/>"
          "<p>For more accurate power readings, meaure your household RMS Voltage and update it if it's different than the current value.</p>"
          "<p><form method=\"post\" action=\"/setvoltage\">"
          "Household Vrms: <input type=\"text\" name=\"voltage\"/ value=\"";
  html += String(voltage);
  html += "\"><input type=\"submit\" value=\"Set Voltage\" name=\"setvoltage\">"
          "</form></p><hr/>";  
  if (deviceId > 0) {
    html += "<p>Your device is registered.  If for some reason power data is not being recorded, please register again.</p>";
  } else {
    html += "<p>After calibration, please register your device by entering the information below.  Power data cannot be reported until the device is registerd.</p>";
  }
  html += "<p><form method=\"post\" action=\"/register\">"
          "Name of appliance or device you are measuring power usage: <input type=\"text\" name=\"appliance\"/><br/>"
          "Electric Utility Account Number: <input type=\"text\" name=\"account\"/><br/>"
          "<input type=\"submit\" value=\"Register\" name=\"Register\">"
          "</form></p><hr/>"
          "<p>Current reading: ";
  html += String(getWatts());
  html += " watts</p><p>No-load calibration: ";
  html += String(noloadval);
  html += "</p></body></html";
  
  server.send(200, "text/html", html);
}

void handleCalibrate() {
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
  noloadval = int( (minReading + maxReading ) / 2);
  Serial.print("noloadval: ");
  Serial.println(noloadval);
  persistNoLoadVal();
  server.sendHeader("Location","/");
  server.send(303);
}

void handleSetVoltage() {
  Serial.println("handleVoltage()");
  if (server.arg(0).toInt() >= 50 && server.arg(0).toInt() <= 260) {
    voltage = server.arg(0).toInt();
    Serial.print("entered voltage:");
    Serial.println(voltage);
    setVoltage();
    server.sendHeader("Location","/");
    server.send(303);
  } else {
    server.send(406, "text/plain", "Please enter a voltage 50 through 260.");
  }
}

void handleRegister() {
  //TODO: 1) send mac addr, appliance name, and billing account to registry endpoint
  //      2) receive unique ID from registry and save to EEPROM
  deviceId = 10; //dummy ID for now
  setDeviceId();
  server.sendHeader("Location","/");
  server.send(303);
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
  //sensor range: 0 = -30Amp, (2 * noloadval) = +30Amp (noloadval is set during calibration -> should be close to 512)
  //Power = Irms * Vrms  (US household Vrms is ~120V)
  //Irms = 1/2 peak-to-peak ampl * 1/(square root of 2)
  //therefore power = 60 * peak-to-peak ampl * 110 / (2 * (2 * noloadval) * 1.4142) 
  //we are assuming load is resistance only (i.e. current is in phase with voltage)
  Serial.print("power: ");
  int power = int( (60 * ampl * voltage) / (2 * (2 * noloadval) * 1.4142) + 0.5); //rounding
  Serial.println(power);
  return power;
}

void sendData(int watts) {

  String macStr = WiFi.macAddress();

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


void hash(char *data, uint8_t *hashval, int hashSize) {
  sha256.reset();
  sha256.update(data, strlen(data));
  sha256.finalize(hashval, hashSize); 
}

const char hexchars[] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F','\0'};
//hexval length should be 2*(data length) + 1
void tohex(uint8_t *data, char* hexVal, int hashSize) {
  for (int i=0; i<hashSize; i++) {
    hexVal[2*i] = hexchars[data[i] >> 4]; //num of first 4 bits
    hexVal[2*i+1] = hexchars[data[i] & 15]; //num of last 4 bits
  }
  hexVal[hashSize*2] = '\0';  
}

void getNoLoadVal() {
   if (EEPROM.read(0) == 0 && EEPROM.read(1) == 1) {  
     noloadval = (EEPROM.read(2) << 8) + EEPROM.read(3);
   } else {
     noloadval = -1;
   }
}

void persistNoLoadVal() {
  //bytes 0 and 1 are set to 0 and 1 to indicate data is good
  EEPROM.write(0, 0);
  EEPROM.write(1, 1);

  //persist val
  EEPROM.write(2, noloadval >> 8);
  EEPROM.write(3, noloadval & 0xFF);
  EEPROM.commit();
}

void getDeviceId() {
  deviceId = getInt(4, -1);
}

void setDeviceId() {
  persistInt(4, deviceId);
}

void getVoltage() {
  voltage = getInt(10, 120);
}

void setVoltage() {
  persistInt(10, voltage);
}

int getInt(int startAddr, int defVal) {
   if (EEPROM.read(startAddr) == 0 && EEPROM.read(startAddr+1) == 1) {  
     return ((EEPROM.read(startAddr+2) << 24) + (EEPROM.read(startAddr+3) << 16) + (EEPROM.read(startAddr+4) << 8) + EEPROM.read(startAddr+5));
   } else {
     return defVal;
   }
}

void persistInt(int startAddr, int val) {
  //bytes n and n+1 are set to 0 and 1 to indicate data is good
  EEPROM.write(startAddr, 0);
  EEPROM.write(startAddr+1, 1);

  //persist val
  EEPROM.write(startAddr+2, val >> 24);
  EEPROM.write(startAddr+3, (val >> 16) & 0xFF);
  EEPROM.write(startAddr+4, (val >> 8) & 0xFF);
  EEPROM.write(startAddr+5, val & 0xFF);
  EEPROM.commit();
}
