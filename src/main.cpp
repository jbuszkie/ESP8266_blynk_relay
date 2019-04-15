#include <Arduino.h>
#include <string.h>
#include "SimpleTimer.h"
#include <ESP8266WiFiMulti.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoOTA.h>
#include "DebugMacros.h"
#include "HTTPSRedirect.h"

void printTerminalAddress(DeviceAddress deviceAddress);
void printAddress(DeviceAddress deviceAddress);
void startWiFi();
void postData(String tag, float value);
void sendUptime();
String lookup_sensor(DeviceAddress address);

// comment out the boards you don't want in
#define TEST_DEV
//#define WEMOS02
//#define WEMOS01

//#ifndef WEMOS02
//#define WEMOS01
//#endif


#define ONE_WIRE_BUS 2  // DS18B20 pin
OneWire ds(ONE_WIRE_BUS);
DallasTemperature DS18B20(&ds);

// Create an array for DS18B20 sensor addresses
DeviceAddress  DS18B20ad[20];
DeviceAddress  tempDeviceAddress;

String DeviceName[20];



int numberOfDevices; // Number of temperature devices found

#define ESP8266_LED 5
#define ESP8266_LED2 0

#define BLYNK_PRINT Serial    // Comment this out to disable prints and save space
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>


int status = WL_IDLE_STATUS;

int state=0;

boolean blink=1;

int OTAcount = 0; // Keep track of dots printed.

float temp;
float temp1[10];
float temp2[10];

// You should get Auth Token in the Blynk App.
// Go to the Project Settings (nut icon).

//Dev1
//char auth[] = "82fc4e9e5794466297495518f37d6f74";

//Dev2
#ifdef TEST_DEV
char MyHost[] = "TEST_DEV";
char auth[] = "c259e3d6661048e59239501bb22c05ef";
#endif

#ifdef WEMOS01
//WEMOS01
char MyHost[] = "WEMOS_01";
char auth[] = "a10ed8dbf98c455782d1525a5c0a2411";
#endif


#ifdef WEMOS02
char MyHost[] = "WEMOS_02";
char auth[] = "c0b8987a084342cab3ee9472536436ca";
#endif

//google stuff
// The ID below comes from Google Sheets.
const char *GScriptId = "AKfycbxWd-rF0RYpHnspmeo5rU-lhbj8qwYQPRg3z2gKB7XYqXsx2Yne";

// Push data on this interval
const int dataPostDelay = 900000;     // 15 minutes = 15 * 60 * 1000

const char* host = "script.google.com";

const int httpsPort = 443;

// echo | openssl s_client -connect script.google.com:443 |& openssl x509 -fingerprint -noout
const char* fingerprint = "";
//const uint8_t fingerprint[20] = {};

// Write to Google Spreadsheet
String url = String("/macros/s/") + GScriptId + "/exec?";
// Fetch Google Calendar events for 1 week ahead
String url2 = String("/macros/s/") + GScriptId + "/exec?";
// Read from Google Spreadsheet
String url3 = String("/macros/s/") + GScriptId + "/exec?read";

String payload_base =  "{\"command\": \"appendRow\", \
                    \"sheet_name\": \"Sheet1\", \
                    \"values\": ";
String payload = "";

HTTPSRedirect* client = nullptr;
// used to store the values of free stack and heap
// before the HTTPSRedirect object is instantiated
// so that they can be written to Google sheets
// upon instantiation
unsigned int free_heap_before = 0;
unsigned int free_stack_before = 0;

String sensorName = "";

WidgetTerminal terminal(V20);

ESP8266WiFiMulti wifiMulti;    // Create an instance of the ESP8266WiFiMulti class, called 'wifiMulti'


SimpleTimer timer; // Create a Timer object called "timer"!

BLYNK_WRITE(V20)
{

  // if you type "Marco" into Terminal Widget - it will respond: "Polo:"
  if (String("Marco") == param.asStr()) {
    terminal.println("You said: 'Marco'") ;
    terminal.println("I said: 'Polo'") ;
  } else  if ((String("Reset") == param.asStr())||(String("reset") == param.asStr())) {
    terminal.println("You said: 'Reset'") ;
    terminal.println("I said: 'Trying to Reset'") ;
    ESP.restart();
  } else  if ((String("List") == param.asStr())||(String("list") == param.asStr())) {
    for(int i=0;i<DS18B20.getDeviceCount(); i++) {
      terminal.print("Device ");
      terminal.print(i,DEC);
      terminal.print(" - ");
      terminal.print(DeviceName[i]);
      terminal.print(" - ");
      printTerminalAddress(DS18B20ad[i]);
      terminal.println("");
    }

  } else {

    // Send it back
    terminal.print("You said:");
    terminal.write(param.getBuffer(), param.getLength());
    terminal.println();
  }

  // Ensure everything is sent
  terminal.flush();
}

BLYNK_WRITE(V4)
{
  //  Use a switch to turn off blinking LED.

  blink = (boolean)param.asInt();

}


void setup()
{
  Serial.begin(115200);
  pinMode(ESP8266_LED, OUTPUT);
  pinMode(ESP8266_LED2, OUTPUT);
  Serial.println();
  Serial.println("Before Begin");

  free_heap_before = ESP.getFreeHeap();
  free_stack_before = ESP.getFreeContStack();
  Serial.printf("Free heap: %u\n", free_heap_before);
  Serial.printf("Free stack: %u\n", free_stack_before);
  Serial.println();
  Serial.print("Connecting to wifi: ");


  startWiFi();                 // Start a Wi-Fi access point, and try to connect to some given access points. Then wait for either an AP or STA connection


  Blynk.config(auth);

  while (Blynk.connect() == false) {
    // Wait until connected
  }

  ArduinoOTA.setHostname(MyHost);
  ArduinoOTA.setPassword("123");

  ArduinoOTA.onStart([]() {
	OTAcount = 0;
    Serial.println("Start");
    terminal.println("Start");
    terminal.flush();
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
    terminal.println("\nEnd");
    terminal.flush();
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
	  //Serial.printf("OTAcount = %d\r\n",OTAcount);
	  if (OTAcount == 0) {
		  Serial.printf("\r\nProgress: ");
		  OTAcount++;
	  } else {
		  Serial.printf(".");
		  if (OTAcount == 19) {
			  Serial.printf(" %02u%% ", (progress / (total / 100)));
			  OTAcount = 0;
		  }
		  else OTAcount++;

	  }
    terminal.printf("Progress: %u%%\r", (progress / (total / 100)));
    terminal.flush();
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("OTA ready");

  timer.setInterval(2000L, sendUptime); //  Here you set interval (1sec) and which function to call

  // Use HTTPSRedirect class to create a new TLS connection
  client = new HTTPSRedirect(httpsPort);
  client->setInsecure();
  client->setPrintResponseBody(false);
  client->setContentTypeHeader("application/json");

  Serial.print("Connecting to ");
  Serial.println(host);

  // Try to connect for a maximum of 5 times
  bool flag = false;
  for (int i=0; i<5; i++){
    int retval = client->connect(host, httpsPort);
    if (retval == 1) {
       flag = true;
       Serial.println("Connection Status:"  + String(client->connected()));
       break;
    }
    else
      Serial.println("Connection failed. Retrying...");
  }



  if (!flag){
    Serial.print("Could not connect to server: ");
    Serial.println(host);
    Serial.println("Exiting...");
    return;
  }


  //initialize names
  for(int i=0; i<20; i++) DeviceName[i]="";



  DS18B20.begin();
  DS18B20.getAddress(DS18B20ad[0],0);
  DS18B20.getAddress(DS18B20ad[1],1);
  DS18B20.getAddress(DS18B20ad[2],2);
  DS18B20.getAddress(DS18B20ad[3],3);
  DS18B20.getAddress(DS18B20ad[4],4);
  DS18B20.getAddress(DS18B20ad[5],5);
  numberOfDevices = DS18B20.getDeviceCount();


  // locate devices on the bus
  Serial.print("Locating devices...");
  Serial.print("Found ");
  Serial.print(numberOfDevices, DEC);
  Serial.println();


   // Loop through each device, print out address
 for(int i=0;i<numberOfDevices; i++)
 {
   // Search the wire for address
   if(DS18B20.getAddress(tempDeviceAddress, i))
  {
    //DS18B20.getAddress(DS18B20ad[i], i);
    DS18B20ad[i] == tempDeviceAddress;
    Serial.print("Found device ");
    Serial.print(i, DEC);
    Serial.print(" with address: ");
    printAddress(DS18B20ad[i]);
    Serial.println();
    Serial.print("looking for name.. ");
    sensorName = lookup_sensor(DS18B20ad[i]);
    DeviceName[i] = sensorName;
    Serial.print("found: "+ sensorName);
    //Serial.print("Setting resolution to ");
    //Serial.println(TEMPERATURE_PRECISION, DEC);

// set the resolution to TEMPERATURE_PRECISION bit (Each Dallas/Maxim device is capable of several different resolutions)
    //sensors.setResolution(tempDeviceAddress, TEMPERATURE_PRECISION);

//     Serial.print("Resolution actually set to: ");
//    Serial.print(DS18B20.getResolution(tempDeviceAddress), DEC);
    Serial.println();
  }else{
    Serial.print("Found ghost device at ");
    Serial.print(i, DEC);
    Serial.print(" but could not detect address. Check power and cabling");
               delay(10000);
  }
 }

  delay(2000);
  Blynk.run();

  terminal.println("\r\n");
  terminal.println(MyHost);
  terminal.print("Connected to ");
  terminal.println(WiFi.SSID());             // Tell us what network we're connected to
  terminal.print("IP Address: ");
  terminal.println(WiFi.localIP());
  terminal.println(__FILE__);
  terminal.println(__DATE__);
  terminal.print("Locating devices...");
  terminal.print("Found ");
  terminal.print(DS18B20.getDeviceCount(), DEC);
  terminal.println(" Devices.");
  for(int i=0;i<DS18B20.getDeviceCount(); i++)
  {
    terminal.print("Device ");
    terminal.print(i,DEC);
    terminal.print(" - ");
    terminal.print(DeviceName[i]);
    terminal.print(" ");
    printTerminalAddress(DS18B20ad[i]);
    terminal.println("");
  }
  terminal.println(F("Blynk v" BLYNK_VERSION ": Device started"));
  terminal.println(F("-------------"));
  //terminal.println(F("Type 'Marco' and get a reply, or type"));
  //terminal.println(F("anything else and get it printed back."));
  terminal.flush();
  Blynk.run();

  for(int i=0;i<DS18B20.getDeviceCount(); i++) {

	  Blynk.setProperty(i, "label", DeviceName[i]);

  }
}
/*__________________________________________________________SETUP_FUNCTIONS__________________________________________________________*/

void startWiFi() { // Try to connect to some given access points. Then wait for a connection
  wifiMulti.addAP("cantwks", "AppleJuice");   // add Wi-Fi networks you want to connect to
  wifiMulti.addAP("Busky_Net", "Netcom1w");
  wifiMulti.addAP("Busky_Net2", "Netcom1w");

  Serial.println("Connecting");
  while (wifiMulti.run() != WL_CONNECTED) {  // Wait for the Wi-Fi to connect
    delay(250);
    Serial.print('.');
  }
  Serial.println("\r\n");
  Serial.println(MyHost);
  Serial.print("Connected to ");
  Serial.println(WiFi.SSID());             // Tell us what network we're connected to
  Serial.print("IP address:\t");
  Serial.print(WiFi.localIP());            // Send the IP address of the ESP8266 to the computer
  Serial.println("\r\n");
  Serial.println(__FILE__);
  Serial.println(__DATE__);
  Serial.println("\r\n");

}


void sendUptime()
{
  // This function sends Arduino up time every 1 second to Virtual Pin (V5)
  // In the app, Widget's reading frequency should be set to PUSH
  // You can send anything with any interval using this construction
  // Don't send more that 10 values per second
  uint i;

  DS18B20.requestTemperatures();
  int update = 0;
  for (i=0 ; i < DS18B20.getDeviceCount() ; i++) {
    temp1[i] = DS18B20.getTempCByIndex(i);
    //Check for -127C or -196.6F Error condition
    if (temp1[i] == -127)
      update = 0;
    else if (temp1[i] != temp2[i])
      update = 1;

  }
  //update=1;
//  #ifdef WEMOS02
//  Blynk.setProperty(V0, "label", "Tank Top");
//  #endif
//  #ifdef WEMOS01
//  Blynk.setProperty(V0, "label", "LWT");
//  #endif
  if (update)
  {
    int mytime = millis();
    Serial.print(mytime/1000);
    terminal.print(mytime/1000);
    for (i=0 ; i < DS18B20.getDeviceCount() ; i++)
    {
    	Serial.print(": Temp C: ");
//      Serial.print(" ");
      Serial.print(temp1[i]);
      Serial.print(" Temp F: ");
//      Serial.print(" ");
      Serial.print(DallasTemperature::toFahrenheit(temp1[i]));
      Serial.print(" ");
      temp2[i] = temp1[i];
      Blynk.virtualWrite(i, String(DallasTemperature::toFahrenheit(temp1[i])));
      //Blynk.virtualWrite(0, 5);
      terminal.print(" T");
      terminal.print((String)i);
      terminal.print(": ");
      terminal.print(String(DallasTemperature::toFahrenheit(temp1[i])));
      terminal.print("F " );
    }
    terminal.print(DallasTemperature::toFahrenheit(temp1[3])-DallasTemperature::toFahrenheit(temp1[2]));
#ifdef WEMOS02
    Blynk.virtualWrite(10, String(DallasTemperature::toFahrenheit(temp1[3])-DallasTemperature::toFahrenheit(temp1[2])));
#endif
#ifdef WEMOS01
    Blynk.virtualWrite(10, String(DallasTemperature::toFahrenheit(temp1[0])-DallasTemperature::toFahrenheit(temp1[2])));
#endif
    int update = 0;
    terminal.println();
    terminal.flush();
    Serial.println();
  }



//    temp1[0] = (float)random(220,260)/10;
//    temp1[1] = (float)random(220,260)/10;
//    temp1[2] = (float)random(220,260)/10;
//    Serial.print(DallasTemperature::toFahrenheit(temp1[0]));
//    Serial.print(" ");
//    Serial.print(DallasTemperature::toFahrenheit(temp1[1]));
//    Serial.print(" ");
//    Serial.print(DallasTemperature::toFahrenheit(temp1[2]));
//    Serial.print(" ");
  //Serial.print("Temperature: ");
  //Serial.println(temp);

  //Serial.println("Led ON!!");
  if (blink) {
    if (state == 0 ) {
      digitalWrite(ESP8266_LED, HIGH); // LED off
      digitalWrite(ESP8266_LED2, LOW); // LED on
      state = 1;
    } else {
      //Serial.println("Led OFF!!");
      digitalWrite(ESP8266_LED, LOW); // LED on
      digitalWrite(ESP8266_LED2, HIGH); // LED off
      state = 0;
    }
  } else {
    digitalWrite(ESP8266_LED, HIGH); // LED off
    digitalWrite(ESP8266_LED2, HIGH); // LED off
  }


//  Blynk.virtualWrite(0, DallasTemperature::toFahrenheit(temp1[0]));
//  Blynk.virtualWrite(1, DallasTemperature::toFahrenheit(temp1[1]));
//  Blynk.virtualWrite(2, DallasTemperature::toFahrenheit(temp1[2]));

  Blynk.virtualWrite(V5, millis() / 1000);
}

// function to print a device address
void printAddress(DeviceAddress deviceAddress)
{
 for (uint8_t i = 0; i < 8; i++)
 {
   if (deviceAddress[i] < 16) Serial.print("0");
   Serial.print(deviceAddress[i], HEX);
 }
}

// function to print a device address
void printTerminalAddress(DeviceAddress deviceAddress)
{
 for (uint8_t i = 0; i < 8; i++)
 {
   if (deviceAddress[i] < 16) terminal.print("0");
   terminal.print(deviceAddress[i], HEX);
 }
}


// This is the main method where data gets pushed to the Google sheet
void postData(String tag, float value){
    if (!client->connected()){
            Serial.println("Connecting to client again…");
            client->connect(host, httpsPort);
    }
    String payload = "tag=" + tag + "&value=" + String(value);
    String urlFinal = url + "tag=" + tag + "&value=" + String(value);
    client->GET(urlFinal, host);
//    Serial.println("url: "+ url);
//    Serial.println("host: "+ *host);
//    Serial.println("Jim Was Here");
//    Serial.println(host);
//    Serial.println("payload: "+ payload);

    Serial.println(client->getResponseBody());

}

String lookup_sensor(DeviceAddress deviceAddress) {

	String textAddress = "";
	String response = "";
	char buffer[8];

    if (!client->connected()){
            Serial.println("Connecting to client again…");
            client->connect(host, httpsPort);
    }
    for (uint8_t i = 0; i < 8; i++)
    {
      sprintf(buffer, "%02X",deviceAddress[i]);
      textAddress = textAddress + buffer;
    }

    String urlFinal = url + "read=" + textAddress;
    client->GET(urlFinal, host);
//    Serial.println(client->getResponseBody());
    response = client->getResponseBody();

    response.trim();
    return response;
}


void loop()
{
  byte i;
  byte present = 0;
  byte data[12];
  byte addr[8];
//  float temp;


  Blynk.run();
  timer.run();
  ArduinoOTA.handle();
  //Blynk.virtualWrite(2, DallasTemperature::toFahrenheit(temp));

}