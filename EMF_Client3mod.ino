#include "BLEDevice.h"
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include "DFRobotDFPlayerMini.h"
//#include <esp_gap_ble_api.h>

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
char* emfGhosts[] = {"02","04","06","07","09","10"};//these are the ghosts that will trigger emf level 5 events
char* startNum[] = {"11"};
char* endNum[] = {"12"};
char* huntNum[] = {"13"};
boolean isEMF = false;
boolean hunt = false;
boolean huntReturn = true;
boolean isStart = false;
boolean isEnd = false;
int numEmfGhosts = sizeof(emfGhosts)/sizeof(emfGhosts[0]);
char* emfGhostCheck;
int emfGhostCheckVal = 0;
int ledPin1 = D3;
int ledPin2 = D4;
int ledPin3 = D7;
int ledPin4 = D8;
int ledPin5 = D9;
int rssi = -500;
int rssiRecentSize = 10;
int rssiRecent[10] = {0,0,0,0,0,0,0,0,0,0}; //array size must equal rssiRecentSize
int rssiCounter = 0;
int rssiAvg;
int rssiRecentTotal;
int rssiThreshold = -70;
int rssiFloor = -90;
int rssiTimer = 100;//time between RSSI reads
int rssiLastTime = 0;
boolean seekRSSI = false;
int defaultVolume = 10;
int DFPlayerTimeout = 500;
int emfRandomizer = 0;
int emfRandomTimer = 0;
int emfRandomPrevTime = 15000;
int emfLevel = 0;
int prevEmfLevel = 0;
boolean newEmfLevel = false;
int nonEmf5Chances[] = {70,10,10,5,5,0};//percent chance that the emf doesn't go off, is a emf level 1, emf level 2, etc...
int emf5Chances[] = {70,2,3,5,5,15};//Each chance arrays should add to 100
boolean isMp3Playing = false;
int huntRandomTimer = 1000;
int huntPrevTime = 0;
int emfLevelHunt = 1;
int startDelay = 60; //in seconds
int startLedDelay = 200;
unsigned long endTimer;
boolean endGameSound = false;  
boolean introGameSound = false;
boolean isGhostNum = false;       // just to prevent the spirit box noise from starting before a ghost value has been received
BLEClient* pClient = BLEDevice::createClient();
DFRobotDFPlayerMini myDFPlayer;
void printDetail(uint8_t type, int value);

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
    setEmfLevel(0);//emf turns off during a hunt if you are far away from ghost
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

void setEmfLevel(int level) {
  int ledPins[] = {ledPin1,ledPin2,ledPin3,ledPin4,ledPin5};
  for (int j = 0; j < level ; j++) {
    digitalWrite(ledPins[j], HIGH);
    //Serial.print("LED Pin High: ");Serial.println(ledPins[j]);
  }
  for (int j = level; j < 5; j++) {
    digitalWrite(ledPins[j], LOW);
    //Serial.print("LED Pin Low: ");Serial.println(ledPins[j]);
  }
  int vol = defaultVolume + (level*4);
  myDFPlayer.volume(vol);
  if (level != 0 && !isMp3Playing) {
    myDFPlayer.loop(1);
    isMp3Playing = true;
  }
  else if (level == 0 && isMp3Playing) {
    myDFPlayer.pause();
    isMp3Playing = false;
  }
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600);
  Serial.println("Starting Arduino BLE Client application...");
  BLEDevice::init("");
  Serial.println(F("Initializing DFPlayer ... (May take 3~5 seconds)"));
  myDFPlayer.begin(Serial2);
  if (!myDFPlayer.begin(Serial2)) {  //Use softwareSerial to communicate with mp3.
    Serial.println(F("Unable to begin:"));
    Serial.println(F("1.Please recheck the connection!"));
    Serial.println(F("2.Please insert the SD card!"));
    while(true){
      delay(0); // Code to compatible with ESP8266 watch dog.
    }
  }
  Serial.println(F("DFPlayer Mini online."));

  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device.  Specify that we want active scanning.
  // Not entirely sure what the setInterval or setWindow options are, but they work as is.
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
  pBLEScan->setActiveScan(true);

  pinMode(ledPin1,OUTPUT);
  pinMode(ledPin2,OUTPUT);
  pinMode(ledPin3,OUTPUT);
  pinMode(ledPin4,OUTPUT);
  pinMode(ledPin5,OUTPUT);

  myDFPlayer.outputDevice(DFPLAYER_DEVICE_SD);
  myDFPlayer.setTimeOut(DFPlayerTimeout);
  myDFPlayer.volume(defaultVolume);  //Set volume value. From 0 to 30
  
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

  if (connected == false) {
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
    Serial.println("Received new Ghost Value");
    if (strcmp(huntNum[0],ghostnumChar) == 0) {
      hunt = true;
      isGhostNum = false;
      Serial.println("HUNT");
      huntReturn = true;
      endGameSound = true;
    }
    else {
      hunt = false;
      //setEmfLevel(0);
      //newEmfLevel = false;
    }
    if (strcmp(startNum[0],ghostnumChar) == 0) {
      isStart = true;
      isGhostNum = false;
      endGameSound = true;
    }
    else {
      isStart = false;
    }
    if (strcmp(endNum[0],ghostnumChar) == 0) {
      isEnd = true;
      setEmfLevel(0);
      newEmfLevel = false;
      huntReturn = false;
      seekRSSI = false;
      isGhostNum = false;
      myDFPlayer.stop();
      Serial.println("Game Over");
    }
    else {
      isEnd = false;
    }
    if (!huntReturn && !isStart && !isEnd) {
      setEmfLevel(0);
      emfGhostCheckVal = 0;
      endGameSound = true;
      introGameSound = true;
      for (int j = 0; j < numEmfGhosts; j++) {
        emfGhostCheck = emfGhosts[j];
        if (strcmp(emfGhostCheck, ghostnumChar) == 0) {
          emfGhostCheckVal++;
        }
      }
      if (emfGhostCheckVal > 0) {
        isEMF = true;
        endTimer = millis() + (startDelay * 1000);
        Serial.println("It's an EMF 5 ghost!!");
      }
      else {
        isEMF = false;
        endTimer = millis() + (startDelay * 1000);
        Serial.println("It's not an EMF 5 ghost!!");
      }
    isGhostNum = true;  
    }
    if (!isEnd && !isStart) {
      seekRSSI = true;
    }
    if (!isEnd && !isStart && huntReturn && !hunt) {   
      emfGhostCheckVal = 0;
      endGameSound = true;
      for (int j = 0; j < numEmfGhosts; j++) {
        emfGhostCheck = emfGhosts[j];
        if (strcmp(emfGhostCheck, ghostnumChar) == 0) {
          emfGhostCheckVal++;
        }
      }
      if (emfGhostCheckVal > 0) {
        isEMF = true;
        Serial.println("It's a Spirit Box ghost!!");
      }
      else {
        isEMF = false;
        Serial.println("It's not a Spirit Box ghost!!");
      }
    isGhostNum = true;
    }
    newghostnum = false;
    //Serial.println(ghostnumChar);
    delay(50);
  }

  if ((int(endTimer) - int(millis())) > 2000 && !isEnd && !isStart && !hunt) {   
    digitalWrite(ledPin1, HIGH);
  }

  if (introGameSound && (int(endTimer) - int(millis())) < 2000) {
    myDFPlayer.volume(defaultVolume*2);
    delay(5);
    myDFPlayer.play(2);
    introGameSound = false;
    digitalWrite(ledPin1, LOW);
    digitalWrite(ledPin2, LOW);
    digitalWrite(ledPin3, LOW);
    digitalWrite(ledPin4, LOW);
    digitalWrite(ledPin5, LOW);
  }

  if ((millis()-emfRandomPrevTime) > emfRandomTimer && !hunt && connected && (int(endTimer) - int(millis())) < 0 && isGhostNum) {
    emfRandomPrevTime = millis();
    emfRandomTimer = 4000 * random(2,6);//every 8-20 seconds, the emf number can change
    emfRandomizer = random(0,100);
    int percentageAdder = 0;
    if (isEMF) {
      for (int j = 0; j < 6; j++) {
        percentageAdder += emf5Chances[j];
        if (emfRandomizer < percentageAdder) {
          emfLevel = j;
          if (emfLevel != prevEmfLevel) {
            newEmfLevel = true;
            prevEmfLevel = emfLevel;
          }
          break;
        }
      }
    }
    if (!isEMF) {
      for (int j = 0; j < 6; j++) {
        percentageAdder += nonEmf5Chances[j];
        if (emfRandomizer < percentageAdder) {
          emfLevel = j;
          if (emfLevel != prevEmfLevel) {
            newEmfLevel = true;
            prevEmfLevel = emfLevel;
          }
          break;
        }
      }  
    }
    Serial.print("EMF Level is: ");Serial.println(emfLevel);
  }

  if (rssiAvg > rssiThreshold && !hunt && newEmfLevel) {
    setEmfLevel(emfLevel);
    newEmfLevel = false;
    Serial.print("EMF has been set to level: ");Serial.println(emfLevel);
  }

  if (rssiAvg < rssiThreshold && !hunt) {
    setEmfLevel(0);
    newEmfLevel = true;
  }

  if ((millis() - huntPrevTime) > huntRandomTimer && hunt) {
    huntPrevTime = millis();
    huntRandomTimer = 50 * random(3,13); //emf level stays at a given state for 0.1-0.6 second
    emfLevelHunt = random(1,6); 
    setEmfLevel(emfLevelHunt);
  }

  if (isEnd && endGameSound) {
    myDFPlayer.volume(defaultVolume*2);
    delay(5);
    myDFPlayer.play(3);
    endGameSound = false;
  }

  if (myDFPlayer.available()) {
    printDetail(myDFPlayer.readType(), myDFPlayer.read()); //Print the detail message from DFPlayer to handle different errors and states.
  }
  
  delay(10); // Delay a second between loops.
}

void printDetail(uint8_t type, int value){
  switch (type) {
    case TimeOut:
      Serial.println(F("Time Out!"));
      break;
    case WrongStack:
      Serial.println(F("Stack Wrong!"));
      break;
    case DFPlayerCardInserted:
      Serial.println(F("Card Inserted!"));
      break;
    case DFPlayerCardRemoved:
      Serial.println(F("Card Removed!"));
      break;
    case DFPlayerCardOnline:
      Serial.println(F("Card Online!"));
      break;
    case DFPlayerUSBInserted:
      Serial.println("USB Inserted!");
      break;
    case DFPlayerUSBRemoved:
      Serial.println("USB Removed!");
      break;
    case DFPlayerPlayFinished:
      Serial.print(F("Number:"));
      Serial.print(value);
      Serial.println(F(" Play Finished!"));
      break;
    case DFPlayerError:
      Serial.print(F("DFPlayerError:"));
      switch (value) {
        case Busy:
          Serial.println(F("Card not found"));
          break;
        case Sleeping:
          Serial.println(F("Sleeping"));
          break;
        case SerialWrongStack:
          Serial.println(F("Get Wrong Stack"));
          break;
        case CheckSumNotMatch:
          Serial.println(F("Check Sum Not Match"));
          break;
        case FileIndexOut:
          Serial.println(F("File Index Out of Bound"));
          break;
        case FileMismatch:
          Serial.println(F("Cannot Find File"));
          break;
        case Advertise:
          Serial.println(F("In Advertise"));
          break;
        default:
          break;
      }
      break;
    default:
      break;
  }
  
}
