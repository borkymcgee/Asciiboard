/**************************************************************************
* Asciiboard, Copyright (C) 2022 Juno Presken
* E-mail: juno.presken+github@protonmail.com
*
* This program is free software: you can redistribute it and/or modify it
* under the terms of the GNU Affero General Public License as published by the
* Free Software Foundation, either version 3 of the License, or (at your
* option) any later version.

* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General
* Public License for more details.

* You should have received a copy of the GNU Affero General Public License
* along with this program. If not, see <https://www.gnu.org/licenses/>.
*
**************************************************************************/

#include <BleKeyboard.h>
#include <Wire.h> // Needed for I2C   B"B@BB
#include <SparkFun_MAX1704x_Fuel_Gauge_Arduino_Library.h>//for battery


BleKeyboard bleKeyboard("Asciiboard", "Juners", 69);
SFE_MAX1704X lipo(MAX1704X_MAX17048); // Create a MAX17048


//pin numbers for input pins
const int inputPins[] = {12,27,33,15,22,13,32,14};

//rtc: 14,32,12,27,33

//pins held low for testing lol
//const int lowPins[] = {26,39,18,16,13,33,14};

//millis last time a button was pressed
unsigned long lastPress = 0;

//current state of the keyboard buttons
char buttonState = 0;

//number of millis after first thumbpress to accept the character pressed
int countDownTime = 1000;


void IRAM_ATTR Ext_INT1_ISR(){

  //grab the thumb button data and put it in buttonState
  if(!digitalRead(inputPins[4])) buttonState |= (1<<4);
  if(!digitalRead(inputPins[5])) buttonState |= (1<<5);
  if(!digitalRead(inputPins[6])) buttonState |= (1<<6);
  if(!digitalRead(inputPins[7])) buttonState |= (1<<7);
}

void setup() {

  //slow down to conserve power
  setCpuFrequencyMhz(80);


  //attach wake trigger to pin 33
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_33,0);

  Serial.begin(115200);
  Wire.begin();
  lipo.begin();
  //saves a lil power by only checking SOC every 45s, probably doesn't actually help much
  //lipo.enableHibernate();

  //low battery level set to 5%
  lipo.setThreshold(5);

  if(lipo.isLow()){
    Serial.println("getting eepy");
    esp_deep_sleep_start();
  }

  bleKeyboard.begin();  //update battery level
  bleKeyboard.setBatteryLevel(int(lipo.getSOC()));

  //set all buttons as input pullup
  for(int i=0; i<8; i++) pinMode(inputPins[i], INPUT_PULLUP);
  //set all testing pins as output
  //for(int i=0; i<7; i++) pinMode(lowPins[i], OUTPUT);
  //digitalwrite 0 to all the testing pins
  //for(int i=0; i<6; i++) digitalWrite(lowPins[i], false);

  //attach interrupt to the thumb buttons
  attachInterrupt(inputPins[7], Ext_INT1_ISR, FALLING); 
  attachInterrupt(inputPins[6], Ext_INT1_ISR, FALLING);
  attachInterrupt(inputPins[5], Ext_INT1_ISR, FALLING);
  attachInterrupt(inputPins[4], Ext_INT1_ISR, FALLING);


  //Serial.print("CPU freq: ");
  //Serial.println(getCpuFrequencyMhz());
}

void loop() {

  //30m after last pressing a button or if battery is empty go for deep sleep
  if(millis() - lastPress > 1800000/* || lipo.isLow()*/){
    Serial.println("getting eepy");
    esp_deep_sleep_start();
  }

  //if a thumb button has been pressed
  if(buttonState != 0){
    //get the rest of the buttons
    if(!digitalRead(inputPins[0])) buttonState |= (1<<0);
    if(!digitalRead(inputPins[1])) buttonState |= (1<<1);
    if(!digitalRead(inputPins[2])) buttonState |= (1<<2);
    if(!digitalRead(inputPins[3])) buttonState |= (1<<3); 
    //wait for any other thumb buttons to be pressed
    delay(countDownTime);
    //catch control characters
    if(bitRead(buttonState,7)){
      switch(buttonState){
        case 0b10000011:  //ETX, use to send
          bleKeyboard.press(KEY_LEFT_CTRL);
          bleKeyboard.press(KEY_RETURN);
          bleKeyboard.releaseAll();
          break;
        case 0b10000100:  //EOT, use to send
          bleKeyboard.press(KEY_LEFT_CTRL);
          bleKeyboard.press(KEY_RETURN);
          bleKeyboard.releaseAll();
          break;
        case 0b10001000:  //BS
          bleKeyboard.write(KEY_BACKSPACE);
          break;
        case 0b10001001:  //HT
          bleKeyboard.write(KEY_TAB);
          break;
        case 0b10001010:  //LF
          bleKeyboard.write(KEY_RETURN);
          break;
        case 0b10001101:  //CR 
          bleKeyboard.write(KEY_RETURN);
          break;
        case 0b10011010:  //SUB
          bleKeyboard.press(KEY_LEFT_CTRL);
          bleKeyboard.press('z');
          bleKeyboard.releaseAll();
          break;
        case 0b10011011:  //ESC
          bleKeyboard.write(KEY_ESC);
          break;
        default:
          bleKeyboard.write(buttonState);
          break;
      }           
    }else{  //normal characters
      //manually send delete
      if(buttonState == 0b01111111) bleKeyboard.write(KEY_DELETE);
      bleKeyboard.print(buttonState);
    }
    //update battery
    bleKeyboard.setBatteryLevel(int(lipo.getSOC()));
    //print to serial for debugging
    Serial.println(buttonState);
    //record time for use in sleep function
    lastPress = millis();
    buttonState = 0;
  }

  //slow down the loop for battery saving, 10 might be like, 1mA lower but meh
  delay(1);
}
