#ifndef ENCODER_H
#define ENCODER_H

#include <Arduino.h>
#include <Wire.h>

#define AS5600_AS5601_DEV_ADDRESS 0x36
#define AS5600_AS5601_REG_RAW_ANGLE 0x0C
#define AS5600_ZPOS 0x01
#define AS5600_MANG 0x05
#define LUT_SIZE 1024

extern float initialAngle;
extern float previousHeight;
extern bool isReferenceSet;
extern float newScale;
extern float heightOffset;

void setZeroPosition(uint16_t zeroPosition);
void setMaxAngle(uint16_t maxAngle);
float readEncoderAngle();
float readEncoderAngleOversampled(uint16_t samples = 1024);
void restoreZeroPositionFromEEPROM();
void setInitialAngleFromSensor();
void loadInitialAngleFromEEPROM();
float updateHeight();
void calibrationMode();

void saveCalibrationToEEPROM(float newScale,float newOffset);
float interpolateHeight(float angle);

// LUT calulation functions
void generateHeightLUT(float* knownAngles,float* knownHeights, int nPoints);
float getHeightFromLUT(float angle);
void storeLUTToEEPROM(int baseAddr = 600);
void loadLUTFromEEPROM(int baseAddr = 600);


#endif