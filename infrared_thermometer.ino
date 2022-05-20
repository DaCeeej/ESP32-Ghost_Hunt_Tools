
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <Wire.h>
#include "DFRobot_OLED12864.h"
#include "OLEDDisplayUi.h"

#define ADC_BIT 4096
#define ADC_SECTION 5

// The remote service we wish to connect to.
static BLEUUID serviceUUID("fcb6b4f6-b45f-4a9b-912a-51ccc372beca");
// The characteristic of the remote service we are interested in.
static BLEUUID    charUUID("e5588c57-1fca-4f40-b7ba-c8c4faa8630f");

static boolean doConnect = false;
static boolean connected = false;

static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLEAdvertisedDevice* myDevice;

const uint8_t notificationOn[] = {0x1, 0x0};
const uint8_t notificationOff[] = {0x0, 0x0};

char* ghostnumChar;//this is the value of the notified characteristic from the ghost server
int scanTime = 0; //in seconds
boolean newghostnum = false;
char* freezingGhosts[] = {"01","02","04","05","08","10"};//these are the ghosts that will trigger emf level 5 events
char* startNum[] = {"11"};
char* endNum[] = {"12"};
char* huntNum[] = {"13"};
boolean isFreezing = false;
boolean hunt = false;
boolean huntReturn = true;
boolean isStart = false;
boolean isEnd = false;
int numFreezingGhosts = sizeof(freezingGhosts)/sizeof(freezingGhosts[0]);
char* freezingGhostCheck;
int freezingGhostCheckVal = 0;
int rssi = -500;
int rssiRecentSize = 10;
int rssiRecent[10] = {0,0,0,0,0,0,0,0,0,0}; //array size must equal rssiRecentSize
int rssiCounter = 0;
int rssiAvg;
int rssiRecentTotal;
int rssiThresholdFar = -86;
int rssiThresholdClose = -75;
int rssiFloor = -95;
int rssiTimer = 100;//time between RSSI reads
int rssiLastTime = 0;
boolean seekRSSI = false;
BLEClient* pClient = BLEDevice::createClient();
float farTemp = 15.0;
float closeTemp = 5.0;
float closerTemp = 0.1;
float freezingTemp = -5.0;
float newTemp = 15.0;
float tempRandomizer = 0.5;
int tempDelay = 2000;
unsigned long lastTempChange = 0;
int startDelay = 60; //in seconds
unsigned long endTimer;
String startTimer;
boolean uiTempFrame = false;
String displayTemp = "##.#";

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

String twoDigits(int digits)
{
  if(digits < 10 && digits >= 0) {
    String i = '0'+String(digits);
    return i;
  }
  else {
    return String(digits);
  }
}

void tempDisplay (OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y)
{
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_24);
  display->drawString(64 + x, 5 + y, "Temp:");

  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->drawString(99 + x, 33 + y, displayTemp + " C");

  display->setFont(ArialMT_Plain_10);
  display->drawString(82 + x, 33 + y, "o");
}

void startGameDelay (OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y)
{
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_24);
  display->drawString(64 + x, 10 + y, "Wait");
  display->setFont(ArialMT_Plain_16);
  display->drawString(64 + x, 40 + y, startTimer);
}

void gameOver (OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y)
{
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_24);
  display->drawString(64 + x, 5 + y, "Game");
  display->drawString(64 + x, 33 + y, "Over");
}

void gameInitiated (OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y)
{
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_24);
  display->drawString(64 + x, 5 + y, "Game");
  display->drawString(64 + x, 33 + y, "Initiated");
}

FrameCallback frames[] = {tempDisplay,startGameDelay,gameOver,gameInitiated};

static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
    seekRSSI = false;
    ghostnumChar = (char*)pData;
    newghostnum = true;
}

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pClient) {
  }

  void onDisconnect(BLEClient* pClient) {
    connected = false;
    seekRSSI = false;
    Serial.println("onDisconnect");
  }
};

bool connectToServer() {
    Serial.print("Forming a connection to ");
    Serial.println(myDevice->getAddress().toString().c_str());
    
    Serial.println(" - Created client");

    pClient->setClientCallbacks(new MyClientCallback());

    // Connect to the remove BLE Server.
    pClient->connect(myDevice);  // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
    Serial.println(" - Connected to server");

    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
      Serial.print("Failed to find our service UUID: ");
      Serial.println(serviceUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    Serial.println(" - Found our service");


    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
    if (pRemoteCharacteristic == nullptr) {
      Serial.print("Failed to find our characteristic UUID: ");
      Serial.println(charUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    Serial.println(" - Found our characteristic");

    if(pRemoteCharacteristic->canNotify()){
      pRemoteCharacteristic->registerForNotify(notifyCallback);
    connected = true;
    //Serial.println("Connected = True");
    return true;
    }
}

/**
 * Scan for BLE servers and find the first one that advertises the service we are looking for.
 */
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
 /**
   * Called for each advertising BLE server.
   */
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("BLE Advertised Device found: ");
    Serial.println(advertisedDevice.toString().c_str());

    // We have found a device, let us now see if it contains the service we are looking for.
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {
      //rssi = advertisedDevice.getRSSI();
      //Serial.print("RSSI: ");Serial.println(rssi);
      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;

    } // Found our server
  } // onResult
}; // MyAdvertisedDeviceCallbacks

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  BLEDevice::init("");
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
  pBLEScan->setActiveScan(true);
  pinMode(keyA, INPUT);
  pinMode(keyB, INPUT);
  ui.disableAllIndicators();
  ui.disableAutoTransition();
  ui.setFrames(frames,4);
  ui.init();
  display.invertDisplay();
  display.flipScreenVertically();
  ui.switchToFrame(0);
  delay(60);
  ui.update();
  Serial.println("Setup Complete");
}

void loop() {
  if (doConnect == true) {
    if (connectToServer()) {
      Serial.println("We are now connected to the BLE Server.");
      pRemoteCharacteristic->writeValue((uint8_t*)notificationOn, 2, true);
    } else {
      Serial.println("We have failed to connect to the server; there is nothin more we will do.");
    }
    doConnect = false;
  }

  if (connected == false){
    BLEDevice::getScan()->start(scanTime,false);
    rssiLastTime = millis();
  }

  if (seekRSSI && rssiTimer < (millis()-rssiLastTime) && connected) {
    rssi = pClient->getRssi();
    rssiLastTime = millis();
    if (rssi < rssiFloor) {
      rssi = rssiFloor;
    }
    rssiRecent[rssiCounter] = rssi;
    rssiCounter++;
    //Serial.print("Latest RSSI: ");Serial.println(rssi);
    for (int j = 0; j < rssiRecentSize; j++) {
      rssiRecentTotal = rssiRecentTotal + rssiRecent[j];
    }
    rssiAvg = rssiRecentTotal/rssiRecentSize;
    //Serial.print("Average RSSI: ");Serial.println(rssiAvg);
    if (rssiCounter >= rssiRecentSize) {
      rssiCounter = 0;
    }
    rssiRecentTotal = 0;
    //Serial.print("RSSI Value: ");Serial.println(rssi);
  }

  if(newghostnum) {
    if (strcmp(huntNum[0],ghostnumChar) == 0) {
      hunt = true;
      huntReturn = true;
    }
    else {
      hunt = false;
    }
    if (strcmp(startNum[0],ghostnumChar) == 0) {
      isStart = true;
    }
    else {
      isStart = false;
    }
    if (strcmp(endNum[0],ghostnumChar) == 0) {
      isEnd = true;
      seekRSSI = false;
      huntReturn = false;
    }
    else {
      isEnd = false;
    }
    if (!huntReturn && !isStart && !isEnd) {
      freezingGhostCheckVal = 0;
      for (int j = 0; j < numFreezingGhosts; j++) {
        freezingGhostCheck = freezingGhosts[j];
        if (strcmp(freezingGhostCheck, ghostnumChar) == 0) {
          freezingGhostCheckVal++;
        }
      }
      if (freezingGhostCheckVal > 0) {
        isFreezing = true;
        seekRSSI = true;
        endTimer = millis() + (startDelay * 1000);
        ui.switchToFrame(1);
      }
      else {
        isFreezing = false;
        seekRSSI = true;
        endTimer = millis() + (startDelay * 1000);
        ui.switchToFrame(1);
      }
    }
    if (!hunt && huntReturn && !isStart && !isEnd) {
      freezingGhostCheckVal = 0;
      for (int j = 0; j < numFreezingGhosts; j++) {
        freezingGhostCheck = freezingGhosts[j];
        if (strcmp(freezingGhostCheck, ghostnumChar) == 0) {
          freezingGhostCheckVal++;
        }
      }
      if (freezingGhostCheckVal > 0) {
        isFreezing = true;
        seekRSSI = true;
        ui.switchToFrame(0);
      }
      else {
        isFreezing = false;
        seekRSSI = true;
        ui.switchToFrame(0);
      }
    }
    newghostnum = false;
    Serial.print("Ghost Value: ");Serial.println(ghostnumChar);
    delay(75);
  }

  if ((int(endTimer) - int(millis())) > 0) {
    unsigned long timeRemaining = endTimer - millis();
    int minutes = int(floor(float(timeRemaining/60000)));
    int seconds = int(ceil(float((timeRemaining-(minutes*60000))/1000)));
    startTimer = String(twoDigits(minutes)+":"+twoDigits(seconds));
  }

  if ((int(endTimer) - int(millis())) < 0 && !isStart && !isEnd) {
    if (!uiTempFrame){
      uiTempFrame = true;
      ui.switchToFrame(0);
      delay(75);
    }
  }

  if (hunt) {
    display.displayOff();
    delay(10 * random(8,31));
    display.displayOn();
    delay(10 * random(8,31));
  }

  if (!isStart && !isEnd && tempDelay < millis()-lastTempChange) {
    lastTempChange = millis();
    if (rssiAvg < rssiThresholdFar || connected == false) {
      tempRandomizer = float(random(0,51))/10;  
      newTemp = farTemp + tempRandomizer;
      Serial.println("FAR");
    }
    if (rssiAvg > rssiThresholdFar && rssiAvg < rssiThresholdClose) {
      tempRandomizer = float(random(0,101))/10;
      newTemp = closeTemp + tempRandomizer;
      Serial.println("Close");
    }
    if (rssiAvg > rssiThresholdClose && !isFreezing) {
      tempRandomizer = float(random(0,51))/10;
      newTemp = closerTemp + tempRandomizer;
      Serial.println("Closer");
    }
    if (rssiAvg > rssiThresholdClose && isFreezing || hunt) {
      tempRandomizer = float(random(0,101))/10;
      newTemp = freezingTemp + tempRandomizer;
      Serial.println("BRRRRRR");
    }
    Serial.print("New Temperature: ");Serial.println(newTemp);
    Serial.print("Randomizer Temperature: ");Serial.println(tempRandomizer);
    int decValue = int((newTemp-floor(newTemp))*10);
    int tensValue = int(floor(newTemp));
    displayTemp = String(twoDigits(tensValue) + "." + String(decValue));
  }

  if (isEnd) {
    uiTempFrame = false;
    ui.switchToFrame(2);
    delay(75);
  }

  if (isStart) {
    uiTempFrame = false;
    ui.switchToFrame(3);
    delay(75);
  }
  
  ui.update();
}
