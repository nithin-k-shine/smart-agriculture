#include "config.h"
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>   // Universal Telegram Bot Library written by Brian Lough: https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot
#include <MQTTClient.h>
#include <ArduinoJson.h>
#include "WiFi.h"

//Temperature sensor variables
#define ADC_VREF_mV 3300.0 // in millivolt
#define ADC_RESOLUTION 4096.0
#define PIN_LM35 34 // ESP32 pin GIOP36 (ADC0) connected to LM35
#define PUMPPIN 5
#define MOISTPIN 35
bool pumpst=false;

//Temperature variable
float tempC=0.0;

//Moisture variable
float soilmoist=0.0;

// The MQTT topics that this device should publish/subscribe
#define AWS_IOT_PUBLISH_TOPIC "farm/pub"

//AWS WiFi client
WiFiClientSecure net = WiFiClientSecure();
// Initialize MQTT client
MQTTClient client = MQTTClient(256);

// Initialize Telegram BOT
#define BOTtoken "xxxxxxxxxxxxxxxxxx"
//Telegram WiFi client
WiFiClientSecure client1;
// Initialize Telegram BOT
UniversalTelegramBot bot(BOTtoken, client1);

// BOT Checks for new messages every 1 second.
int botRequestDelay = 1000;
unsigned long lastTimeBotRan;

// Publishes message to AWS every 10s
int AWSpubdelay = 10000;
unsigned long LastTymMsgsent;

//Variable to store temperature,Moisture level and Pump status
String tmp;
String moist;
String pump;

// Handle what happens when you receive new messages in Telegram bot
void handleNewMessages(int numNewMessages) {
  Serial.println("handling New Messages....");
  Serial.println(String(numNewMessages));

  for (int i=0; i<numNewMessages; i++) {
    // Chat id of the requester
    String chat_id = String(bot.messages[i].chat_id);
    
    // Print the received message
    String text = bot.messages[i].text;
    Serial.println(text);

    String from_name = bot.messages[i].from_name;

    if (text == "/start") {
      String welcome = "Welcome, " + from_name + ".\n";
      welcome += "Use the following commands to control your outputs.\n\n";
      welcome += "/tmp to get the temperature \n";
      welcome += "/Soil to know the Soil Moisture level \n";
      welcome += "/pump to control the water pump \n";
      bot.sendMessage(chat_id, welcome, "");
    }
    
    if (text == "/tmp"){
      tmp="";
      tmp+=String(tempC);
      tmp+="C";
      bot.sendMessage(chat_id,tmp, "");
    }

    if (text == "/Soil") {
      moist="Moisture value:";
      moist+=String(soilmoist);
      bot.sendMessage(chat_id,moist, "");
    }

    if (text == "/pump") {
      if(pumpst==false){
        pump="Pump is OFF,click /PumpON to turn pump ON";
      }
      else if(pumpst==true){
        pump="Pump is ON,click /PumpOFF to turn pump OFF";
      }
      bot.sendMessage(chat_id,pump, "");
    }
    if(text=="/PumpON"){
      digitalWrite (PUMPPIN, HIGH);
      pumpst=true;
    }
    if(text=="/PumpOFF"){
      digitalWrite (PUMPPIN, LOW);
      pumpst=false;
    }
  }
}

#define BUFFER_LEN 256
char msg[BUFFER_LEN];

void connectAWS()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println("Connecting to Wi-Fi");

  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }

  // Configure WiFiClientSecure to use the AWS IoT device credentials
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);

  // Connect to the MQTT broker on the AWS endpoint we defined earlier
  client.begin(AWS_IOT_ENDPOINT, 8883, net);

  Serial.println("");
  Serial.println("Connecting to AWS IOT");

  while (!client.connect(THINGNAME)) {
    Serial.print(".");
    delay(100);
  }

  if(!client.connected()){
    Serial.println("AWS IoT Timeout!");
    return;
  }
  Serial.println("");
  Serial.println("AWS IoT Connected!");
}
 
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode (PUMPPIN, OUTPUT);
  Serial.begin(9600);
  client1.setCACert(TELEGRAM_CERTIFICATE_ROOT);// Add root certificate for api.telegram.org
  connectAWS();
}

void loop() {
  client.loop();
  //Temperature data
  // read the ADC value from the temperature sensor
  int adcVal = analogRead(PIN_LM35);
  int moistVal=analogRead(MOISTPIN);
  // convert the ADC value to voltage in millivolt
  float milliVolt = adcVal * (ADC_VREF_mV / ADC_RESOLUTION);
  // convert the voltage to the temperature in °C
  tempC = (milliVolt / 100);
  soilmoist=moistVal * (ADC_VREF_mV / ADC_RESOLUTION);
  //AWS - publishing data
  if (millis() - LastTymMsgsent > AWSpubdelay) {
    LastTymMsgsent = millis();
    //============================================================================
    String Farm_code = "PineappleFarm_VIT";
    String temperature="";
    temperature+=String(tempC);
    temperature+="C";
    String Moisture = String(soilmoist);
    String pumps="";
    if(pumpst){
      pumps+= "ON";
    }
    else{
      pumps+= "OFF";
    }
    
    snprintf (msg, BUFFER_LEN, "{\"Farm_code\" : \"%s\", \"Temperature Data\" : \"%s\", \"Soil Moisture Rate\" : \"%s\",\"WaterPump status\" : \"%s\"}", Farm_code.c_str(), temperature.c_str(), Moisture.c_str(),pumps.c_str());
    Serial.print("Publish message: ");
    Serial.println(msg);
    client.publish(AWS_IOT_PUBLISH_TOPIC, msg);
    //=============================================================================
    Serial.print("Heap: "); Serial.println(ESP.getFreeHeap()); //Low heap can cause problems
  }
  
  //Telegram Bot
  if (millis() > lastTimeBotRan + botRequestDelay){
      int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  
      while(numNewMessages) {
        Serial.println("got response");
        handleNewMessages(numNewMessages);
        numNewMessages = bot.getUpdates(bot.last_message_received + 1);
      }
      lastTimeBotRan = millis();
  }

  //LED Blinking
  digitalWrite(LED_BUILTIN, HIGH); 
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
  
  delay(1000);
}
