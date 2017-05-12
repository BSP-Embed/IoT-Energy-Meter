/***********************************************************************************/
/* This is an simple application to illustrate IoT Home Automation.                */
/* This is an example for our Monochrome OLEDs based on SSD1306 drivers            */
/*  Pick one up today in the adafruit shop!                                        */ 
/*  ------> http://www.adafruit.com/category/63_98                                 */
/* This example is for a 128x64 size display using I2C to communicate              */
/* 3 pins are required to interface (2 I2C and one reset)                          */
/* Adafruit invests time and resources providing this open source code,            */
/* please support Adafruit and open-source hardware by purchasing                  */
/* products from Adafruit!                                                         */
/* Written by Limor Fried/Ladyada for Adafruit Industries.                         */
/* BSD license, check license.txt for more information                             */
/* All text above, and the splash screen must be included in any redistribution    */ 
/************************************************************************************/
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>            /* Local DNS Server used for redirecting all requests to the configuration portal */
#include <ESP8266WebServer.h>     /* Local WebServer used to serve the configuration portal */
#include <WiFiManager.h>          /* https://github.com/tzapu/WiFiManager WiFi Configuration Magic */

#define VOLTAGE_DIVIDER     10.0f
#define REF_VOLT            0.9f
#define RESOLUTION          1024
#define WATTS_THRES         25.0
#define AC_VOLT             230.0
#define VPP_RMS             0.3535
#define BASE_PRICE          125
#define UNITS_UPL_FREQ      30            /* In 2Sec */
#define THEFT_THRESHOLD     15
#define VperAmp             0.1f           /* See AC712 Datasheet */
#define TRUE                1
#define FALSE               0
#define OLED_RESET          4
#define SSD1306_LCDHEIGHT   64
Adafruit_SSD1306 display(OLED_RESET);

const char* ssid      = "IotEM";              /* Device SSID */
String apiKey         = "GBH1K3293KFNO8WY";   /* Replace with your thingspeak API key */
const char* server    = "api.thingspeak.com";

/* Create an instance of the client */
WiFiClient client;
WiFiManager wifiManager;

/* Port Pin Definition */
int InVolPin      = A0;
int LoadPin       = 14;
int PulsePin      = 12;
struct {
  unsigned char LdCon: 1;
  unsigned char Units:1;
} Flags;
 
double Voltage, VRMS, AmpsRMS, Watts;
volatile byte interruptCounter = 0;
int Pulses = 0;
int PrevUnits = 0;
int PrevMUnits = 0;
int Units, MeasUnits;

#if (SSD1306_LCDHEIGHT != 64)
  #error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

#define   LoadOn()     digitalWrite(LoadPin, 1)
#define   LoadOff()    digitalWrite(LoadPin, 0);

static void DispInfo      (void);
static void DispStat      (void);
static void SendUnits     (void);
static void SendTheftInfo (void) ;
static void SendSMS       (int8_t Type);
static void DisplayUnits(void);
static void TheftOccurred (void);

ADC_MODE(ADC_TOUT);

void setup(void)   {         
  Wire.begin(0,2);                            
  Serial.begin(9600);
  pinMode(InVolPin, INPUT);
  pinMode(LoadPin, OUTPUT);
  pinMode(PulsePin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PulsePin), handleInterrupt, FALLING);
  Flags.LdCon = FALSE;
  display.begin(SSD1306_SWITCHCAPVCC, 0x3c);  
  display.display();
  delay(500);
  LoadOn();
  ConnectAP();
  DisplayUnits();
}

void loop() {
  static unsigned long i = 0, j = 0, l = 0;
  VRMS = getVPP() *  VPP_RMS;
  AmpsRMS = VRMS / VperAmp;
  Watts = AmpsRMS * AC_VOLT;
   if (Watts >= WATTS_THRES)
       Flags.LdCon = TRUE;
    else
      Flags.LdCon = FALSE;
  #ifdef DEBUG
    Serial.print(Watts);
    Serial.print(AmpsRMS);
    Serial.println(" Amps RMS"); 
  #endif
 if (Flags.LdCon) { 
   #ifdef DEBUG
      Serial.println(MeasUnits);
      Serial.println(Pulses);
      Serial.println(l); 
   #endif
  if (MeasUnits == Pulses)
    if(++l > THEFT_THRESHOLD)   TheftOccurred();
 }
 if (i++ >= UNITS_UPL_FREQ) {   /* End of Day */
    Units = Pulses - PrevUnits;
    PrevUnits = Pulses;
    SendUnits();
    i = 0;
  }
  if (interruptCounter > 0) {
      interruptCounter--;
      Pulses++;
      MeasUnits = Pulses;
      l = 0;
      #ifdef DEBUG
        Serial.print("Total Units: ");
        Serial.println(Pulses);
      #endif
      DisplayUnits();
  }
  delay(1000);
}
void handleInterrupt() {
  interruptCounter++;
}
static void TheftOccurred(void) {
  display.clearDisplay(); 
  display.setCursor(0,0);                  
  display.setTextSize(3);
  display.setTextColor(WHITE);
  display.print("!THEFT!");
  display.display();
  SendTheftInfo();
  delay(5000);
  LoadOff();
  display.clearDisplay(); 
  display.setCursor(0,0);           
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.print(" Contact  CESCOM");
  display.display();
  delay(2000);
  ESP.deepSleep(0, WAKE_RF_DEFAULT); /* RIP */
  for(;;);
}
void DisplayUnits(void) {
  display.clearDisplay(); 
  display.setCursor(0,0);                  
  display.setTextSize(3);
  display.setTextColor(WHITE);
  display.print(Pulses);
  display.setCursor(90,13);
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.print("Kwh");
  display.display();
}
void ConnectAP(void) {
  #ifdef DEBUG
    Serial.print("Connecting Wifi: ");
    Serial.println(ssid);
  #endif
  display.clearDisplay();                   /* For Display */
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("Connecting");
  display.display();
  wifiManager.autoConnect(ssid);
  #ifdef DEBUG
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    IPAddress ip = WiFi.localIP();
    Serial.println(ip);
  #endif
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("Connected");
  display.display();
  delay(1000);
} 
void SendTheftInfo(void) {
   if (client.connect(server,80)) {  
    String postStr = apiKey;
           postStr +="&field2=";
           postStr += String(1); 
           postStr += "\r\n\r\n";
     client.print("POST /update HTTP/1.1\n"); 
     client.print("Host: api.thingspeak.com\n"); 
     client.print("Connection: close\n"); 
     client.print("X-THINGSPEAKAPIKEY: "+apiKey+"\n"); 
     client.print("Content-Type: application/x-www-form-urlencoded\n"); 
     client.print("Content-Length: "); 
     client.print(postStr.length()); 
     client.print("\n\n"); 
     client.print(postStr);
  }
 client.stop();
}
void SendUnits(void) {
   if (client.connect(server,80)) {  
    String postStr = apiKey;
           postStr +="&field1=";
           postStr += String(Units);
           postStr += "\r\n\r\n";
     client.print("POST /update HTTP/1.1\n"); 
     client.print("Host: api.thingspeak.com\n"); 
     client.print("Connection: close\n"); 
     client.print("X-THINGSPEAKAPIKEY: "+apiKey+"\n"); 
     client.print("Content-Type: application/x-www-form-urlencoded\n"); 
     client.print("Content-Length: "); 
     client.print(postStr.length()); 
     client.print("\n\n"); 
     client.print(postStr);
  }
 client.stop();
}
float getVPP() {
  float result;
  int readValue;             //value read from the sensor
  int maxValue = 0;          // store max value here
  int minValue = 1024;          // store min value here
  uint32_t start_time = millis();
   while((millis()-start_time) < 1000) //sample for 1 Sec
   {
       readValue = analogRead(InVolPin);
       // see if you have a new maxValue
       if (readValue > maxValue) 
       {
           /*record the maximum sensor value*/
           maxValue = readValue;
       }
       if (readValue < minValue) 
       {
           /*record the minimum sensor value*/
           minValue = readValue;
       }
   }
   // Subtract min from max
   result = (((maxValue - minValue) * REF_VOLT) / RESOLUTION) * VOLTAGE_DIVIDER ;
   return result;
}
