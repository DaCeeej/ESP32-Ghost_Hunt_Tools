/*
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleServer.cpp
    Ported to Arduino ESP32 by Evandro Copercini
    updates by chegewara
*/

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <Wire.h>
#include "DFRobot_OLED12864.h"
#include "OLEDDisplayUi.h"
#include "DFRobotDFPlayerMini.h"

#define ADC_BIT 4096
#define ADC_SECTION 5

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID        "fcb6b4f6-b45f-4a9b-912a-51ccc372beca"
#define CHARACTERISTIC_UUID "e5588c57-1fca-4f40-b7ba-c8c4faa8630f"

static BLECharacteristic *pCharacteristic;
int numOfDevices = 2;
int currentConnectedDevices = 0;
bool deviceAdded = false;
bool anyDeviceConnected = false;
bool allDevicesConnected = false;
boolean gameStarted = false;
int startDelay = 60; //in seconds
unsigned long endTimer;
String startTimer;
char *valToNotify;
boolean ghostNotified = false;
int frameRecovery;
boolean isHunt = false;
int huntMp3[] = {2, 3, 4, 5, 6, 7, 8, 9};
int huntTrackNum;
boolean huntTrackSelected = false;
int huntDelay;
int endHuntDelay;
int huntTimer;
int endHuntTimer;
boolean huntValuesSet = false;
boolean huntMp3Playing = false;
boolean huntOver = false;
int huntVol = 30;
DFRobotDFPlayerMini myDFPlayer;
void printDetail(uint8_t type, int value);

//Setup callbacks onConnect and onDisconnect
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    anyDeviceConnected = true;
    currentConnectedDevices++;
    deviceAdded = true;
    Serial.print("Number of Connected Devices:");
    Serial.println(currentConnectedDevices);
    BLEDevice::startAdvertising();
    if (currentConnectedDevices == numOfDevices){
      delay(5000); //wait to reconnect completely before stopping advertising
      allDevicesConnected = true;
      Serial.println("All devices are now connected");
      BLEDevice::stopAdvertising();
    }
  };
  void onDisconnect(BLEServer* pServer) {
    allDevicesConnected = false;
    BLEDevice::startAdvertising();
    currentConnectedDevices--;
    Serial.print("Number of Connected Devices:");
    Serial.println(currentConnectedDevices);
    if (currentConnectedDevices == 0){
      anyDeviceConnected = false;
    }
  }
};

DFRobot_OLED12864  display(0x3c);
OLEDDisplayUi ui(&display);

#ifdef __AVR__
const uint8_t pin_SPI_cs = 5, keyA = 3, keyB = 8;
#elif ((defined __ets__) || (defined ESP_PLATFORM))
const uint8_t pin_SPI_cs = D5, keyA = D3, keyB = D8;
#endif

const uint8_t  pin_analogKey = A0;

enum enum_key_analog {
  key_analog_no,
  key_analog_right,
  key_analog_center,
  key_analog_up,
  key_analog_left,
  key_analog_down,
} key_analog;

enum_key_analog read_key_analog(void)
{
  int adValue = analogRead(pin_analogKey);
  if(adValue > ADC_BIT * (ADC_SECTION * 2 - 1) / (ADC_SECTION * 2)) {
    return key_analog_no;
  }
  else if(adValue > ADC_BIT * (ADC_SECTION * 2 - 3) / (ADC_SECTION * 2)) {
    return key_analog_right;
  }
  else if(adValue > ADC_BIT * (ADC_SECTION * 2 - 5) / (ADC_SECTION * 2)) {
    return key_analog_center;
  }
  else if(adValue > ADC_BIT * (ADC_SECTION * 2 - 7) / (ADC_SECTION * 2)) {
    return key_analog_up;
  }
  else if(adValue > ADC_BIT * (ADC_SECTION * 2 - 9) / (ADC_SECTION * 2)) {
    return key_analog_left;
  }
  else {
    return key_analog_down;
  }
}
  char *ghostnum[] = {"01","02","03","04","05","06","07","08","09","10","11","12","13"};
  char *ghostnames[] = {"Demon","Poltergeist","Revenant","Banshee","Jinn","Mare","Oni","Shade","Wraith","Yurei","Start","End","Hunt"};
  int ghostindicator = 0;
  int totalghostqty = (sizeof(ghostnum)/sizeof(ghostnum[0]))-3;
  int huntrecovery = ghostindicator;
  bool hold = false;

String twoDigits(int digits)
{
  if(digits < 10) {
    String i = '0'+String(digits);
    return i;
  }
  else {
    return String(digits);
  }
}

void ghostnamedis (OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y)
{
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(0 + x,0 + y,"Tools Connected: "+String(currentConnectedDevices));
  
  display->setFont(ArialMT_Plain_10);
  display->drawString(0 + x, 22 + y, "Select Ghost:");
  display->setFont(ArialMT_Plain_24);
  display->drawString(0 + x, 38 + y, String(ghostnames[ghostindicator]));

  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(128 + x, 15 + y, "Start");
}

void huntison (OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y)
{
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_24);
  display->drawString(64 + x, 22 + y, "HUNT");

  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(128 + x, 50 + y, "End");
}

void startGameQuestion (OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y)
{ 
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(0 + x,0 + y,"Tools Connected: "+String(currentConnectedDevices));
  
  display->setFont(ArialMT_Plain_16);
  display->drawString(0 + x, 21 + y, "Start game");
  display->drawString(0 + x, 45 + y, "as "+String(ghostnames[ghostindicator])+"?");

  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(128 + x, 15 + y, "Yes");
  display->drawString(128 + x, 50 + y, "No");
}

void wait (OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y)
{
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_24);
  display->drawString(64 + x, 22 + y, "Wait");
}

void endGame (OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y)
{
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_16);
  display->drawString(10 + x, 22 + y, "End Game?");

  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(128 + x, 15 + y, "Yes");
  display->drawString(128 + x, 50 + y, "No");
}

void midGame (OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y)
{
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_24);
  display->drawString(64 + x, 22 + y, "HIDE");
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(128 + x, 50 + y, "End");
}

void startGame (OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y)
{
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_24);
  display->drawString(64 + x, 10 + y, "HIDE");
  display->setFont(ArialMT_Plain_16);
  display->drawString(64 + x, 40 + y, startTimer);
  
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(128 + x, 50 + y, "End");
}

FrameCallback frames[] = {ghostnamedis,huntison,startGameQuestion,wait,endGame,midGame,startGame};

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600);
  Serial.println("Starting BLE work!");
  pinMode(keyA, INPUT);
  pinMode(keyB, INPUT);
  valToNotify = ghostnum[(totalghostqty+1)];  //should be pointing to the "end" game event ghostname

  BLEDevice::init("GhostController");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID,
                                         BLECharacteristic::PROPERTY_NOTIFY |
                                         BLECharacteristic::PROPERTY_READ
                                       );


  pCharacteristic->setValue(valToNotify);
  pService->start();
  // BLEAdvertising *pAdvertising = pServer->getAdvertising();  // this still is working for backward compatibility
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  //pAdvertising->setScanResponse(true);
  //pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  //pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("Waiting a client connection to notify...");
  Serial.println(F("Initializing DFPlayer ... (May take 3~5 seconds)"));
  myDFPlayer.begin(Serial2);
  if (!myDFPlayer.begin(Serial2)) {  //Use softwareSerial to communicate with mp3.
    Serial.println(F("Unable to begin:"));
    Serial.println(F("1.Please recheck the connection!"));
    Serial.println(F("2.Please insert the SD card!"));
    while (true) {
      delay(0); // Code to compatible with ESP8266 watch dog.
    }
  }
  Serial.println(F("DFPlayer Mini online."));
  myDFPlayer.volume(huntVol);
  ui.disableAllIndicators();
  ui.disableAutoTransition();
  ui.setFrames(frames,7);
  // Initialising the UI will init the display too.
  ui.init();
  display.flipScreenVertically();
}

void loop() {
  //int ghostnumchange = ghostindicator;
  if (!hold && !gameStarted) {
    if (read_key_analog() == key_analog_up && ghostindicator < totalghostqty - 1
    || read_key_analog() == key_analog_right && ghostindicator < totalghostqty - 1) {
      ghostindicator++;
      huntrecovery = ghostindicator;
      hold = true;
      Serial.println(ghostnames[ghostindicator]);
    }
    else if (read_key_analog() == key_analog_up && ghostindicator == totalghostqty - 1
    || read_key_analog() == key_analog_right && ghostindicator == totalghostqty - 1){
      ghostindicator = 0;
      huntrecovery = ghostindicator;
      hold = true;
      Serial.println(ghostnames[ghostindicator]);
    }
    if (read_key_analog() == key_analog_down && ghostindicator > 0
    || read_key_analog() == key_analog_left && ghostindicator > 0) {
      ghostindicator--;
      huntrecovery = ghostindicator;
      hold = true;
      Serial.println(ghostnames[ghostindicator]);
    }
    else if (read_key_analog() == key_analog_down && ghostindicator == 0
    || read_key_analog() == key_analog_left && ghostindicator == 0){
      ghostindicator = totalghostqty - 1;
      huntrecovery = ghostindicator;
      hold = true;
      Serial.println(ghostnames[ghostindicator]);
    }  
    if (digitalRead(keyA) == 0) {
      ui.switchToFrame(2);
      delay(75);
      ui.update();
      hold = true;
      while (true) {
        //Serial.println("Entered while loop for starting game");
        if (digitalRead(keyB) != 0 && digitalRead(keyA) != 0 && read_key_analog() == key_analog_no) {
           hold = false;
        }
        if (digitalRead(keyA) == 0 && !hold) {
          gameStarted = true;
          hold = true;
          delay(25);
          ui.switchToFrame(3);
          delay(75);
          ui.update();
          delay(75);
          valToNotify = ghostnum[(totalghostqty)];
          pCharacteristic->setValue(valToNotify);
          pCharacteristic->notify();
          delay(3000);
          break;
        }
        if (digitalRead(keyB) == 0 && !hold) {
          ui.switchToFrame(0);
          hold = true;
          break;
        }
      }
    }
  }
   
  if (digitalRead(keyB) != 0 && digitalRead(keyA) != 0 && read_key_analog() == key_analog_no) {
    hold = false;
  }

  if (gameStarted) {
    if (!huntTrackSelected) {
      huntTrackNum = random(0,8);
      huntTrackSelected = true;
      Serial.print("Track Num: ");Serial.println(huntTrackNum);
    }
    if (!ghostNotified) {
      endTimer = millis() + (startDelay * 1000);
      Serial.print("End Timer = ");Serial.println(endTimer);
      Serial.print("Current Millis = ");Serial.println(millis());
      Serial.print("End Timer - Current Millis = ");Serial.println(int(endTimer) - int(millis()));
      ghostNotified = true;
      valToNotify = ghostnum[ghostindicator];
      pCharacteristic->setValue(valToNotify);
      pCharacteristic->notify();
      delay(25);
      ui.switchToFrame(6);
      frameRecovery = 6;
    }
    if ((int(endTimer) - int(millis())) > 0) {
      unsigned long timeRemaining = endTimer - millis();
      int minutes = int(floor(float(timeRemaining/60000)));
      int seconds = int(ceil(float((timeRemaining-(minutes*60000))/1000)));
      startTimer = String(twoDigits(minutes)+":"+twoDigits(seconds));
    }
    if ((int(endTimer) - int(millis())) < 0 && !isHunt) {
      //Serial.println("switching to frame 5");
      ui.switchToFrame(5);
      delay(75);
      ui.update();
      frameRecovery = 5;
      if (!huntValuesSet) {
        huntTimer = 5000 * random(3,9);
        huntDelay = 10000 * random(6,16);
        endHuntDelay = huntDelay + millis();
        endHuntTimer = huntDelay + huntTimer + millis();
        huntValuesSet = true;
      }
    }
    if ((int(endHuntDelay) - int(millis())) < 0 && (int(endTimer) - int(millis())) < 0 && !isHunt) {
      isHunt = true;
      valToNotify = ghostnum[(totalghostqty+2)];
      pCharacteristic->setValue(valToNotify);
      pCharacteristic->notify();
      delay(25);
    }
    if ((int(endHuntTimer) - int(millis())) < 0 && (int(endTimer) - int(millis())) < 0 && isHunt) {
      isHunt = false;
      huntValuesSet = false;
      valToNotify = ghostnum[ghostindicator];
      pCharacteristic->setValue(valToNotify);
      pCharacteristic->notify();
      delay(25);
    }
    if (isHunt) {
      ui.switchToFrame(1);
      frameRecovery = 1;
      if (!huntMp3Playing) {
        myDFPlayer.loop(huntMp3[huntTrackNum]);
        huntMp3Playing = true;
        huntOver = true;
      }
    }
    if (digitalRead(keyB) == 0 && !hold) {
      ui.switchToFrame(4);
      delay(75);
      ui.update();
      hold = true;
      while (true) {
        if (digitalRead(keyB) != 0 && digitalRead(keyA) != 0 && read_key_analog() == key_analog_no) {
           hold = false;
        }
        if (digitalRead(keyA) == 0 && !hold) {
          gameStarted = false;
          ghostNotified = false;
          isHunt = false;
          huntValuesSet = false;
          hold = true;
          huntTrackSelected = false;
          ui.switchToFrame(0);
          valToNotify = ghostnum[(totalghostqty+1)];
          pCharacteristic->setValue(valToNotify);
          pCharacteristic->notify();
          delay(50);
          break;
        }
        if (digitalRead(keyB) == 0 && !hold) {
          ui.switchToFrame(frameRecovery);
          delay(75);
          hold = true;
          break;
        }
      }
    }
  }

  if (huntOver && !isHunt) {
      myDFPlayer.stop();
      huntOver = false;
      huntMp3Playing = false;
    }
    
  ui.update();

  if (anyDeviceConnected && deviceAdded) {
    delay(2000);
    pCharacteristic->setValue(valToNotify);
    pCharacteristic->notify();
    deviceAdded = false;
    delay(25);
  }
}
