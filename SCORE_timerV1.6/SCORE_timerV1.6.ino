/*
   FITZ Scoreboard
   This work is based on:
   ESP8266 + FastLED + IR Remote + MSGEQ7: https://github.com/jasoncoon/esp8266-fastled-webserver
   Copyright (C) 2015 Jason Coon
   which is heavy taken from many other's work!

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
 * I have comminted this but you should have a basic understanding of :FastLED; Arduino; html; css; jquery; javascript and wifi networks
 */

#include "FastLED.h"
FASTLED_USING_NAMESPACE

extern "C" {
#include "user_interface.h"
}

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <DNSServer.h>
#include <EEPROM.h>
#include "GradientPalettes.h"
//DNS SERVER //this will make any webpage redirect to the index page
const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);
DNSServer dnsServer;
//DNS SERVER
const bool apMode = true; //if connecting to a WIFI network change to "false"

// AP mode password
const char WiFiAPPSK[] = "scoreboard"; //This is the password needed to log onto the AP

// Wi-Fi network to connect to (if not in AP mode)  SET THIS TO YOUR WIFI SSID && PASSWORD
const char* ssid = "";
const char* password = "";

ESP8266WebServer server(80);

#define DATA_PIN      6    // for Huzzah: Pins w/o special function:  #4, #5, #12, #13, #14; // #16 does not work :(
#define LED_TYPE      WS2812
#define COLOR_ORDER   GRB
#define NUM_LEDS      103

#define MILLI_AMPS         3000     // IMPORTANT: set here the max milli-Amps of your power supply 5V 2A = 2000
#define FRAMES_PER_SECOND  120 // here you can control the speed. With the Access Point / Web Server the animations run a bit slower.

CRGB leds[NUM_LEDS];

uint8_t patternIndex = 0;

const uint8_t brightnessCount = 5;
uint8_t brightnessMap[brightnessCount] = { 16, 32, 64, 128, 255 };
int brightnessIndex = 3;
uint8_t brightness = brightnessMap[brightnessIndex];
uint8_t tempBrightness;

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))
#define SECONDS_PER_PALETTE 120

///////////////////////////////////////////////////////////////////////

// Forward declarations of an array of cpt-city gradient palettes, and
// a count of how many there are.  The actual color palette definitions
// are at the bottom of this file.
extern const TProgmemRGBGradientPalettePtr gGradientPalettes[];
extern const uint8_t gGradientPaletteCount;

// Current palette number from the 'playlist' of color palettes
uint8_t gCurrentPaletteNumber = 0;

CRGBPalette16 gCurrentPalette( CRGB::Black);
CRGBPalette16 gTargetPalette( gGradientPalettes[0] );

uint8_t currentPatternIndex = 0; // Index number of which pattern is current
bool autoplayEnabled  = false;
bool ActiveTeam       = 0;
uint8_t autoPlayDurationSeconds = 10;
unsigned int autoPlayTimeout = 0;
uint8_t gHue = 0; // rotating "base color" used by many of the patterns
CRGB solidColor = CRGB::Red;
uint8_t power = 1;

//timer vars
unsigned long timerOldTime = 0;
int interval = 100;
long int startTime = 36000; //the time for the timer in tenths of seconds 1 minute = 600
int currentTime;
uint8_t timerDisplayDelay;
bool setTimer   = true;
bool pauseTimer = false;
bool startTimer = true;
bool stopTimer  = false;
bool colonOn    = false;
int8_t colonCount = 0;


// hours:minute:seconds:tenthSeconds
uint8_t hh;
uint8_t mm;
uint8_t ss;
uint8_t ms;

//shared with timer and game time
uint8_t timerDigit1;
uint8_t timerDigit2;
uint8_t timerDigit3;
uint8_t timerDigit4;

//game timer vars
unsigned long timerGameOldTime = 0;
long int startGameTime = 36000; //the time for the timer in tenths of seconds 1 minute = 600
int currentGameTime;
uint8_t timerGameDisplayDelay;
bool setGameTimer   = true;
bool pauseGameTimer = false;
bool startGameTimer = false;
bool stopGameTimer  = false;


// hours:minute:seconds:tenthSeconds
uint8_t gamehh;
uint8_t gamemm;
uint8_t gamess;
uint8_t gamems;

//shared with timer and game time
uint8_t timerGameDigit1;
uint8_t timerGameDigit2;
uint8_t timerGameDigit3;
uint8_t timerGameDigit4;

//display vars
bool blackout       = true;  
bool showFlare      = false;  //add a little flare to the numbers
bool showPatterns   = false;  //shows the patterns or if false shows the timers or score
bool showJudges     = false;  // Shows the Judges Scores
bool showScores     = false;   //shows the Team scores
bool showGameTimer  = false;  //shows the game timer
bool showTimer      = false;  //shows the count DOWN timer
bool commitScores;            // Commits the current scores

//judges scores
int8_t setJudge1 = -1;        //set to -1 as a place holder  all "if" statments are based on setting of -1 or not!
int8_t setJudge2 = -1;
int8_t setJudge3 = -1;
int8_t setJudge4 = -1;




/***********************************************************************
   Digit Setup and Arrays
*/

int digitOffset[2][2] {{0, 25}, {53, 78}};      //defines the offset or starting point of each digit. This assumes 25 pixals per digit. also has 3 leds as seperator between each group of 2 digits
int jDigitOffset[] {digitOffset[0][0],digitOffset[0][1],digitOffset[1][0],digitOffset[1][1]};  //same as above just used for judges better than a switch/case in function


//Numbers Display Array. This ishe first digit other digits will use an offset i.e. 0 + 25 will display the first pixel of the second digit. the 1st number in the Array is how many there are in the array (to save calculating array size each time.)
int num[10][26] {
  /*ZERO*/  {23, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21},
  /*ONE*/   {10, 19, 18, 17, 16, 15, 14, 13, 12, 11},
  /*TWO*/   {20, 0, 21, 20, 19, 18, 17, 16, 15, 24, 23, 22, 4, 5, 6, 7, 8, 9, 10, 11},
  /*THREE*/ {20, 0, 21, 20, 19, 18, 17, 16, 15, 24, 23, 22, 14, 13, 12, 7, 8, 9, 10, 11},
  /*FOUR*/  {16, 0, 1, 2, 3, 22, 23, 24, 15, 16, 17, 18, 14, 13, 12, 11},
  /*FIVE*/  {20, 18, 19, 20, 21, 0, 1, 2, 3, 22, 23, 24, 14, 13, 12, 11, 10, 9, 8, 7},
  /*SIX*/   {23, 18, 19, 20, 21, 0, 1, 2, 3, 22, 23, 24, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4},
  /*SEVEN*/ {13, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 0},
  /*EIGHT*/ {26, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24},
  /*NINE*/  {19, 0, 1, 2, 3, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24}
};

/*********************************************************************
  Setup for the Team varibles
***********************************************************************/
uint16_t teamScore[] = { 0 , 0 };
int8_t teamColor[2][3] = {{247, 19, 19}, {0, 51, 255}};
int8_t judgeColor[4][3] = {{247, 130, 41},{84, 184, 54} , {255 ,195 ,0},{243, 75, 122}};
//char16_t TeamName1 =""; //set for V2.0
//char16_t TeamName2 =""; //set for V2.0
//uint8_t lastScore1_1;   //Used for undos and fades
//uint8_t lastScore1_2;   //Used for undos and fades
//uint8_t lastScore2_1;   //Used for undos and fades TEAM 2
//uint8_t lastScore2_2;   //Used for undos and fades TEAM 2
/***********************************************************************/


void setup(void) {
  Serial.begin(115200);
  delay(10);
  Serial.setDebugOutput(true);
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);         // for WS2812
  FastLED.setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(brightness);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, MILLI_AMPS);
  fill_solid(leds, NUM_LEDS, solidColor);  
  FastLED.show();
  EEPROM.begin(512);
  loadSettings();
  Serial.println();
  Serial.print( F("Heap: ") ); Serial.println(system_get_free_heap_size());
  Serial.print( F("Boot Vers: ") ); Serial.println(system_get_boot_version());
  Serial.print( F("CPU: ") ); Serial.println(system_get_cpu_freq());
  Serial.print( F("SDK: ") ); Serial.println(system_get_sdk_version());
  Serial.print( F("Chip ID: ") ); Serial.println(system_get_chip_id());
  Serial.print( F("Flash ID: ") ); Serial.println(spi_flash_get_id());
  Serial.print( F("Flash Size: ") ); Serial.println(ESP.getFlashChipRealSize());
  Serial.print( F("Vcc: ") ); Serial.println(ESP.getVcc());
  Serial.println();


  SPIFFS.begin();
  {
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      Serial.printf("FS File: %s, size: %s\n", fileName.c_str(), String(fileSize).c_str());
    }
    Serial.printf("\n");
  }

  if (apMode)
  {
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    // Do a little work to get a unique-ish name. Append the
    // last two bytes of the MAC (HEX'd) to "Thing-":
    uint8_t mac[WL_MAC_ADDR_LENGTH];
    WiFi.softAPmacAddress(mac);
    String macID = String(mac[WL_MAC_ADDR_LENGTH - 2], HEX) +
                   String(mac[WL_MAC_ADDR_LENGTH - 1], HEX);
    macID.toUpperCase();
    String AP_NameString = "FITZ || SCOREBOARD || FITZ " + macID;

    char AP_NameChar[AP_NameString.length() + 1];
    memset(AP_NameChar, 0, AP_NameString.length() + 1);

    for (int i = 0; i < AP_NameString.length(); i++)
      AP_NameChar[i] = AP_NameString.charAt(i);

    WiFi.softAP(AP_NameChar, WiFiAPPSK);
    dnsServer.start(DNS_PORT, "*", apIP);

    Serial.printf("Connect to Wi-Fi access point: %s\n", AP_NameChar);
    Serial.println("and open http://192.168.4.1 or ANY.com in your browser");
  }
  else
  {
    WiFi.mode(WIFI_STA);
    Serial.printf("Connecting to %s\n", ssid);
    if (String(WiFi.SSID()) != String(ssid)) {
      WiFi.begin(ssid, password);
    }

    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }

    Serial.print("Connected! Open http://");
    Serial.print(WiFi.localIP());
    Serial.println(" in your browser");
  }

  //  server.serveStatic("/", SPIFFS, "/index.htm"); // ,"max-age=86400"

  server.on("/all", HTTP_GET, []() {
    sendAll();
  });

  server.on("/power", HTTP_GET, []() {
    sendPower();
  });

  server.on("/Team", HTTP_POST, []() {
    String value = server.arg("value");
    ActiveTeam = value.toInt();
  });
  
  server.on("/Team", HTTP_GET, []() {
    String value = server.arg("value");
    ActiveTeam = value.toInt();
  });
  
  server.on("/Judge1", HTTP_POST, []() {
    String value = server.arg("value");
    setJudge1 = value.toInt();
  });
  server.on("/Judge2", HTTP_POST, []() {
    String value = server.arg("value");
    setJudge2 = value.toInt();
  });
  server.on("/Judge3", HTTP_POST, []() {
    String value = server.arg("value");
    setJudge3 = value.toInt();
  });
  server.on("/Judge4", HTTP_POST, []() {
    String value = server.arg("value");
    setJudge4 = value.toInt();
  });

  server.on("/setTimer", HTTP_POST, []() {
    String value = server.arg("value");
    startTime = value.toInt();    
    setTimer = true;
    
  });

  server.on("/setGameTimer", HTTP_POST, []() {
    String value = server.arg("value");
    startGameTime = value.toInt();    
    setGameTimer = true;
    
  });

    server.on("/timerFunction", HTTP_POST, []() {
    String value = server.arg("value");
    int8_t test = value.toInt(); 
      switch(test){
        case 0:             //stop Timer
          currentTime = 0;
          stopTimer = true;
          pauseTimer = true;
          showScores = true;
          showTimer  = false;
          Serial.println("stop timer");
          break;
        case 1:             //start Timer
          startTimer = true;
          pauseTimer =false;
          Serial.println("start timer");
          break;
        case 2:             //Pause Timer
          pauseTimer = true;
          colonOn    = true;
          Serial.println("pause timer");
          break;
        case 3:             //reset Timer
          setTimer   = true;
          pauseTimer = true;
          colonOn    = true;
          Serial.println("reset timer");
          break;
      }
  });

   server.on("/gametimerFunction", HTTP_POST, []() {
    String value = server.arg("value");
    int8_t test = value.toInt(); 
      switch(test){
        case 0:             //stop Timer
          currentGameTime = 0;
          stopGameTimer = true;
          pauseGameTimer = true;
          showScores = true;
          showGameTimer  = false;
          Serial.println("stop timer");
          break;
        case 1:             //start Timer
          startGameTimer = true;
          Serial.println("start timer");
          break;
        case 2:             //Pause Timer
          pauseGameTimer = true;
          colonOn    = true;
          Serial.println("pause timer");
          break;
        case 3:             //reset Timer
          setGameTimer   = true;
          pauseGameTimer = true;
          colonOn    = true;
          Serial.println("reset timer");
          break;
      }
  });

  
  server.on("/RS", HTTP_POST, []() {
    teamScore[0] = 0;
    teamScore[1] = 0; 
  });
  
  server.on("/editScoreUp", HTTP_POST, []() {
      String value = server.arg("value");
      teamScore[ActiveTeam]++;
  });

  server.on("/editScoreDn", HTTP_POST, []() {
      String value = server.arg("value");
      teamScore[ActiveTeam]--;
  });
 
  server.on("/CS", HTTP_POST, []() {  //Commit Scores to Memory
    String value = server.arg("value");
    if (setJudge1>-1) {teamScore[ActiveTeam] = teamScore[ActiveTeam]+setJudge1; setJudge1=-1;}  //Add to team score and reset the Judge score to -1
    if (setJudge2>-1) {teamScore[ActiveTeam] = teamScore[ActiveTeam]+setJudge2; setJudge2=-1;}
    if (setJudge3>-1) {teamScore[ActiveTeam] = teamScore[ActiveTeam]+setJudge3; setJudge3=-1;}
    if (setJudge4>-1) {teamScore[ActiveTeam] = teamScore[ActiveTeam]+setJudge4; setJudge4=-1;}
    wipe(0);
    showScores  = true;
    showJudges  = false;
    showFlare   = false;
  });

  server.on("/solidColor", HTTP_GET, []() {
    sendSolidColor();
  });

  server.on("/solidColor", HTTP_POST, []() {
    String r = server.arg("r");
    String g = server.arg("g");
    String b = server.arg("b");
    setSolidColor(r.toInt(), g.toInt(), b.toInt());
    teamColor[ActiveTeam][0] = r.toInt();
    teamColor[ActiveTeam][1] = g.toInt();
    teamColor[ActiveTeam][2] = b.toInt();
//    Serial.print("Red :");
//    Serial.println(r);
//    Serial.print("Green :");
//    Serial.println(g);
//    Serial.print("Blue :");
//    Serial.println(b);
    sendSolidColor();
  });

  server.on("/pattern", HTTP_GET, []() {
    sendPattern();
  });

  server.on("/pattern", HTTP_POST, []() {
    String value = server.arg("value");
    setPattern(value.toInt());
    if (setPattern == 0) {
      showPatterns = false;
    }
    else {
      showPatterns = true;
    }
    sendPattern();
  });

  server.on("/patternUp", HTTP_POST, []() {
    adjustPattern(true);
    sendPattern();
  });

  server.on("/patternDown", HTTP_POST, []() {
    adjustPattern(false);
    sendPattern();
  });

  server.on("/brightness", HTTP_GET, []() {
    sendBrightness();
  });

  server.on("/brightness", HTTP_POST, []() {
    String value = server.arg("value");
    setBrightness(value.toInt());
    sendBrightness();
  });

  server.on("/brightnessUp", HTTP_POST, []() {
    adjustBrightness(true);
    sendBrightness();
  });

  server.on("/brightnessDown", HTTP_POST, []() {
    adjustBrightness(false);
    sendBrightness();
  });

  server.on("/showScores", HTTP_POST,[](){ 
    blackout      =false;  
    showJudges    =false;
    showTimer     = false;
    showGameTimer =false;
    showFlare     =false;
    
    if(showScores==true){showScores=false;}else{showScores=true;}
  });

  server.on("/showJudges", HTTP_POST,[](){
    blackout      =false;   
    showScores    =false;
    showTimer     =false;
    showGameTimer =false;
    showFlare     =false;
    
    if(showJudges==true){showJudges=false;}else{showJudges=true;}
  });

  server.on("/showGameTime", HTTP_POST,[](){ 
    blackout      =false;    
    showScores    =false;
    showTimer     =false;
    showJudges    =false;
    showFlare     =false;
    
    if(showGameTimer==true){showGameTimer=false;}else{showGameTimer=true;}
  });

  server.on("/showTimer", HTTP_POST,[](){
    blackout      =false;    
    showScores    =false;
    showGameTimer =false;
    showJudges    =false;
    showFlare     =false;
    
    if(showTimer==true){showTimer=false;}else{showTimer=true;}
  });

  server.on("/blackout", HTTP_POST,[](){   
    showScores    =false;
    showGameTimer  =false;
    showJudges    =false;
    showFlare     =false;
    showTimer     =false;
    
    if(blackout==true){blackout=false;}else{blackout=true;}
  });

  
  //file locations
  server.serveStatic("/index.htm", SPIFFS, "/index.htm");
  server.serveStatic("/fonts", SPIFFS, "/fonts", "max-age=86400");
  server.serveStatic("/js", SPIFFS, "/js");
  server.serveStatic("/css", SPIFFS, "/css", "max-age=86400");
  server.serveStatic("/images", SPIFFS, "/images", "max-age=86400");
  server.serveStatic("/", SPIFFS, "/index.htm");
  server.begin();

  Serial.println("HTTP server started");

  autoPlayTimeout = millis() + (autoPlayDurationSeconds * 1000);
  bool timerActive = false;
  unsigned long timer;
  int delayTime = 10;
}
//display the judges score

void PScore(uint8_t score, bool team) {
  if (teamScore[0] >= 100) {
    teamScore[0] = 0;
  }
  if (teamScore[1] >= 100) {
    teamScore[1] = 0;
  }

  int8_t d1 = score / 10;            //Grabbing the "tens" place of the score
  int8_t d2 = score % 10;            //Grabbing the "ones" place of the score
  /*********************************************************************
    First Digit of Team.
  **********************************************************************/
  int8_t s = num[d1][0];             //number of the leds in the array used in the for loop (first digit of the num sub array's)
  for (int8_t i = 1 ; i < s; i++) {                //loop to set LED colors of the "tens" place. d1 means digit 1
    int16_t k = num[d1][i] + digitOffset[team][0];           // Setting the correct pixel and the group offset
    leds[k] = CRGB(teamColor[team][0], teamColor[team][1], teamColor[team][2]);
  }
  /*********************************************************************
    Second Digit of Team. Same as the first with a 25 pixel offset
  **********************************************************************/
  s = num[d2][0];                                 //number of the leds in the array second digit

  for (int i = 1; i < 25; i++) {
    leds[i + digitOffset[team][1]] = CHSV(0, 0, 0);
  }
  for (int i = 1; i < s; i++) {
    int16_t k = num[d2][i] + digitOffset[team][1];
    leds[k] = CRGB(teamColor[team][0], teamColor[team][1], teamColor[team][2]);
  }
}

void pTimer(){
      fadeToBlackBy( leds, NUM_LEDS, 5);
        hh = currentTime / 36000 % 24;
        mm = currentTime / 600 % 60;
        ss = currentTime / 10 % 60;
        ms = currentTime  % 10;
       // Serial.print("Time: ");Serial.print(hh);Serial.print(":");Serial.print(mm);Serial.print(":");Serial.print(ss);Serial.print(":");Serial.println(ms);      
        if(currentTime>36000){timerDigit1=hh/10; timerDigit2=hh%10; timerDigit3=mm/10; timerDigit4=mm%10;}
        if(currentTime<=36000 && currentTime>1800){timerDigit1=NULL; timerDigit2=NULL; timerDigit3=mm/10; timerDigit4=mm%10;}
        //MOVING THE TIME OVER
        if(currentTime<=1800 && currentTime>1720){timerDigit1=NULL; timerDigit2=NULL; timerDigit3=NULL; timerDigit4=mm%10;}
        if(currentTime<=1720 && currentTime>1710){timerDigit1=NULL; timerDigit2=NULL; timerDigit4=NULL; timerDigit3=mm%10;}
        if(currentTime<=1710 && currentTime>1700){timerDigit1=NULL; timerDigit4=NULL; timerDigit3=NULL; timerDigit2=mm%10;}
        
        if(currentTime<=1700 && currentTime>600){timerDigit1=NULL; timerDigit2=mm%10; timerDigit3=ss/10; timerDigit4=ss%10;}
        
        if(currentTime<=600 && currentTime>580){timerDigit1=ss/10; timerDigit2=ss%10; timerDigit3=ss/10; timerDigit4=ss%10; }
        if(currentTime<=580 && currentTime>100){timerDigit1=ss/10; timerDigit2=ss%10; timerDigit3=NULL; timerDigit4=NULL;}
        if(currentTime<=100){timerDigit1=ss/10; timerDigit2=ss%10; timerDigit3=ms; timerDigit4=NULL;}
        if(currentTime<=0){pauseTimer = true; timerDisplayDelay++;}
        
        if(timerDisplayDelay>=50){setTimer = true;}

//        Serial.print(timerDigit1);
//        Serial.print(timerDigit2);
//        Serial.print(":");
//        Serial.print(timerDigit3);
//        Serial.println(timerDigit4);
fill_solid(leds, NUM_LEDS, CRGB::Black);
TimerDisplay(timerDigit1,1);
TimerDisplay(timerDigit2,2);
TimerDisplay(timerDigit3,3);
TimerDisplay(timerDigit4,4);
}

void pGameTimer(){
      fadeToBlackBy( leds, NUM_LEDS, 5);
        hh = currentGameTime / 36000 % 24;
        mm = currentGameTime / 600 % 60;
        ss = currentGameTime / 10 % 60;
        ms = currentGameTime  % 10;
       // Serial.print("Time: ");Serial.print(hh);Serial.print(":");Serial.print(mm);Serial.print(":");Serial.print(ss);Serial.print(":");Serial.println(ms);      
        if(currentGameTime>36000){timerDigit1=hh/10; timerDigit2=hh%10; timerDigit3=mm/10; timerDigit4=mm%10;}
        if(currentGameTime<=36000 && currentGameTime>1800){timerDigit1=NULL; timerDigit2=NULL; timerDigit3=mm/10; timerDigit4=mm%10;}
        //MOVING THE TIME OVER
        if(currentGameTime<=1800 && currentGameTime>1720){timerDigit1=NULL; timerDigit2=NULL; timerDigit3=NULL; timerDigit4=mm%10;}
        if(currentGameTime<=1720 && currentGameTime>1710){timerDigit1=NULL; timerDigit2=NULL; timerDigit4=NULL; timerDigit3=mm%10;}
        if(currentGameTime<=1710 && currentGameTime>1700){timerDigit1=NULL; timerDigit4=NULL; timerDigit3=NULL; timerDigit2=mm%10;}
        
        if(currentGameTime<=1700 && currentGameTime>600){timerDigit1=NULL; timerDigit2=mm%10; timerDigit3=ss/10; timerDigit4=ss%10;}
        
        if(currentGameTime<=600 && currentGameTime>580){timerDigit1=ss/10; timerDigit2=ss%10; timerDigit3=ss/10; timerDigit4=ss%10; }
        if(currentGameTime<=580 && currentGameTime>100){timerDigit1=ss/10; timerDigit2=ss%10; timerDigit3=NULL; timerDigit4=NULL;}
        if(currentGameTime<=100){timerDigit1=ss/10; timerDigit2=ss%10; timerDigit3=ms; timerDigit4=NULL;}
        if(currentGameTime<=0){pauseTimer = true; timerDisplayDelay++;}
        
        if(timerDisplayDelay>=50){setTimer = true;}

//        Serial.print(timerDigit1);
//        Serial.print(timerDigit2);
//        Serial.print(":");
//        Serial.print(timerDigit3);
//        Serial.println(timerDigit4);
fill_solid(leds, NUM_LEDS, CRGB::Black);
TimerDisplay(timerDigit1,1);
TimerDisplay(timerDigit2,2);
TimerDisplay(timerDigit3,3);
TimerDisplay(timerDigit4,4);
}

void TimerDisplay(int8_t score, int8_t digit) {
  digit--;
  if(score ==-1){return;}
  int8_t s = num[score][0];                                     //number of the leds in the array used in the for loop (first digit of the num sub array's)
  for (int8_t i = 1 ; i < s; i++) {                             //loop to set LED colors of the "tens" place. d1 means digit 1
    int16_t k = num[score][i] + jDigitOffset[digit];            // Setting the correct pixel and the group offset
      if(colonCount ==10  ){colonOn = true;}
      if(colonCount >=15){colonOn = false;  colonCount = 0;}
     
      if(colonOn == true){
        leds[50] = CRGB(teamColor[ActiveTeam][0], teamColor[ActiveTeam][1], teamColor[ActiveTeam][2]);
        leds[52] = CRGB(teamColor[ActiveTeam][0], teamColor[ActiveTeam][1], teamColor[ActiveTeam][2]);
      }else{
        leds[50] = CRGB(0,0,0);
        leds[52] = CRGB(0,0,0);
      }
    leds[k] = CRGB(255,0,0);
  }
}
void PJudgesScore(int8_t score, int8_t Judge) {
  Judge--;
  if(score ==-1){return;}
  int8_t s = num[score][0];                                     //number of the leds in the array used in the for loop (first digit of the num sub array's)
  for (int8_t i = 1 ; i < s; i++) {                             //loop to set LED colors of the "tens" place. d1 means digit 1
    int16_t k = num[score][i] + jDigitOffset[Judge];            // Setting the correct pixel and the group offset
    leds[k] = CRGB(judgeColor[Judge][0], judgeColor[Judge][1], judgeColor[Judge][2]);
  }
}



typedef void (*Pattern)();
typedef Pattern PatternList[];
typedef struct {
  Pattern pattern;
  String name;
} PatternAndName;
typedef PatternAndName PatternAndNameList[];

// List of patterns to cycle through.  Each is defined as a separate function below.
PatternAndNameList patterns = {
  { colorwaves, "Color Waves" },
  { palettetest, "Palette Test" },
  { pride, "Pride" },
  { rainbow, "Rainbow" },
  { rainbowWithGlitter, "Rainbow With Glitter" },
  { confetti, "Confetti" },
  { sinelon, "Sinelon" },
  { juggle, "Juggle" },
  { bpm, "BPM" },
  { showSolidColor, "Solid Color" },
};

const uint8_t patternCount = ARRAY_SIZE(patterns);

void loop(void) {
  dnsServer.processNextRequest();
  server.handleClient();
  if (power >= 15) {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    FastLED.delay(150);
    return;
  }else{
//Timer
    if(setTimer == true){currentTime = startTime; timerDisplayDelay = 0; setTimer = false;} //set the timer to input
    unsigned long timerNewTime = millis();
  if(timerNewTime - timerOldTime >= interval) {
    timerOldTime = timerNewTime; 
    colonCount++;

    //Serial.println(colonCount);
    if( pauseTimer == false && startTimer == true ){ currentTime--; }
    if( showTimer == true){ pTimer();}

//game timer
    //Serial.println(colonCount);
    if( pauseGameTimer == false && startGameTimer == true ){ currentGameTime--; }
    if( showGameTimer == true){ pGameTimer();}
    }
//pattern
  if (showPatterns == true) {
    pride();
  }

if(blackout == true){fadeToBlackBy( leds, NUM_LEDS, 2);}

if(showScores == true){
  showTimer     = false;
  showGameTimer = false;
  fadeToBlackBy( leds, NUM_LEDS, 10);
  //fill_solid(leds, NUM_LEDS, CRGB::Black);
  PScore(teamScore[0], 0);
  PScore(teamScore[1], 1);
  FastLED.delay(1);  
  }

if(setJudge1>-1 || setJudge2>-1 || setJudge3>-1 || setJudge4>-1){
  showTimer     = false;
  showGameTimer = false;
  showScores    = false;
  showJudges    = true;
  judgeTeamFlare();
 if(setJudge1>4 || setJudge2>4 || setJudge3>4 || setJudge4>4){showFlare = true;}
  //fill_solid(leds, NUM_LEDS, CRGB::Black);
  //leds[50] = CRGB(teamColor[ActiveTeam][0], teamColor[ActiveTeam][1], teamColor[ActiveTeam][2]);
  //leds[51] = CRGB(teamColor[ActiveTeam][0], teamColor[ActiveTeam][1], teamColor[ActiveTeam][2]);
  //leds[52] = CRGB(teamColor[ActiveTeam][0], teamColor[ActiveTeam][1], teamColor[ActiveTeam][2]);
  PJudgesScore(setJudge1,1);
  PJudgesScore(setJudge2,2);
  PJudgesScore(setJudge3,3);
  PJudgesScore(setJudge4,4);
  FastLED.delay(1); 
  }
  
if(commitScores == true){
  showFlare = false;
  fadeToBlackBy( leds, NUM_LEDS, 30);
  COMMITSCORES(); 
}

if(showFlare == true){
  uint8_t count5 = 0 ;
  if(setJudge1 < 5 ){showFlare =false;}
  if(setJudge2 < 5 ){showFlare =false;}
  if(setJudge3 < 5 ){showFlare =false;}
  if(setJudge4 < 5 ){showFlare =false;}
  if(setJudge1 ==5 ){count5++;}
  if(setJudge2 ==5 ){count5++;}
  if(setJudge3 ==5 ){count5++;}
  if(setJudge4 ==5 ){count5++;}
  
  switch(count5){
    case 0:
      break;
    case 1:
      addGlitter(80);
      break;
    case 2:
      addGlitter(150);
      break;
    case 3:
      addGlitter(230);
      confetti();
      break;
    case 4:
      addGlitter(230);
      gHue++;
      confetti();
      
      break;
      
  }
}

  FastLED.show();
  FastLED.delay(1000 / FRAMES_PER_SECOND);
}
}

void wipe(int which)
{
  int rowsCount[] {8,5,2,2,5,2,2,2,5};
  int colsCount[] {5,8,3,3,3,8};
  int stripsCount[]{3,5,5,5};
  int barsCount[] {2,8,8};
  
  int rows[8][5] {{0,21,20,19,18},{1,17},{2,16},{3,22,23,24,15},{4,14},{5,13},{6,12},{7,8,9,10,11}};
  int cols[5][8] {{0,1,2,3,4,5,6,7},{21,22,8},{20,23,9},{19,24,10},{18,17,16,15,14,13,12,11}};
  int strips[3][5] {{0,21,20,19,18},{3,22,23,24,15},{7,8,9,10,11}};
  int bars[2][8] {{0,1,2,3,4,5,6,7},{18,17,16,15,14,13,12,11}};

  switch(which){
    case 0:
    int k=rowsCount[0]; 
      for(int h=0; h>=4; h++){
        for(int j=0 ; j > rowsCount[k] ; j++){          
          for(int i=1 ; i>=k ; i++){
          int pixel = rows[i][j] + jDigitOffset[h];
          leds[pixel] = CHSV(128,128,128);
          FastLED.show();
          FastLED.delay(80);
        }
      }
    }
  }

}

void loadSettings()
{
  brightness = EEPROM.read(0);
  
  currentPatternIndex = EEPROM.read(1);
  if (currentPatternIndex < 0)
    currentPatternIndex = 0;
  else if (currentPatternIndex >= patternCount)
    currentPatternIndex = patternCount - 1;

  byte r = EEPROM.read(2);
  byte g = EEPROM.read(3);
  byte b = EEPROM.read(4);

  if (r == 0 && g == 0 && b == 0)
  {
  }
  else
  {
    solidColor = CRGB(r, g, b);
  }
  
}

void sendAll()
{
  String json = "{";

  json += "\"power\":" + String(power) + ",";
  json += "\"brightness\":" + String(brightness) + ",";
  json += "\"team\":" + String(ActiveTeam) + ",";
  json += "\"currentPattern\":{";
  json += "\"index\":" + String(currentPatternIndex);
  json += ",\"name\":\"" + patterns[currentPatternIndex].name + "\"}";

  json += ",\"solidColor\":{";
  json += "\"r\":" + String(solidColor.r);
  json += ",\"g\":" + String(solidColor.g);
  json += ",\"b\":" + String(solidColor.b);
  json += "}";

  json += ",\"patterns\":[";
  for (uint8_t i = 0; i < patternCount; i++)
  {
    json += "\"" + patterns[i].name + "\"";
    if (i < patternCount - 1)
      json += ",";
  }
  json += "]";

  json += "}";

  server.send(200, "text/json", json);
  json = String();
}

void sendPower()
{
  String json = String(power);
  server.send(200, "text/json", json);
  json = String();
}

void sendTeam()
{
    String json = String(ActiveTeam);
    server.send(200, "text/json", json);
    json = String();
}

void sendPattern()
{
  String json = "{";
  json += "\"index\":" + String(currentPatternIndex);
  json += ",\"name\":\"" + patterns[currentPatternIndex].name + "\"";
  json += "}";
  server.send(200, "text/json", json);
  json = String();
}

void sendBrightness()
{
  String json = String(brightness);
  server.send(200, "text/json", json);
  json = String();
}

void sendSolidColor()
{
  String json = "{";
  json += "\"r\":" + String(solidColor.r);
  json += ",\"g\":" + String(solidColor.g);
  json += ",\"b\":" + String(solidColor.b);
  json += "}";
  server.send(200, "text/json", json);
  json = String();
}

void setPower(uint8_t value)
{
  power = value == 0 ? 0 : 1;
}

void setSolidColor(CRGB color)
{
  setSolidColor(color.r, color.g, color.b);
}

void COMMITSCORES(){
  if(setJudge1 >-1){  teamScore[ActiveTeam] += setJudge1;  setJudge1 = -1;}
  if(setJudge2 >-1){  teamScore[ActiveTeam] += setJudge2;  setJudge2 = -1;}
  if(setJudge3 >-1){  teamScore[ActiveTeam] += setJudge3;  setJudge3 = -1;}
  if(setJudge4 >-1){  teamScore[ActiveTeam] += setJudge4;  setJudge4 = -1;}
  showJudges = false;
  showScores = true;
  commitScores=false;
}

void setSolidColor(uint8_t r, uint8_t g, uint8_t b)
{
  solidColor = CRGB(r, g, b);

  EEPROM.write(2, r);
  EEPROM.write(3, g);
  EEPROM.write(4, b);

  setPattern(patternCount - 1);
}

// increase or decrease the current pattern number, and wrap around at the ends
void adjustPattern(bool up)
{
  if (up)
    currentPatternIndex++;
  else
    currentPatternIndex--;

  // wrap around at the ends
  if (currentPatternIndex < 0)
    currentPatternIndex = patternCount - 1;
  if (currentPatternIndex >= patternCount)
    currentPatternIndex = 0;

  EEPROM.write(1, currentPatternIndex);
  EEPROM.commit();
}

void setPattern(int value)
{
  // don't wrap around at the ends
  if (value < 0)
    value = 0;
  else if (value >= patternCount)
    value = patternCount - 1;

  currentPatternIndex = value;

  EEPROM.write(1, currentPatternIndex);
  EEPROM.commit();
}

// adjust the brightness, and wrap around at the ends
void adjustBrightness(bool up)
{
  if (up)
    brightnessIndex++;
  else
    brightnessIndex--;

  // wrap around at the ends
  if (brightnessIndex < 0)
    brightnessIndex = brightnessCount - 1;
  else if (brightnessIndex >= brightnessCount)
    brightnessIndex = 0;

  brightness = brightnessMap[brightnessIndex];

  FastLED.setBrightness(brightness);

  EEPROM.write(0, brightness);
  EEPROM.commit();
}

void setBrightness(int value)
{
  // don't wrap around at the ends
  if (value > 255)
    value = 255;
  else if (value < 0) value = 0;

  brightness = value;

  FastLED.setBrightness(brightness);

  EEPROM.write(0, brightness);
  EEPROM.commit();
}


void judgeTeamFlare()
{
  // to show which team is active during judging
  fadeToBlackBy( leds, NUM_LEDS, 15);
  int pos = beatsin16(40, 0, NUM_LEDS - 1);
  leds[pos] += CRGB(teamColor[ActiveTeam][0], teamColor[ActiveTeam][1], teamColor[ActiveTeam][2]); 
  fadeToBlackBy( leds, NUM_LEDS, 30);
}

void showSolidColor()
{
  fill_solid(leds, NUM_LEDS, solidColor);
}

void rainbow()
{
  // FastLED's built-in rainbow generator
  fill_rainbow( leds, NUM_LEDS, gHue, 10);
}

void rainbowWithGlitter()
{
  // built-in FastLED rainbow, plus some random sparkly glitter
  rainbow();
  addGlitter(80);
}

void addGlitter( fract8 chanceOfGlitter)
{
  if ( random8() < chanceOfGlitter) {
    leds[ random16(NUM_LEDS) ] += CRGB::White;
  }
}

void confetti()
{
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy( leds, NUM_LEDS, 10);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV( gHue + random8(64), 200, 255);
}

void sinelon()
{
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy( leds, NUM_LEDS, 20);
  int pos = beatsin16(30, 0, NUM_LEDS - 1);
  leds[pos] += CRGB(teamColor[ActiveTeam][0], teamColor[ActiveTeam][1], teamColor[ActiveTeam][2]);
  //gHue = gHue + random(1,10);
}

void bpm()
{
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 62;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
  for ( int i = 0; i < NUM_LEDS; i++) { //9948
    leds[i] = ColorFromPalette(palette, gHue + (i * 2), beat - gHue + (i * 10));
  }
}

void juggle()
{
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy( leds, NUM_LEDS, 20);
  byte dothue = 0;
  for ( int i = 0; i < 8; i++)
  {
    leds[beatsin16(i + 7, 0, NUM_LEDS)] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}

// Pride2015 by Mark Kriegsman: https://gist.github.com/kriegsman/964de772d64c502760e5
// This function draws rainbows with an ever-changing,
// widely-varying set of parameters.
void pride() {
  static uint16_t sPseudotime = 0;
  static uint16_t sLastMillis = 0;
  static uint16_t sHue16 = 0;

  uint8_t sat8 = beatsin88( 87, 220, 250);
  uint8_t brightdepth = beatsin88( 341, 96, 224);
  uint16_t brightnessthetainc16 = beatsin88( 203, (25 * 256), (40 * 256));
  uint8_t msmultiplier = beatsin88(147, 23, 60);

  uint16_t hue16 = sHue16;//gHue * 256;
  uint16_t hueinc16 = beatsin88(113, 1, 3000);

  uint16_t ms = millis();
  uint16_t deltams = ms - sLastMillis ;
  sLastMillis  = ms;
  sPseudotime += deltams * msmultiplier;
  sHue16 += deltams * beatsin88( 400, 5, 9);
  uint16_t brightnesstheta16 = sPseudotime;

  for ( uint16_t i = 0 ; i < NUM_LEDS; i++) {
    hue16 += hueinc16;
    uint8_t hue8 = hue16 / 256;

    brightnesstheta16  += brightnessthetainc16;
    uint16_t b16 = sin16( brightnesstheta16  ) + 32768;

    uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
    uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
    bri8 += (255 - brightdepth);

    CRGB newcolor = CHSV( hue8, sat8, bri8);

    nblend(leds[i], newcolor, 64);
  }
}

// ColorWavesWithPalettes by Mark Kriegsman: https://gist.github.com/kriegsman/8281905786e8b2632aeb
// This function draws color waves with an ever-changing,
// widely-varying set of parameters, using a color palette.
void colorwaves()
{
  static uint16_t sPseudotime = 0;
  static uint16_t sLastMillis = 0;
  static uint16_t sHue16 = 0;

  // uint8_t sat8 = beatsin88( 87, 220, 250);
  uint8_t brightdepth = beatsin88( 341, 96, 224);
  uint16_t brightnessthetainc16 = beatsin88( 203, (25 * 256), (40 * 256));
  uint8_t msmultiplier = beatsin88(147, 23, 60);

  uint16_t hue16 = sHue16;//gHue * 256;
  uint16_t hueinc16 = beatsin88(113, 300, 1500);

  uint16_t ms = millis();
  uint16_t deltams = ms - sLastMillis ;
  sLastMillis  = ms;
  sPseudotime += deltams * msmultiplier;
  sHue16 += deltams * beatsin88( 400, 5, 9);
  uint16_t brightnesstheta16 = sPseudotime;

  for ( uint16_t i = 0 ; i < NUM_LEDS; i++) {
    hue16 += hueinc16;
    uint8_t hue8 = hue16 / 256;
    uint16_t h16_128 = hue16 >> 7;
    if ( h16_128 & 0x100) {
      hue8 = 255 - (h16_128 >> 1);
    } else {
      hue8 = h16_128 >> 1;
    }

    brightnesstheta16  += brightnessthetainc16;
    uint16_t b16 = sin16( brightnesstheta16  ) + 32768;

    uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
    uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
    bri8 += (255 - brightdepth);

    uint8_t index = hue8;
    //index = triwave8( index);
    index = scale8( index, 240);

    CRGB newcolor = ColorFromPalette(gCurrentPalette, index, bri8);

    nblend(leds[i], newcolor, 128);
  }
}

// Alternate rendering function just scrolls the current palette
// across the defined LED strip.
void palettetest()
{
  static uint8_t startindex = 0;
  startindex--;
  fill_palette( leds, NUM_LEDS, startindex, (256 / NUM_LEDS) + 1, gCurrentPalette, 255, LINEARBLEND);
}


