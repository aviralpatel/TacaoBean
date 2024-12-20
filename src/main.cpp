#include <Arduino.h>
// wifi libraries
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <ESP8266WebServer.h>
// display libraries
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
// neopixel
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>

#include "morecode.h"
#include "webpage.h"

// Access Point
const char* ap_ssid = "TacaoBean";
const char* ap_password = "tacaobean";
ESP8266WebServer server(80);
void setupAP();
bool credentials_received = false;

// Network
String router_ssid = "";
String router_pass = "";
std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
HTTPClient https;

const char* message_url = "https:your/firebase/url_message.json";
const char* post_url = "https:your/firebase/url_post.json";
int message_id;
int last_message_id;
String payload("");
void fetch(const char*, bool);
void post(const char*, const char*);

// display 
// SCL= D1, SDA= D2
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
void quickPrint(const char* message, uint8_t x = 0, uint8_t y = 0);

// neopixel
#define LED_PIN D6
#define LED_N 2
Adafruit_NeoPixel Strip(LED_N, LED_PIN, NEO_RGB + NEO_KHZ800);
uint32_t C_PINK = Strip.Color(121, 255, 121); // g r b
uint32_t C_PURPLE = Strip.Color(121, 121, 255); 
bool strip_on = false;
uint8_t brightness_p = 20;
uint8_t brightness = 51;

// buttons
#define TOUCH_PIN D7
#define TYPE_BUTTON D5
#define SELECT_BUTTON D4 // boot fails if pulled low during boot

struct NormalButton{
  uint8_t pin;
  uint32_t delta = 0;
  uint32_t start;
  uint32_t duration;
  bool isPressed = false;
  bool run = false;
  NormalButton(uint8_t connect_pin){
    this->pin = connect_pin;
  }
};

struct TouchButton{
  uint8_t pin;
  uint32_t delta = 0;
  uint32_t start;
  uint32_t duration;
  bool isPressed = false;
  bool run = false;
  TouchButton(uint8_t connect_pin){
    this->pin = connect_pin;
  }
};

NormalButton typeButton(TYPE_BUTTON);
NormalButton selectButton(SELECT_BUTTON);
TouchButton touchButton(TOUCH_PIN);
void IRAM_ATTR handleTypeButton();
void IRAM_ATTR handleSelectButton();
void IRAM_ATTR handleTouchButton();
void handleLongTouchPress();
void handleShortTouchPress();

// runtime variables
uint8_t mode = 0;
uint32_t pooling_time = 30000; // pool every thirty seconds
uint32_t last_pool_time = 0;
String send_string("");
String hold_string("");

// eeprom
uint8_t eeprom_pointer = 0;
void writeStringToEEPROM(String &writeStr);
void readDataFromEEPROM();
void clearEEPROM();

void setup() {
  Serial.begin(115200);
  EEPROM.begin(512);
  // initialise display
  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)){
    Serial.println("display allocation failed");
    while(true){
      delay(100);
    }
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.setTextColor(WHITE);
  display.println("Booting...");
  display.display();
  delay(1000);

  // Connect to Network
  readDataFromEEPROM(); // get network parameters from eeprom
  client->setInsecure(); // bypass SSL
  WiFi.begin(router_ssid.c_str(), router_pass.c_str());
  Serial.print("Connecting to WiFi ..");
  display.clearDisplay();
  display.printf("Connecting to WiFi\nssid- %s\npass- %s\n", router_ssid.c_str(), router_pass.c_str());
  display.display();
  uint32_t wifi_start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
    if(millis() - wifi_start > 12000){
      Serial.println("Setting up Access Point");
      setupAP();
      break;
    }
  }
  if(WiFi.getMode() != WIFI_AP_STA){
    Serial.printf("\nwifi connected\n");
    quickPrint("Connected to Wifi");
    fetch(message_url, false);
  }
  // initialise user inputs
  pinMode(typeButton.pin, INPUT_PULLUP);
  pinMode(selectButton.pin, INPUT_PULLUP);
  pinMode(touchButton.pin, INPUT_PULLDOWN_16);
  attachInterrupt(touchButton.pin, handleTouchButton, CHANGE);
  attachInterrupt(typeButton.pin, handleTypeButton, CHANGE);
  attachInterrupt(selectButton.pin, handleSelectButton, CHANGE);

  // initialise neopixel
  Strip.begin();
  Strip.clear();
  Strip.setBrightness(0);
  Strip.show();

}

void loop() {
  if(WiFi.getMode() == WIFI_AP_STA){
    server.handleClient();
    if(credentials_received){
      Serial.println("Credentails Received, Restart Now");
      delay(1000);
    }
  }
  else{
    if(touchButton.run){
      Serial.printf("touch button pressed for duration- %d\n", touchButton.duration);
      if(touchButton.duration > 1200){
        handleLongTouchPress();
      }
      else{
        handleShortTouchPress();
      }
      touchButton.run = false;
    }
    

    if(millis() - last_pool_time > pooling_time){
    fetch(message_url, false);
    last_pool_time = millis();
    }

    switch (mode){
      case 0: // print the message
        quickPrint(payload.c_str(), 0, 0);
        break;

      case 1: // 
        if(typeButton.run){
          typeButton.run = false;
          if(brightness_p <= 90){
            brightness_p = brightness_p + 10;
          }
        }
        if(selectButton.run){
          selectButton.run = false;
          if(brightness_p >= 20){
            brightness_p = brightness_p - 10;
          }
        }
        brightness = map(brightness_p, 0, 100, 0, 255);
        if(strip_on){
          Strip.setBrightness(brightness);
          Strip.show();
        }
        display.clearDisplay();
        display.setCursor(0, 12);
        display.printf("Brightness - %d%%\n", brightness_p);
        display.setCursor(0, 24);
        display.printf("Status - %s\n", strip_on ? "ON" : "OFF");
        display.display();
        break;

      case 2:
        if(typeButton.run){
          typeButton.run = false;
          if(typeButton.duration > 300){
            hold_string = hold_string + "-";
          }
          else {
            hold_string = hold_string + ".";
          }
          Serial.println(hold_string);
        }
        if(selectButton.run){
          selectButton.run = false;
          if(selectButton.duration > 700){
            // upload the message
            quickPrint("message sent");
            String reqData("");
            reqData = "{\"node\": \"" + send_string + "\"}";
            post(post_url, reqData.c_str());
            send_string = "";
            delay(1000);
          }
          else{
            int index = -1;
            int length = sizeof(dot_dash)/sizeof(dot_dash[0]);
            const char* hold_string_c = hold_string.c_str();
            for(int i=0; i<length; i++){
              if(strcmp(hold_string_c, dot_dash[i]) == 0){
                index = i;
                break;
              }
            }
            if(index != -1){
              char letter = letters[index];
              Serial.printf("Match Found- %s, %c\n", hold_string_c, letter);
              send_string = send_string + letter;
            }
            else{
              Serial.println("No match found");
            }
          }
          hold_string = "";
        }
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println(hold_string);
        display.drawFastHLine(0, 9, 129, WHITE);
        display.setCursor(0, 12);
        display.println(send_string);
        display.display();
        break;     
    }
  }
}

void fetch(const char* url, bool isRef){
  const char* error = "ERROR WHILE FETCHING";
  if((WiFi.status() == WL_CONNECTED)){
    if(https.begin(*client, url)){
      int responseCode = https.GET();
      Serial.printf("Response Code= %d\n", responseCode);
      if(responseCode >= 200 && responseCode < 300){
        if(!isRef){
          payload = https.getString();
          payload.remove(0, 1);
          payload.remove(payload.length() - 1, 1);
          Serial.printf("Data Received- %s\n", payload.c_str());
        }
        else{
          message_id = https.getString().toInt();
          Serial.printf("ID Received- %d\n", message_id);
        }
      }
      else{
        Serial.printf("%s- %s\n", error, "Bad Response Code");
      }
      https.end();
    }
  }
  else{
    Serial.printf("%s- %s\n", error, "WiFi not Connected");
  }
}

void post(const char* url, const char* query){
  const char* error = "ERROR WHILE POSTING";
  if((WiFi.status() == WL_CONNECTED)){
    if(https.begin(*client, url)){
      https.addHeader("Content-Type", "application/json");
      int responseCode = https.PUT(query);
      Serial.printf("Response Code= %d\n", responseCode);
      https.end();
    }
  }
  else{
    Serial.printf("%s- %s\n", error, "WiFi not Connected");
  }
}

void IRAM_ATTR handleTypeButton(){
  if(millis() - typeButton.delta > 30){
    typeButton.delta = millis();
    if(!typeButton.isPressed){
      typeButton.isPressed = true;
      typeButton.start = millis();
    }
    else{
      typeButton.duration = millis() - typeButton.start;
      typeButton.run = true;
      typeButton.isPressed = false;
    }
  }
}

void IRAM_ATTR handleSelectButton(){
  if(millis() - selectButton.delta > 30){
    selectButton.delta = millis();
    if(!selectButton.isPressed){
      selectButton.isPressed = true;
      selectButton.start = millis();
    }
    else{
      selectButton.isPressed = false;
      selectButton.duration = millis() - selectButton.start;
      selectButton.run = true;
    }
  }
}

void IRAM_ATTR handleTouchButton(){
  if(millis() - touchButton.delta > 30){
    touchButton.delta = millis();
    if(!touchButton.isPressed){
      touchButton.isPressed = true;
      touchButton.start = millis();
    }
    else{
      touchButton.isPressed = false;
      touchButton.duration = millis() - touchButton.start;
      touchButton.run = true;
    }
  }
}

void handleLongTouchPress(){
  if(!strip_on){
    strip_on = true;
    Strip.setBrightness(brightness);
    Strip.setPixelColor(0, C_PINK);
    Strip.setPixelColor(1, C_PURPLE);
    Strip.show();
  }
  else{
    strip_on = false;
    Strip.setBrightness(0);
    Strip.show();
  }
}

void handleShortTouchPress(){
  if(mode < 2){
    mode = mode + 1;
    if(mode == 2){
      hold_string = "";
      send_string = "";
    }
  }
  else{
    mode = 0;
  }
  Serial.printf("mode- %d\n", mode);
}

void quickPrint(const char* message, uint8_t x, uint8_t y){
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(x, y);
  display.println(message);
  display.display();
}

void writeStringToEEPROM(String &writeStr){
  Serial.printf("Writing to EEPROM at address- %d\n", eeprom_pointer);
  byte len = writeStr.length();
  EEPROM.write(eeprom_pointer, len);
  eeprom_pointer = eeprom_pointer+1;
  for(int i=0; i<len; i++){
    EEPROM.write(eeprom_pointer, writeStr[i]);
    eeprom_pointer = eeprom_pointer + 1;
  }
  EEPROM.commit();
}

void readDataFromEEPROM(){
  uint8_t pointer = 0;
  byte ssid_len = EEPROM.read(pointer);
  char ssid_data[ssid_len + 1]; // +1 for null character
  pointer = pointer + 1;
  for(int i=0; i<ssid_len; i++){
    ssid_data[i] = EEPROM.read(pointer);
    pointer = pointer + 1; 
  }
  ssid_data[ssid_len] = '\0';
  router_ssid = String(ssid_data);
  byte pass_len = EEPROM.read(pointer);
  pointer = pointer + 1;
  char pass_data[pass_len + 1];
  for(int i=0; i<pass_len; i++){
    pass_data[i] = EEPROM.read(pointer);
    pointer = pointer + 1;
  }
  pass_data[pass_len] = '\0';
  router_pass = String(pass_data);
  Serial.printf("EEPROM Data\nSSID- %s,PASS- %s\n", router_ssid.c_str(), router_pass.c_str());
}

void clearEEPROM() {
  for (int i = 0; i < EEPROM.length(); i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
  Serial.println("EEPROM cleared");
}

void setupAP(){
  Serial.println("Setup AP running");
  WiFi.softAP(ap_ssid, ap_password);
  IPAddress IP = WiFi.softAPIP();

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.printf("Hotspot ON\n");
  display.printf("SSID- %s\n PSK- %s\nIP- 192.168.4.1\n", ap_ssid, ap_password);
  // display.printf("IP- %s", IP.toString().c_str());
  display.display();

  server.on("/", HTTP_GET, [&](){
    server.send(200, "text/html", index_html);
  });
  server.on("/cred", HTTP_GET, [&]() {
    if (server.hasArg("ssid") && server.hasArg("pass")) {
        String new_ssid = server.arg("ssid");
        String new_pass = server.arg("pass");
        clearEEPROM();
        writeStringToEEPROM(new_ssid);
        writeStringToEEPROM(new_pass);
        server.send(200, "text/plain", "Parameters received, Restart the device now");
        credentials_received = true;
        quickPrint("Restart the device");
    } else {
        server.send(400, "text/plain", "Missing parameters");
    }
  });
  server.begin();
  Serial.print("Wifi Mode- ");
  Serial.println(WiFi.getMode());
}