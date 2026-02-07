#include "sleep.h"
#include <Arduino.h>

bool sleepMode = 0;
unsigned long lastInteractionTime = 0;
const unsigned long inactivityThreshold = 60000;
const float sleepValue = 1.0;
static float heightForSleep = 0.0f;

int fadeValue = 0;
int fadeAmount = 10;
bool fadeDirectionUp = true;

void updateSleepStatus(float currentHeight, int tftPowerPin){
        if(abs(currentHeight - heightForSleep) > sleepValue){
        lastInteractionTime = millis();
        heightForSleep = currentHeight;
    }

    if(millis() - lastInteractionTime > inactivityThreshold){
        digitalWrite(tftPowerPin,LOW);
        sleepMode = 1;
    } else {
        digitalWrite(tftPowerPin, HIGH);
        sleepMode = 0;
    }

}

void blinkLed(int ledPin){
      static unsigned long lastToggle = 0;
      static bool ledState = false;

      if(millis() - lastToggle >= 500){
          lastToggle = millis();
          ledState = !ledState;
          digitalWrite(ledPin, ledState ? HIGH : LOW);
      }
}

void handleSleepLED(int ledPin){
        if(sleepMode == 1){
                                             
                if(fadeDirectionUp){
                    fadeValue += fadeAmount;
                    if(fadeValue >= 150){
                        fadeValue = 150;
                        fadeDirectionUp = false;
                    }
                }else{
                    fadeValue -= fadeAmount;
                    if(fadeValue <= 0){
                        fadeValue = 0;
                        fadeDirectionUp = true;
                    }
                }
                analogWrite(ledPin,fadeValue);
            } 
}

bool isSleeping(){
    return sleepMode;
}
