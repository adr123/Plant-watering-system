#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <String>

//Internet and telegram setup
const char* ssid = "";
const char* password = "";
String botToken = "";
WiFiClientSecure client;
UniversalTelegramBot bot(botToken, client);

//Global Variables
int relayControl;  //Relay port
int mSensor; //Moisture sensor port

int state; //Track the state of the pump (on or off)
int button; //Button's state
bool pressed; //Track if the button is being pressed or not

unsigned long cycleLength = 86400000UL * 5; // ms in a day, *5 
unsigned long timeSinceLast = 0; //
int sprayTrigger = 2500; //Moisture threshold before the pump is allowed to water the plant
int sprayTime = 15 * 1000; //Takes aroudn 2000 ms to get water up to tip. 15 seconds is heavy watering, should be enough for a basil plant
char waterer[20]; //Last person to water the plant


// Function to update or read the last person who watered the plant
String updateWaterers(bool read, String name = "") {
  if (read == true) {
    return String(waterer); //Only return the last waterer
  } else {
    strncpy(waterer, name.c_str(), 20 - 1); 
    waterer[20 - 1] = '\0';  
    Serial.println("New waterer written");
    return "";
  }
}

// Function to get or update the time since last watering
unsigned long getTimeSinceLast(bool read) {
  if (read == true) {
    return timeSinceLast;  // Return the last time watered (ms)
  } else {
    timeSinceLast = millis();  // Update the time
    return 0;  
  }
}

//Function to water the plant
void waterThePlant(String name) {
  updateWaterers(false, name); // Corrected function call
  digitalWrite(relayControl, HIGH); // Pump on
  Serial.println("ON!!!!");
  delay(sprayTime); // Watering duration. Takes about 2 seconds for water to peak
  digitalWrite(relayControl, LOW); // Pump off
  Serial.println("OFF!!!!");
  getTimeSinceLast(false);
}


void setup() {
  relayControl = 12; 
  mSensor = 34;

  pinMode(mSensor, INPUT);
  pinMode(relayControl, OUTPUT);
  digitalWrite(relayControl, HIGH);
  pinMode(27, INPUT);
  Serial.begin(9600);
  Serial.println("Initialized");
  digitalWrite(12, LOW); // Start NC
  state = 0;
  pressed = false;

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  client.setInsecure();
}

void loop() {
  int moistness = analogRead(mSensor);
  button = digitalRead(27);
  Serial.print("moistness:");
  Serial.println(moistness);

  //Telegram related functions
  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

  for (int i = 0; i < numNewMessages; i++) { //Runs per unread (arduino pov) message
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;
    String from_name = bot.messages[i].from_name;

    if (from_name == "") from_name = "Unknown";

    Serial.println("Received message from: " + from_name);
    Serial.println("Message: " + text);

    // Process Commands
    if (text.equalsIgnoreCase("water the plant")) {
      waterThePlant(from_name);
      bot.sendMessage(chat_id, "Plant has been watered!", "");
    } else if (text.equalsIgnoreCase("on")) { //Debugging functions
      digitalWrite(relayControl, HIGH); 
      Serial.println("ON!!!!");
      state = 0;
    } else if (text.equalsIgnoreCase("of")) { //Debugging functions
      digitalWrite(relayControl, LOW); 
      Serial.println("OFF!!!!");
      state = 1;
    } else if(text.indexOf("Modify period") >= 0) { //Modify period request
      int cmdIndex = text.indexOf("Modify period");
      int cmdLength = String("Modify period").length();
      String numberStr = text.substring(cmdIndex + cmdLength);
      numberStr.trim();
      unsigned long days = numberStr.toInt();
      cycleLength = days*86400000UL;
      bot.sendMessage(chat_id, "New period length is: " + String(cycleLength/86400000UL), "");
    } else if(text.indexOf("Modify spray") >= 0){ //Modify spray length request
      int cmdIndex = text.indexOf("Modify spray");
      int cmdLength = String("Modify spray").length();
      String numberStr = text.substring(cmdIndex + cmdLength);
      numberStr.trim();
      unsigned long spray = numberStr.toInt();
      sprayTime = spray*1000;
      bot.sendMessage(chat_id, "New spray duration is: " + String(sprayTime/1000), "");
    } else if(text.indexOf("Modify trigger") >= 0){ //Modify moisture trigger request
      int cmdIndex = text.indexOf("Modify trigger");
      int cmdLength = String("Modify trigger").length();
      String numberStr = text.substring(cmdIndex + cmdLength);
      numberStr.trim();
      unsigned long threshold = numberStr.toInt();
      sprayTime = threshold;
      bot.sendMessage(chat_id, "New spray duration is: " + String(sprayTime/1000), "");
    } else if (text.indexOf("Information") >= 0) { //Gives information about the plant and controller
      unsigned long elapsedTime = getTimeSinceLast(true);
      unsigned long remainingTime = (cycleLength > elapsedTime) ? (cycleLength - elapsedTime) : 0;
      int days = remainingTime / 86400000UL;
      bot.sendMessage(chat_id, "Current hydration level: " + String(moistness) +"\nHydration level should range between 2300 (wet) to 2800 (dry)\nNext water cycle is at 2500 and in " + String(days) + " days\nCurrent spray-time is: " + String(sprayTime/1000) + " seconds\nCurrent period length is: " + String(cycleLength/86400000UL) + " days", "");
      bot.sendMessage(chat_id, "Last user to water the plant was " + updateWaterers(true), "");
    } else {
      String welcome = "Hello, how can I serve you " + from_name + "?\n";
      welcome += "Just say...\n\n";
      welcome += "Water the plant\n";
      welcome += "Information\n";
      welcome += "Modify period (days)\n";
      welcome += "Modify spray (seconds)\n";
      welcome += "Modify trigger (moisture value)\n";

      welcome += "\n*For example: 'Modify period 5'*";
      bot.sendMessage(chat_id, welcome, "Markdown");
    }
  }

  if (moistness >= sprayTrigger && getTimeSinceLast(true) >= cycleLength) { //Automatic watering
    waterThePlant("computer");
  }

  if (button == 1 && pressed == false) { //Debugging using the physical button
    if (state == 0) { // Pump
      digitalWrite(relayControl, LOW);
      state = 1;
    } else if (state == 1) { // Don't pump
      digitalWrite(relayControl, HIGH);
      state = 0;
    }
    pressed = true;
  } else if (button == 0) {
    pressed = false;
  }
  Serial.println(state);
  Serial.print("button: ");
  Serial.println(button);
}
