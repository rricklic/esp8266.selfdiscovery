#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

////////////////////////////////////////////////////////////////////////////////
//                                    Notes
//
// Board: NodeMCU 1.0 ESP8266-12E
// LED: GPIO2 (Onboard LED)
//
// Access Point:
//   Default IP address: 192.168.4.1
//   SSID: ESP8266
//   Password: fake1234
//   Channel: 6
//
// EEPROM:
//   0-31: SSID
//   32-95: Password
////////////////////////////////////////////////////////////////////////////////

#define DEFAULT_SSID "ESP8266"
#define DEFAULT_PASSWORD "fake1234" //must be at least 8 characters
#define DEFAULT_CHANNEL 6

#define EEPROM_SIZE 512
#define EEPROM_MIN 0
#define EEPROM_SSID_MIN 0
#define EEPROM_SSID_MAX 31
#define EEPROM_PASSWORD_MIN 32
#define EEPROM_PASSWORD_MAX 95
#define EEPROM_MAX 95

#define LED_PIN 2 //GPIO2

ESP8266WebServer server(80);

String content;
String availableNetworks;
int statusCode;

////////////////////////////////////////////////////////////////////////////////
void setup() 
{
   EEPROM.begin(EEPROM_SIZE);
   WiFi.disconnect();

   Serial.begin(115200);
   Serial.println();

   pinMode(LED_PIN, OUTPUT);
   digitalWrite(LED_PIN, LOW);
    
   String eepromSSID = readEEPROM(EEPROM_SSID_MIN, EEPROM_SSID_MAX);
   String eepromPassword = readEEPROM(EEPROM_PASSWORD_MIN, EEPROM_PASSWORD_MAX);

   if(eepromSSID.length()) {
      setupStation(eepromSSID, eepromPassword);
   }
   else {
      setupAccessPoint();
   }
}

////////////////////////////////////////////////////////////////////////////////
void loop()
{
   server.handleClient();
}

////////////////////////////////////////////////////////////////////////////////
String scanAvailableNetworks()
{
   WiFi.mode(WIFI_STA);
   WiFi.disconnect();

   String result = "";
   Serial.print("Scanning for available networks...");
   int numNetworks = WiFi.scanNetworks();
   Serial.println(" done");
   if(numNetworks == 0) {
      result = "No networks found";
   }
   else {
      Serial.print(numNetworks);
      Serial.println(" networks found");

      result += numNetworks;
      result += " networks found<br/>";
      for(int index = 0; index < numNetworks; ++index) {
         Serial.println(WiFi.SSID(index));
         result += (index + 1); 
         result += ": ";
         result += WiFi.SSID(index);
         result += " (";
         result += WiFi.RSSI(index);
         result += ")";
         result += (WiFi.encryptionType(index) == ENC_TYPE_NONE) ? " <br/>" : "<br/>";
         //TODO: use select html element
         delay(10);
      }
   }

   Serial.println("RESULTS= " + result);
   return result;
}

////////////////////////////////////////////////////////////////////////////////
void setupAccessPoint()
{
   availableNetworks = scanAvailableNetworks();
   
   boolean result = WiFi.softAP(DEFAULT_SSID, DEFAULT_PASSWORD, DEFAULT_CHANNEL);
   if(result) {
      setupAccessPointServer();
   }
   else {
      Serial.println("Failed to setup as access point server");
   }

   flashLed(10, 100, 100);
}

////////////////////////////////////////////////////////////////////////////////
void setupAccessPointServer()
{
   Serial.print("Setting ESP8266 as access point server...");
   server.on("/", []() {
      IPAddress ip = WiFi.softAPIP();
      String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
      content = "<!DOCTYPE HTML>\r\n<html>Hello from ESP8266 (access point) at ";
      content += ipStr;
      content += "<p>";
      content += "</p><form method='get' action='setting'><label>SSID: </label><input type='text' name='ssid' length=32><input type='password' name='pass' length=64><input type='submit'></form>";
      content += availableNetworks;
      content += "</html>";
      server.send(200, "text/html", content);  
   });
   server.on("/setting", []() {
      String ssid = server.arg("ssid");
      String password = server.arg("pass");
      if(ssid.length() > 0 && password.length() > 0) {
         Serial.println("Clearing EEPROM...");
         for(int index = EEPROM_MIN; index <= EEPROM_MAX; ++index) { EEPROM.write(index, 0); }

         Serial.println("Writing ssid to EEPROM...");
         writeEEPROM(ssid, EEPROM_SSID_MIN, EEPROM_SSID_MAX);

         Serial.println("Writing password to EEPROM..."); 
         writeEEPROM(password, EEPROM_PASSWORD_MIN, EEPROM_PASSWORD_MAX);

         EEPROM.commit();
         EEPROM.end();
         
         content = "{\"Success\": \"saved network credentials to EEPROM... setting up as station server\"}";
         statusCode = 200;
         server.send(200, "text/html", content);
         delay(5000);

         server.stop();
         server.close();
         WiFi.disconnect();
         ESP.restart();
      } 
      else {
         content = "{\"Error\": \"404 not found\"}";
         statusCode = 404;
         Serial.println("Sending 404");
      }
      server.send(statusCode, "application/json", content);
   });  

   server.begin();
   Serial.println(" done");
}

////////////////////////////////////////////////////////////////////////////////
String readEEPROM(int startIndex, int endIndex)
{
   char c;
   String string = "";
   for(int index = startIndex; index <= endIndex; ++index) {
      c = char(EEPROM.read(index));
      if(c == 0) { break; }
      string += c;
   }
   return string;
}

////////////////////////////////////////////////////////////////////////////////
void writeEEPROM(String string, int startIndex, int endIndex)
{
   for(int index = startIndex; index-startIndex < string.length() && index < endIndex; ++index) {
      EEPROM.write(index, string[index-startIndex]);
   }
}

////////////////////////////////////////////////////////////////////////////////
void setupStation(String ssid, String password)
{
   WiFi.begin(ssid.c_str(), password.c_str());
   if(testWifi()) {
      setupStationServer();
      flashLed(3, 500, 100);
   }
   else {
      setupAccessPoint();
   }
}

////////////////////////////////////////////////////////////////////////////////
bool testWifi()
{
   int tries = 0;
   Serial.println("Testing Wifi connection...");  
   while(tries < 15) {
      if(WiFi.status() == WL_CONNECTED) { 
         Serial.println(" - connected"); 
         return true; 
      }
      delay(1000);
      Serial.print(WiFi.status());    
      tries++;
   }
   Serial.println("");
   Serial.println("Connection timed out");
   return false;
}

////////////////////////////////////////////////////////////////////////////////
void setupStationServer()
{
   Serial.print("Setting ESP8266 as station server...");
   //Index
   server.on("/", []() {
      IPAddress ip = WiFi.localIP();
      String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
      content = "<!DOCTYPE HTML>\r\n<html>Hello from ESP8266 (station) at ";
      content += ipStr;
      content += "<p>";
      content += "</p><form method='get' action='cleareeprom'><input type='submit' value = 'Clear EEPROM'></form>";
      content += "</html>";
      server.send(200, "text/html", content);      
   });
   //Clear EEPROM
   server.on("/cleareeprom", []() {      
      Serial.println("clearing eeprom");
      for(int i = EEPROM_MIN; i <= EEPROM_MAX; ++i) { EEPROM.write(i, 0); }
      EEPROM.commit();
      EEPROM.end();

      content = "{\"Success\": \"Cleared EEPROM... restarting\"}";
      statusCode = 200;
      server.send(200, "text/html", content);
      delay(5000);

      server.stop();
      server.close();
      WiFi.disconnect();
      ESP.restart();
    });
    server.begin();
    Serial.println(" done");
}

////////////////////////////////////////////////////////////////////////////////
void flashLed(int times, int onDelay, int offDelay)
{
   for(int i = 0; i < times; i++) {
      digitalWrite(LED_PIN, HIGH);
      delay(onDelay);
      digitalWrite(LED_PIN, LOW);
      delay(offDelay);
   }
}
