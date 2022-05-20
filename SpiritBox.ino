#include "BLEDevice.h"
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include "DFRobotDFPlayerMini.h"
#include "DFRobot_HT1632C.h"

#define DATA D4
#define CS D3
#define WR D7

DFRobot_HT1632C ht1632c = DFRobot_HT1632C(DATA, WR,CS);

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
char* spiritGhosts[] = {"01","03","05","06","09","10"}; //these are the ghosts that will trigger emf level 5 events
char* startNum[] = {"11"};
char* endNum[] = {"12"};
char* huntNum[] = {"13"};
boolean isSpirit = false;
boolean hunt = false;
boolean huntReturn = true;
boolean isStart = false;
boolean isEnd = false;
int numSpiritGhosts = sizeof(spiritGhosts) / sizeof(spiritGhosts[0]);
char* spiritGhostCheck;
int spiritGhostCheckVal = 0;
int ledPin1 = D9;
int buttonPin1 = D8;
int rssi = -500;
int rssiRecentSize = 5;
int rssiRecent[10] = {0, 0, 0, 0, 0}; //array size must equal rssiRecentSize
int rssiCounter = 0;
int rssiAvg;
int rssiRecentTotal;
int rssiThreshold = -75;
int rssiFloor = -90;
int rssiTimer = 1000;//time between RSSI reads
int rssiLastTime = 0;
boolean seekRSSI = false;
int defaultVolume = 18;
int DFPlayerTimeout = 500;
char* ageText[] = {"Young", "Old", "Adult", "Child"};
int ageMp3[] = {5, 7, 9, 20};
int age;
char* aggressiveText[] = {"Kill", "Attack", "Next", "Die"};
int aggressiveMp3[] = {2, 3, 6, 10};
char* locationText[] = {"Close", "Behind", "Here", "Away", "Far"};
int locationMp3[] = {4, 8, 12, 15, 18};
int ghostEventMp3[] = {11, 16, 30};
int toolMp3 = 1;
int gameEventMp3[] = {13, 19}; //19 is the one to use
char* questions[] = {"How old are you?", "What do you want", "Where are you"};
int questionRandomizer = 0;
int responseRandomizer = 0;
char* huntText[] = {"Run"};
int huntRandomizer = 0;
int surpriseHuntMp3 = 21;
int surpriseHuntVol = 20;
int surpriseNum = 1;
boolean isHuntPlaying = false;
boolean isSurprisePlaying = false;
char* surpriseHuntText[] = {"Ohhh Ah AH AH AH...RUN!!"};
int footsteps[] = {22, 23, 24, 25, 26, 27, 28, 29};
int introMp3[] = {31, 32, 33, 34};
int introVol = 20;
float fmCounter = 87.9;
int fmTimerOG = 90;
int fmTimer = 90;
int prevFmTimer = 0;
float fmAddAmount = 0.2;
boolean isFuzzPlaying = false;
boolean isButton1Pressed = false;
int buttonDelay = 6000;
int lastButtonPress = 0;
int spiritChance = 50; //percent of the time that a spirit box event will happen when in range of ghost
int spiritDistanceChance = 40; //percent of the time that a spirit box "far" or "away" event will happen. remember that it needs to pick the "where are you" question on top of this.
int spiritRandomizer = 0;
boolean isFuzz = false;
int ghostEventRandomizer = 0;
int ghostEventTrack = 0;
int ghostEventChance = 20;
int ghostEventVol = 24;
int ghostEventTimer = 10000;
int ghostEventLast = 200000000;
int startDelay = 60; //in seconds
unsigned long endTimer;
char* endGameText[] = {"GAME OVER"};
char* startGameText[] = {"GAME STARTING"};
boolean firstGhostNum = false;       // just to prevent the spirit box noise from starting before a ghost value has been received
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
      isFuzzPlaying = false;
      isFuzz = true;
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

  if (pRemoteCharacteristic->canNotify()) {
    pRemoteCharacteristic->registerForNotify(notifyCallback);
    connected = true;
    //Serial.println("Connected = True");
    return true;
  }
}

/**
   Scan for BLE servers and find the first one that advertises the service we are looking for.
*/
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    /**
        Called for each advertising BLE server.
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

void fuzz() {
  myDFPlayer.volume(defaultVolume);
  if (!isFuzzPlaying) {
    myDFPlayer.stop();
    delay(500);
    myDFPlayer.loop(1);
    isFuzzPlaying = true;
  }  
  delay(50);
  if ((millis() - prevFmTimer) > fmTimer) {
    prevFmTimer = millis();
    fmCounter += fmAddAmount;
    ht1632c.print(fmCounter, uint8_t(1));
    //Serial.println(fmCounter);
  }
  fmTimer = fmTimerOG;
  if (fmCounter >= 107.9) {
    fmCounter = 87.7;
    ht1632c.clearScreen();
    fmTimer = 0;
  }
}

void ghostResponse(int track, char* text) {
  myDFPlayer.play(track);
  delay(5);
  if (String(text).length() > 5) {
    ht1632c.clearScreen();
    strcpy(text, text);
    strcpy(text, text);
    ht1632c.print(text,35);
  }
  else {
    ht1632c.clearScreen();
    ht1632c.print(text);
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
    while (true) {
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

  pinMode(ledPin1, OUTPUT);
  pinMode(buttonPin1, INPUT);

  myDFPlayer.outputDevice(DFPLAYER_DEVICE_SD);
  myDFPlayer.setTimeOut(DFPlayerTimeout);
  myDFPlayer.volume(defaultVolume);  //Set volume value. From 0 to 30

  ht1632c.begin();
  ht1632c.isLedOn(true);
  ht1632c.clearScreen();
  ht1632c.setCursor(0, 0);
  ht1632c.print(fmCounter, uint8_t(1));
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
    BLEDevice::getScan()->start(scanTime, false);
    rssiLastTime = millis();
  }

  if (seekRSSI && rssiTimer < (millis() - rssiLastTime) && connected) {
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
    rssiAvg = rssiRecentTotal / rssiRecentSize;
    Serial.print("Average RSSI: ");Serial.println(rssiAvg);
    if (rssiCounter >= rssiRecentSize) {
      rssiCounter = 0;
    }
    rssiRecentTotal = 0;
    //Serial.print("RSSI Value: ");Serial.println(rssi);
  }

  if (newghostnum) {
    Serial.println("Received new Ghost Value");
    if (strcmp(huntNum[0], ghostnumChar) == 0) {
      hunt = true;
      isSurprisePlaying = false;
      isHuntPlaying = false;
      huntRandomizer = random(0,50);
      huntReturn = true;
      Serial.print("Hunt Num: ");Serial.println(huntRandomizer);
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
      isFuzz = false;
      isFuzzPlaying = false;
      seekRSSI = false;
      huntReturn = false;
      myDFPlayer.stop();
    }
    else {
      isEnd = false;
    }
    if (!huntReturn && !isStart && !isEnd) {   
      spiritGhostCheckVal = 0;
      for (int j = 0; j < numSpiritGhosts; j++) {
        spiritGhostCheck = spiritGhosts[j];
        if (strcmp(spiritGhostCheck, ghostnumChar) == 0) {
          spiritGhostCheckVal++;
        }
      }
      if (spiritGhostCheckVal > 0) {
        isSpirit = true;
        age = random(0,4);
        endTimer = millis() + (startDelay * 1000);
        ht1632c.clearScreen();
        Serial.println("It's a Spirit Box ghost!!");
      }
      else {
        isSpirit = false;
        endTimer = millis() + (startDelay * 1000);
        ht1632c.clearScreen();
        Serial.println("It's not a Spirit Box ghost!!");
      }
    }
    if (!isEnd && !isStart) {
      seekRSSI = true;
    }
    if (!isEnd && !isStart && huntReturn && !hunt) {   
      spiritGhostCheckVal = 0;
      for (int j = 0; j < numSpiritGhosts; j++) {
        spiritGhostCheck = spiritGhosts[j];
        if (strcmp(spiritGhostCheck, ghostnumChar) == 0) {
          spiritGhostCheckVal++;
        }
      }
      if (spiritGhostCheckVal > 0) {
        isSpirit = true;
        age = random(0,4);
        ht1632c.clearScreen();
        Serial.println("It's a Spirit Box ghost!!");
      }
      else {
        isSpirit = false;
        ht1632c.clearScreen();
        Serial.println("It's not a Spirit Box ghost!!");
      }
    }
    newghostnum = false;
    //Serial.println(ghostnumChar);
    delay(50);
    firstGhostNum = true;
  }

  if (isFuzz) {
    fuzz();
  }

  if (isEnd) {
    ht1632c.clearScreen();
    ht1632c.print(endGameText[0],35);
  }

  if (isStart) {
    ht1632c.clearScreen();
    ht1632c.print(startGameText[0],35);
    myDFPlayer.volume(introVol);
    myDFPlayer.play(introMp3[random(0,4)]);
  }

  if ((int(endTimer) - int(millis())) > 0) {
    //unsigned long timeRemaining = endTimer - millis();
    //int minutes = int(floor(float(timeRemaining/60000)));
    //int seconds = int(ceil(float((timeRemaining-(minutes*60000))/1000)));
    //String startTimerStr = String(twoDigits(minutes)+":"+twoDigits(seconds));
    //const char* startTimer = startTimerStr.c_str();
    char* wait[] = {"WAIT"};
    ht1632c.print(wait[0]);
    //delay(250);
  }

  if ((int(endTimer) - int(millis())) < 0 && !isFuzz && !isEnd && !isStart && !hunt && !isButton1Pressed && firstGhostNum) {
    ht1632c.clearScreen();
    isFuzz = true;
    ghostEventLast = int(millis());
    lastButtonPress = millis();
  }

  if (digitalRead(buttonPin1) == LOW && !hunt && (millis() - lastButtonPress) > buttonDelay &&
      !isStart && !isEnd && (int(endTimer) - int(millis())) < 0) {
    isButton1Pressed = true;
    lastButtonPress = millis();
    isFuzz = false;
  }

  if (isButton1Pressed && !hunt) {
    questionRandomizer = random(0, 3); //the 2nd value in the random function needs to equal the number of potential questions you can ask the ghost
    Serial.println(questions[questionRandomizer]);
    spiritRandomizer = random(1,101);
    if (spiritRandomizer <= spiritChance && rssiAvg > rssiThreshold && isSpirit) {
      switch (questionRandomizer) { 
        case 0:
          //responseRandomizer = random(0, (sizeof(ageMp3)/sizeof(ageMp3[0])));//this will need to move to the "start game" section when value 11 is received. aggressive and location are ok
          ghostResponse(ageMp3[age],ageText[age]);
          Serial.println("Age Response");
          break;
        case 1:
          responseRandomizer = random(0, (sizeof(aggressiveMp3)/sizeof(aggressiveMp3[0])));
          ghostResponse(aggressiveMp3[responseRandomizer],aggressiveText[responseRandomizer]);
          Serial.println("Aggressive Response");
          break;
        case 2:
          responseRandomizer = random(0, 3);//not using far and away for this part
          ghostResponse(locationMp3[responseRandomizer],locationText[responseRandomizer]);
          Serial.println("Location Response");
          break;
       }   
      delay(2500);
    }
    else if (spiritRandomizer <= spiritDistanceChance && questionRandomizer == 2 && rssiAvg < rssiThreshold && isSpirit) {
      int randDistance = random(4,6);
      ghostResponse(locationMp3[randDistance],locationText[randDistance]);
      Serial.println("Location Response");
      delay(2500);
    }
    else {
      myDFPlayer.stop();
      ht1632c.clearScreen();
      ht1632c.print(fmCounter, uint8_t(1));
      delay(1500);
      digitalWrite(ledPin1,HIGH);
      delay(1000);
      digitalWrite(ledPin1,LOW);
    }
    isButton1Pressed = false;
    isFuzz = true;
    isFuzzPlaying = false;
    ht1632c.clearScreen();
    ghostEventLast = int(millis());
  }

  if (hunt) {
    ghostEventLast = int(millis());
  }

  if (hunt && connected) {
    if (huntRandomizer == surpriseNum && !isSurprisePlaying) {
      isFuzz = false;
      myDFPlayer.volume(surpriseHuntVol);
      myDFPlayer.play(surpriseHuntMp3);
      ht1632c.print(surpriseHuntText[0],35);
      isSurprisePlaying = true;
      isFuzzPlaying = false;
      Serial.println("Surprise hunt begin");
    }
    if (huntRandomizer == surpriseNum && isSurprisePlaying) {
      ht1632c.print(surpriseHuntText[0],35);
    }
    if (!isHuntPlaying && huntRandomizer != surpriseNum) {
      ht1632c.clearScreen();
      ht1632c.print(huntText[0]);
      Serial.println("Normal hunt begin");
      digitalWrite(ledPin1,HIGH);
      delay(200);
      digitalWrite(ledPin1,LOW);
      ht1632c.clearScreen();
      delay(200);
      digitalWrite(ledPin1,HIGH);
      ht1632c.print(huntText[0]);
      delay(200);
      digitalWrite(ledPin1,LOW);
      ht1632c.clearScreen();
      delay(200);
      digitalWrite(ledPin1,HIGH);
      ht1632c.print(huntText[0]);
      delay(200);
      digitalWrite(ledPin1,LOW);
      ht1632c.clearScreen();
      delay(200);
      digitalWrite(ledPin1,HIGH);
      ht1632c.print(huntText[0]);
      delay(200);
      digitalWrite(ledPin1,LOW);
      ht1632c.clearScreen();
      delay(200);
      digitalWrite(ledPin1,HIGH);
      ht1632c.print(huntText[0]);
      delay(200);
      digitalWrite(ledPin1,LOW);
      ht1632c.clearScreen();
      delay(200);
      digitalWrite(ledPin1,HIGH);
      ht1632c.print(huntText[0]);
      delay(200);
      digitalWrite(ledPin1,LOW);
      ht1632c.clearScreen();
      delay(200);
      digitalWrite(ledPin1,HIGH);
      ht1632c.print(huntText[0]);
      delay(200);
      digitalWrite(ledPin1,LOW);
      ht1632c.clearScreen();
      delay(200);
      isHuntPlaying = true; 
    }
  }
  
  if (!hunt && connected && rssiAvg > rssiThreshold && (int(millis())-ghostEventLast) > ghostEventTimer && !isButton1Pressed &&
      !isEnd && !isStart && (int(endTimer) - int(millis())) < 0) {
    ghostEventRandomizer = random(1,101);
    Serial.print("Ghost Event Random Number: ");Serial.println(ghostEventRandomizer);
    delay(50);
    if (ghostEventRandomizer <= ghostEventChance) {
      Serial.println("Ghost Event Started");
      myDFPlayer.stop();
      myDFPlayer.volume(ghostEventVol);
      delay(5);
      ghostEventTrack = random(0,(sizeof(ghostEventMp3)/sizeof(ghostEventMp3[0])));
      if (ghostEventTrack == 2) {
        myDFPlayer.volume((defaultVolume * 3));
        Serial.println("LOUDA!!");
        delay(5);
      }
      Serial.print("Ghost Event Mp3 Size: ");Serial.println(sizeof(ghostEventMp3)/sizeof(ghostEventMp3[0]));
      Serial.print("Ghost Event Track Indicator Val: ");Serial.println(ghostEventTrack);
      Serial.print("Ghost Event Track Number: ");Serial.println(ghostEventMp3[ghostEventTrack]);
      myDFPlayer.play(ghostEventMp3[ghostEventTrack]);
      int mp3Status = myDFPlayer.readState();
      while (mp3Status != -1) {
        mp3Status = myDFPlayer.readState();
        digitalWrite(ledPin1,HIGH);
        delay(50);
        digitalWrite(ledPin1,LOW);
        delay(25);
        digitalWrite(ledPin1,HIGH);
        delay(100);
        digitalWrite(ledPin1,LOW);
        delay(25);
        digitalWrite(ledPin1,HIGH);
        delay(75);
        digitalWrite(ledPin1,LOW);
        delay(40);
      }
      digitalWrite(ledPin1,LOW);
      delay(100);
      myDFPlayer.volume(defaultVolume);
      isFuzzPlaying = false;
    }
    ghostEventLast = int(millis());
  }

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
