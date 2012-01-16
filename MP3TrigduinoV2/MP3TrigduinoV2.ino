/*
  MP3Trigduino
  @author Carl Jensen
  @url  dimsos.fridata.dk
  @github  https://github.com/carlfj/

  This sketch requires Arduino 1.0 since it is using SoftwareSerial.
  It also requires the updated version of MP3Trigger, currently forked on github at
  https://github.com/carlfj/Arduino-MP3-sample-player-with-volumes
 */


#include <SoftwareSerial.h>
#include <EEPROM.h>
//#include <MP3Trigger.h> // Using Serial
#include <MP3TriggerSoftwareSerial.h> // Using SoftwareSerial
SoftwareSerial trigSerial = SoftwareSerial(2, 3);

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

MP3TriggerSS trigger;
//Debug d = Debug(true, Serial);
const boolean DEBUG = true;


void setup() {
  // Resetting volumes array from EEPROM
  loadVolumes();
  
  // init trigger  
  //trigger.setup(); // Using Serial
  trigger.setup(&trigSerial); // Using SoftwareSerial

  //setupEncoderPins(&PINB, 8, 9, 0, 1);
  setupEncoder();
  
  //Using quiet mode to implement triggering from trigger inputs on the MP3 Trigger circuit board:
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
  // trigger.trigger(t1);
  trigger.trigger(t + 1);
  lastTriggered = t;

}


/*
 * From MP3TriggerV2UserGuide_2010-07-30.pdf :
 *     Comments:  The VS1053 volume will be set to the value n. Per the VS1053 datasheet,
 *     maximum volume is 0x00, and values much above 0x40 (64, red.) are too low to be audible.
 *
 * Volume is mapped in reverse so input value from 0 to 1023 becomes sent value from 63 to 0.
 */
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
    storeVolume(lastTriggered, lastTrigNewVol);
    if(liveVolume) {
      setTriggerVolume(lastTrigNewVol);
    }
  }

}


/*
 * Stores volume to volume array and EEPROM
 * Input: byte i       input no. (0 - NUMTRIG-1)
 *        int  volume  volume (0 - MAXVOL)
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
    //These lines can be removed.
  }
}


int readVolumeEncoder() {
  static int encCount = 0;
  int8_t tmpdata = read_encoder();
  if( tmpdata ) {
    encCount += tmpdata;
  }
  if(encCount == 0) return 0;
  
  //encCount = map(encCount, 0, 255, 0, MAXVOL); // can be mapped to other scale
  if( abs(encCount) >= volThr) {
    int volDiff = encCount;
    encCount = 0; // Only reset encCouont
    return volDiff;
  }
  else {
    // change is less than threshold volThr, report no change
    return 0;
  }
}


/* Rotary encoder routines. Exists as a library, included directly below for easier code portatbility */
#define ENC_A 9
#define ENC_B 8
//#define ENC_PORT PIND // Use for digital pins 0-7
#define ENC_PORT PINB // Use for digital pins 8-13
//#define ENC_PORT PINC // Use for Analog pins



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

