#ifndef SLEEP_H
#define SLEEP_H

void updateSleepStatus(float currentHeight, int tftPowerPin);
void handleSleepLED(int ledPin);
void blinkLed(int ledPin);
void ledOff();
bool isSleeping();

 extern unsigned long lastInteractionTime;

#endif