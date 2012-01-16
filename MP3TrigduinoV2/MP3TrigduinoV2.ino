/*
  MP3Trigduino
  @author Carl Jensen
  @url  dimsos.fridata.dk
  @github  https://github.com/carlfj/

  This sketch requires Arduino 1.0 since it is using SoftwareSerial.
  It also requires the updated version of MP3Trigger, currently forked on github at
  https://github.com/carlfj/MP3Trigger-for-Arduino
 */


#include <SoftwareSerial.h>
#include <EEPROM.h>
#include <MP3TriggerSoftwareSerial.h>
//#include <Debug.h>

// Number of sample trigger buttons, excluding alternative buttons.
// Trigger buttons start from the pin 2 to keep serial port free.
const byte NUMTRIG = 18;

// Analog pin for trigger
const byte MAXVOL = 63; // only one byte allocated in EEPROM per trigger
const byte volThr = 1;
const int volEepromAdr = 0;
boolean liveVolume = true;

byte volumes[NUMTRIG];
int lastTriggerVol = MAXVOL;
byte lastTriggered = 0;

SoftwareSerial trigSerial = SoftwareSerial(2, 3);
MP3TriggerSS trigger;
//Debug d = Debug(true, Serial);
const boolean DEBUG = true;


void setup() {
  // Resetting volumes array from EEPROM
  loadVolumes();
  
  // init trigger  
  //trigger.setup(); // Using Serial
  trigger.setup(&trigSerial); // Using SoftwareSerial
  Serial.begin(9600);

  //setupEncoderPins(&PINB, 8, 9, 0, 1);
  setupEncoder();
  
  trigger.quietMode(1, quietModeTriggered);
}


boolean waitForTriggerToBoot = true;
void loop() {
  updateAndReadInput();
  
  if(waitForTriggerToBoot && millis() > 4000) {
    trigger.quietMode(1, quietModeTriggered);
    waitForTriggerToBoot = false;
  }
}


void triggerWithVol(byte t) {
  if(t >= NUMTRIG) return; // assert t < NUMTRIG, e.g. 0 - 17

  if(volumes[t] != lastTriggerVol) {
    lastTriggerVol = volumes[t];
    setTriggerVolume(lastTriggerVol);
  }
  // Trigger value mapped to values  1 - NUMTRIG e.g. 1 - 18
  // so mp3 files can be called TRACK001.mp3 to TRACK016.mp3.
  // If they are mapped directly to pin files would have to be named ex: TRACK007.mp3 to TRACK023.mp3.
  //          trigger.trigger(t1);
  trigger.trigger(t + 1);
  lastTriggered = t;

  //d.debugVar("lastTriggered ", t);
}


void setTriggerVolume(int newVol) {
    trigger.setVolume( constrain(newVol, 0, MAXVOL) );
}

void quietModeTriggered(int i) {
  triggerWithVol(i - 1);
}

void updateAndReadInput() {
  trigger.update();
  
  int readVolDiff = readVolumeEncoder();
  if(readVolDiff != 0) {
    int lastTrigNewVol = volumes[lastTriggered] + readVolDiff;
    //d.debugVarNoln("lastTr ", lastTriggered);
    //d.debugVar("newVol ", lastTrigNewVol);
    
    storeVolume(lastTriggered, lastTrigNewVol);
    if(liveVolume) {
      setTriggerVolume(lastTrigNewVol);
    }
  }

}


/*
 * Stores volume to volume array and EEPROM
 * Input: byte i 
 */
void storeVolume(byte i, int volume) {
  if(i >= NUMTRIG) return; // assert t < NUMTRIG
  volumes[i] = constrain(volume, 0, MAXVOL);
  EEPROM.write(volEepromAdr + i, volumes[i]); // address is volEepromAdr + trigPin
}


void loadVolumes() {
  for (int i = 0; i < NUMTRIG; i++) {
    volumes[i] = EEPROM.read(volEepromAdr + i);
    if (volumes[i] > MAXVOL) {
      // volume value is bad data, resetting. Thouhg this means that new chips kan have arbitrary volume,
      // but not out of value range.
      volumes[i] = MAXVOL;
      EEPROM.write(volEepromAdr + i, MAXVOL);
    }
    
    //To reset all volumes to max:
    //EEPROM.write(volEepromAdr + i, 0);
  }
}


/*
 * From MP3TriggerV2UserGuide_2010-07-30.pdf :
 *     Comments:  The VS1053 volume will be set to the value n. Per the VS1053 datas  heet,
 *     maximum volume is 0x00, and values much above 0x40 (64, red.) are too low to be audible.
 *
 * Volume is mapped in reverse so input value from 0 to 1023 becomes sent value from 63 to 0.
 */
int readVolumeEncoder() {
  static int lastEncoderCount = 0;
  
  //loopEncoderPin();
  //int encCount = getEncoderCounter();
  
  //static uint8_t counter = 0;
  static int encCount = 0;
  int8_t tmpdata = read_encoder();
  if( tmpdata ) {
    encCount += tmpdata;
  }
  if(encCount == 0) return 0;
  
  int volDiff = encCount; //map(encCount, 0, 255, 0, MAXVOL);
  if( abs(volDiff) >= volThr) {
    encCount = 0; // lastEncoderCount = encCount;
    return volDiff;
  }
  else {
    // change is less than threshold volThr, report no change
    return 0;
  }
}


/* Rotary encoder routines. Exists as a library, included here for easier code portatbility */
#define ENC_A 9
#define ENC_B 8
#define ENC_PORT PINB

void setupEncoder() {
  pinMode(ENC_A, INPUT);
  digitalWrite(ENC_A, HIGH); 
  pinMode(ENC_B, INPUT);
  digitalWrite(ENC_B, HIGH);
}

int8_t read_encoder()
{
  int8_t enc_states[] = {0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0}; 
  static uint8_t old_AB = 0;
  old_AB <<= 2;                   //remember previous state
  old_AB |= ( ENC_PORT & 0x03 );  //add current state
  return ( enc_states[( old_AB & 0x0f )]);
}






/*

 Using quiet mode to implement triggering from trigger inputs on the MP3 Trigger circuit board:
 
    Command: Quiet Mode (ASCII)  
    Number of bytes:  2  
    Command byte: ‘Q’  
    Data byte:  N = ASCII ‘0’ or ‘1’  
    Comments:  If N=’1’, Quiet mode is turned on. If N=’0’, Quiet mode is turned off.  Default state is off. 
     
     
    MP3 Trigger Outgoing Message Summary 
     
    The MP3 Trigger sends the following ASCII messages:  
      
    ‘X’:  When the currently playing track finishes.  
    ‘x’:  When the currently playing track is cancelled by a new command.  
    ‘E’:  When a requested track doesn’t exist (error).  
      
    In response to a Status Request Command, data byte = ‘0’, the MP3 Trigger sends an 18-byte version string: e.g. 
    “=MP3 Trigger v1.00” . 
      
    In response to a Status Request Command, data byte = ‘1’, the MP3 Trigger sends the number of  MP3 tracks on the 
    currently installed microSD card: e.g. “=14”. 
     
    In Quiet Mode only, when one or more trigger inputs are activated, the MP3 Trigger sends ‘M’ followed by a 3-byte bit 
    mask indicating which triggers were activated: 
     
     Data byte 0:  TRIG01 through TRIG08 
     Data byte 1:  TRIG09 through TRIG16 
     Data byte 2:  TRIG17 and TRIG18 
     
    A value of 1 in a bit position indicates that the corresponding trigger input was activated. 


*/
