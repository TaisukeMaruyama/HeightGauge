#include "encoder.h"
#include <EEPROM.h>
#include <Wire.h>
#include <math.h>

// AS5600 register //
#define AS5600_AS5601_DEV_ADDRESS 0x36
#define AS5600_AS5601_REG_RAW_ANGLE 0x0C
#define AS5600_ZMC0 0x00
#define AS5600_ZPOS 0x01
#define AS5600_MPOS 0x03
#define AS5600_MANG 0x05

// parameter //
bool isReferenceSet = false;
float currentAngle = 0;
float previousHeight = NAN;
float initialAngle = 0;
float relativeAngle = 0;
const float proveLength = 80.68612f;
const float caribHeight = 5.0f;
const float minHeight = 1.9f;
const float maxHeight = 50.0f;
float height = 0.0;
float smoothHeight = 0.0;
const float smoothingFactor = 0.7f; //RC Fillter

float scaleFactor = 1.394124f;
float offset = 0.0f;
float heightOffset = 0.0f;

static float heightLUT[LUT_SIZE];
static bool lutready = false;
static float minLUTAngle = 0.0f;
static float maxLUTAngle = 90.0f;


float interpolateHeight(float angle){
    const int NUM_POINTS = 5;
    float knownAngles[NUM_POINTS];
    float knownHeight[NUM_POINTS];

    for(int i=0; i<NUM_POINTS; i++){
        EEPROM.get(100+i*sizeof(float),knownAngles[i]);
        EEPROM.get(200+i*sizeof(float),knownHeight[i]);
    }

    for(int i=0; i<NUM_POINTS-1; i++){
        for(int j=i+1; j<NUM_POINTS; j++){
            if(knownAngles[i] > knownAngles[j]){
                float tmpA = knownAngles[i];
                knownAngles[i]=knownAngles[j];
                knownAngles[j]=tmpA;
                float tmpH = knownHeight[i];
                knownHeight[i] = knownHeight[j];
                knownHeight[j] = tmpH;
            }
        }
    }


    if(angle <= knownAngles[0]){
        float t = (angle - knownAngles[0])/ (knownAngles[1]-knownAngles[0]);
        return knownHeight[0] + t * (knownHeight[1] - knownHeight[0]);
    }
    if(angle >= knownAngles[NUM_POINTS -1]){
        float t = (angle - knownAngles[NUM_POINTS -2])/(knownAngles[NUM_POINTS - 1] - knownAngles[NUM_POINTS - 2]);
        return knownHeight[NUM_POINTS -2]+t*(knownHeight[NUM_POINTS-1]-knownHeight[NUM_POINTS -2]);
    } 
   
    for(int i=0; i<NUM_POINTS-1; i++){
        if(angle >= knownAngles[i] && angle <= knownAngles[i+1]){
            float t = (angle - knownAngles[i]) / (knownAngles[i+1] - knownAngles[i]);
            return knownHeight[i] + t * (knownHeight[i+1] - knownHeight[i]);
        }
    }
    return 0.0f;
}

void generateHeightLUT(float* knownAngles,float* knownHeights, int nPoints){
    for(int i=0; i<nPoints-1; i++){        
        for(int j=i+1; j<nPoints; j++){
        if(knownAngles[i] > knownAngles[j]){
            float ta = knownAngles[i];
            knownAngles[i] = knownAngles[j];
            knownAngles[j] = ta;
            float th = knownHeights[i];
            knownHeights[i] = knownHeights[j];
            knownHeights[j] = th;
        }
    }
}
  minLUTAngle = knownAngles[0];
  maxLUTAngle = knownAngles[nPoints -1];
      for(int i=0; i<LUT_SIZE; i++){
            float angle = minLUTAngle + (maxLUTAngle - minLUTAngle) * ((float)i / (LUT_SIZE -1));
            //find interval
            int idx = 0;
            while (idx < nPoints-2 && angle > knownAngles[idx+1]) idx++;
            float t = (angle - knownAngles[idx]) / (knownAngles[idx+1] - knownAngles[idx]);
            float h = knownHeights[idx] + t * (knownHeights[idx+1] - knownHeights[idx]);
            heightLUT[i] = h;
            
        }     
    
    lutready = true;    
}

float getHeightFromLUT(float angle){
    if(!lutready) return 0.0f;
    if(angle <= minLUTAngle) return heightLUT[0];
    if(angle >= maxLUTAngle) return heightLUT[LUT_SIZE -1];
    float idxF = (angle - minLUTAngle) / (maxLUTAngle - minLUTAngle) * (LUT_SIZE -1);
    int idx = (int) idxF;
    float frac = idxF - idx;
    float h =  heightLUT[idx] + frac * (heightLUT[idx +1] - heightLUT[idx]);
    return h;
}

void storeLUTToEEPROM(int baseAddr){
    EEPROM.put(baseAddr,minLUTAngle);
     baseAddr += sizeof(float);
    EEPROM.put(baseAddr,maxLUTAngle);
     baseAddr += sizeof(float);
    for(int i=0; i<LUT_SIZE; i++){
        EEPROM.put(baseAddr,heightLUT[i]);
        baseAddr += sizeof(float);
    }    
}

void loadLUTFromEEPROM(int baseAddr){
    EEPROM.get(baseAddr,minLUTAngle);
     baseAddr += sizeof(float);
    EEPROM.get(baseAddr,maxLUTAngle);
     baseAddr += sizeof(float);
    for(int i=0; i<LUT_SIZE; i++){
        EEPROM.get(baseAddr,heightLUT[i]);
        baseAddr += sizeof(float);
    }
    lutready = true;
}

void initEncorder(){
    Wire.begin();
    Wire.setClock(400000);
    restoreZeroPositionFromEEPROM();

    EEPROM.get(2,initialAngle);
    isReferenceSet = true;

}

void setZeroPosition(uint16_t zeroPosition) {
    Wire.beginTransmission(AS5600_AS5601_DEV_ADDRESS);
    Wire.write(AS5600_ZPOS);
    Wire.write(zeroPosition >> 8);
    Wire.write(zeroPosition & 0xFF);
    Wire.endTransmission();
}

void setMaxAngle(uint16_t maxAngle) {
    Wire.beginTransmission(AS5600_AS5601_DEV_ADDRESS);
    Wire.write(AS5600_MANG);
    Wire.write(maxAngle >> 8);
    Wire.write(maxAngle & 0xFF);
    Wire.endTransmission();
}

float readEncoderAngle() {
    Wire.beginTransmission(AS5600_AS5601_DEV_ADDRESS);
    Wire.write(AS5600_AS5601_REG_RAW_ANGLE);
    Wire.endTransmission(false);
    Wire.requestFrom(AS5600_AS5601_DEV_ADDRESS, 2);

    uint8_t highByte = Wire.read();
    uint8_t lowByte = Wire.read();

    uint16_t RawAngle = ((uint16_t)highByte << 8) | lowByte;
    // uint16_t invertedAngle = 4095 - RawAngle;
    return RawAngle * (360.0 / 4096.0);
}

float readEncoderAngleOversampled(uint16_t samples = 1024) {
    double sum =0.0;
    for (uint16_t i = 0; i < samples; i++) {
        sum += readEncoderAngle();
        delayMicroseconds(50);
    }
    return (float) (sum / samples);
}

void saveCurrentZeroPositionToEEPROM(){
    Wire.beginTransmission(AS5600_AS5601_DEV_ADDRESS);
    Wire.write(AS5600_AS5601_REG_RAW_ANGLE);
    Wire.endTransmission(false);
    Wire.requestFrom(AS5600_AS5601_DEV_ADDRESS,2);
    uint8_t highByte = Wire.read();
    uint8_t lowByte = Wire.read();
    uint16_t rawAngle = ((uint16_t)highByte << 8) | lowByte;

    setZeroPosition(rawAngle);

    EEPROM.put(0,rawAngle);
    
}

void restoreZeroPositionFromEEPROM(){
    uint16_t rawAngle;
    EEPROM.get(0,rawAngle);
    setZeroPosition(rawAngle);
}

void setInitialAngleFromSensor(){
    initialAngle = readEncoderAngle();
    EEPROM.put(2,initialAngle);
    isReferenceSet = true;  
}


float updateHeight(){
    if(!isReferenceSet) return 0.0f;
    currentAngle = readEncoderAngleOversampled(256);
    float relativeAngle = currentAngle - initialAngle;
    // float relativeRad = radians(relativeAngle);
    float rawHeight =  getHeightFromLUT(currentAngle);
    height = rawHeight - heightOffset;
    
    /*
    if(height < minHeight){
        height = minHeight;
    }
    if(height > maxHeight){
        height = maxHeight;
    }     
*/
    return height;

}

