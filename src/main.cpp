#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Wire.h>
#include "encoder.h"
#include "batt.h"
#include "sleep.h"
#include "display.h"
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeMonoBoldOblique9pt7b.h>
#include <EEPROM.h>
#include <stdlib.h>

// IO settings //
#define GreenLed 16 //powerLed
#define TFT_CS  7
#define TFT_DC  10
#define TFT_SDA 4
#define TFT_SCL 6
#define TFT_RST 9
#define TFT_WIDTH 160
#define TFT_HEIGHT 80
#define TFT_POWER_PIN 13
const int ButtonPin = 12;

// AS5600 register //
#define AS5600_AS5601_DEV_ADDRESS 0x36
#define AS5600_AS5601_REG_RAW_ANGLE 0x0C
#define AS5600_ZMC0 0x00
#define AS5600_ZPOS 0x01
#define AS5600_MPOS 0x03
#define AS5600_MANG 0x05
uint16_t zeroPosition = 0x0000;
uint16_t maxAngle = 0x0400; //maxAngle 90deg
#define AS5600_AS5601_DEV_ADDRESS 0x36
#define AS5600_AS5601_REG_RAW_ANGLE 0x0C

// height variable //

// power LED variable //
bool GreenLedState = false;

// prototype //
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

enum CalibrationState {
    CAL_IDLE,
    CAL_WAIT_RECHECK
};
CalibrationState calState = CAL_IDLE;


float readEncoderAngle();
void setZeroPosition(uint16_t zeroPosition);
void setMaxAngle(uint16_t maxAngle);


// carib //
float calibJigLow = 5.0f;
float calibJigHeigh = 30.3f;
float newScale = 1.0f;

void setup() {

    // I2C settings
    Wire.begin();
    Wire.setClock(400000);
    restoreZeroPositionFromEEPROM();
    EEPROM.get(2,initialAngle);
    EEPROM.get(300,heightOffset);

    pinMode(ButtonPin,INPUT_PULLUP);

    delay(100);

    // calibration mode config

    bool enterCalibration = false;
    uint32_t pressStart = millis();

    if(digitalRead(ButtonPin)==LOW){
        while(millis() - pressStart < 3000){
            if(digitalRead(ButtonPin)==HIGH){
                break;
            }
            delay(10);
        }
    if(digitalRead(ButtonPin)==LOW && millis() - pressStart >=3000){
        enterCalibration = true;
       }
    }

    if(enterCalibration){ 
    pinMode(TFT_POWER_PIN,OUTPUT);
    digitalWrite(TFT_POWER_PIN,HIGH);    
    tft.initR(INITR_GREENTAB); //for greentab setting
    tft.invertDisplay(true);
    tft.fillScreen(ST7735_BLACK);
    tft.setRotation(1);
    tft.setTextSize(1);
      
    calibrationMode();       
            
    }

    // EEPROM settings
    isReferenceSet = true;

    // AS5600 MaxAngle settings
    setMaxAngle(maxAngle);


    // I2C settings
    Wire.begin();
    Wire.setClock(400000);

    pinMode(TFT_POWER_PIN,OUTPUT);
    digitalWrite(TFT_POWER_PIN,HIGH);

    
    tft.initR(INITR_GREENTAB); //for greentab setting
    tft.invertDisplay(true);
    tft.fillScreen(ST7735_BLACK);
    tft.setRotation(1);
    tft.setTextSize(1);
  
    // startup message
    tft.setTextColor(0xf7be);
    tft.setFont(&FreeMonoBoldOblique9pt7b);
    tft.setCursor(40,70);
    tft.println("KO");
    tft.setCursor(65,70);
    tft.println("PROPO");

    delay(3000);
    tft.fillRect(33,60,95,30,ST7735_BLACK);    


}

void(*resetFunc)(void) = 0;

void showMassage(const char* line1, const char* line2){
    tft.fillScreen(ST7735_BLACK);
    tft.setTextColor(ST7735_WHITE);
    tft.setFont(&FreeMonoBoldOblique9pt7b);
    tft.setCursor(50,30);
    tft.println(line1);

    tft.setFont(&FreeMonoBoldOblique9pt7b);
    tft.setCursor(50,50);
    tft.println(line2);    
}

void calibrationMode(){

    const int NUM_POINTS = 5;
    float knownHeights[NUM_POINTS] = {2.5f,5.0f,16.0f,27.0f,38.0f};
    float measuredAngles[NUM_POINTS];
    float minAngle[NUM_POINTS];
    float maxAngle[NUM_POINTS]; 
    float rangeAngle[NUM_POINTS];

    // stabilize the encoder
    const uint8_t STABILIZE_TIME = 10; //seconds
    int sec;
    for(sec= STABILIZE_TIME; sec >= 0; sec--){
       uint32_t start = millis();

       tft.setTextSize(1);
       tft.fillScreen(ST7735_BLACK);
       tft.setCursor(40,40);
       tft.setTextColor(ST7735_WHITE);
       tft.println("CalibrationMode");
       tft.setCursor(40,60);
       tft.println("wait ");
       tft.setCursor(40,80);
       tft.print(sec);
       tft.println(" sec");
       
       while (millis() - start < 1000){
        readEncoderAngle();
        delay(5);
       }    
    }

    tft.fillScreen(ST7735_BLACK);
    tft.setTextColor(ST7735_WHITE);
    tft.setCursor(40,40);
    tft.println("CalibrationMode");
    tft.setCursor(40,60);
    tft.println("Press BTN");

    while (digitalRead(ButtonPin) == LOW);
    while (digitalRead(ButtonPin) == HIGH);
   
    for (int i = 0; i < NUM_POINTS; i++) {
        tft.fillScreen(ST7735_BLACK);
        tft.setCursor(40,40);
        tft.print("set ");
        tft.print(knownHeights[i],1);
        tft.println("mm");
        tft.setCursor(40,60);
        tft.println("Press BTN");
        tft.setCursor(50,70);
        tft.println("to");
        tft.setCursor(40,80);
        tft.print("measure");

    while(digitalRead(ButtonPin)==LOW);
    while(digitalRead(ButtonPin)==HIGH);

    const int samples = 20000;
    uint32_t sumRaw = 0;
    uint32_t selectedCount = 0;
    uint16_t minRaw = 0xFFFF;
    uint16_t maxRaw = 0x0000;

    // 1st pass: collect min/max and sum (for fallback average)
    // show simple feedback to user while sampling
    tft.fillScreen(ST7735_BLACK);
    tft.setCursor(40,40);
    tft.println("Searching...");

    for (int j = 0; j < samples; j++) {
        uint16_t raw = readEncoderAngle();
        sumRaw += raw;
        if (raw < minRaw) minRaw = raw;
        if (raw > maxRaw) maxRaw = raw;
        delayMicroseconds(50);
    }

    // compute min/max angles for display
    minAngle[i] = (float)minRaw * 360.0f / 4096.0f;
    maxAngle[i] = (float)maxRaw * 360.0f / 4096.0f;
    rangeAngle[i] = maxAngle[i] - minAngle[i];

    // If the raw range is small, build a histogram for that range and find the mode.
    // This uses much less RAM when the spread is small (common in real device: ~1 raw step).
    uint16_t range = (uint16_t)(maxRaw - minRaw) + 1;

    if (range > 0 && range <= 3000) { // safety cap to avoid large allocations
        // allocate histogram (counts need to hold up to `samples` -> use uint16_t)
        uint16_t *hist = (uint16_t *)calloc(range, sizeof(uint16_t));
        if (hist != NULL) {
            // 2nd pass: fill histogram
            // show simple feedback to user while sampling
            tft.fillScreen(ST7735_BLACK);
            tft.setCursor(40,40);
            tft.println("Searching...");

            for (int j = 0; j < samples; j++) {
                uint16_t raw = readEncoderAngle();
                hist[raw - minRaw]++;
                delayMicroseconds(50);
            }

            // find mode index
            uint16_t modeIdx = 0;
            uint16_t maxCount = 0;
            for (uint16_t k = 0; k < range; k++) {
                if (hist[k] > maxCount) {
                    maxCount = hist[k];
                    modeIdx = k;
                }
            }

            uint16_t modeRaw = (uint16_t)(minRaw + modeIdx);
            measuredAngles[i] = (float)modeRaw * 360.0f / 4096.0f;
            selectedCount = maxCount; // record how many times the selected raw value occurred

            free(hist);
        } else {
            // allocation failed -> fallback to average
            float avgAngle = ((float)sumRaw / samples) * 360.0f / 4096.0f;
            measuredAngles[i] = avgAngle;
        }
    } else {
        // range too big (unexpected) -> fallback to average
        float avgAngle = ((float)sumRaw / samples) * 360.0f / 4096.0f;
        measuredAngles[i] = avgAngle;
    }


    tft.fillScreen(ST7735_BLACK);
    tft.setCursor(30,30);
    tft.print("Height: ");
    tft.print(knownHeights[i],3);
    tft.println(" mm");
    
    tft.setCursor(30,50);
    tft.print("Angle: ");
    tft.print(measuredAngles[i],5);
    tft.println(" deg");
    
    tft.setCursor(30,70);
    tft.print("Count: ");
    tft.print(selectedCount);
    tft.print("/");
    tft.println(samples);


    while (digitalRead(ButtonPin) == HIGH);
    while (digitalRead(ButtonPin) == LOW);
    }

    
for(int i=0; i<NUM_POINTS; i++){
    EEPROM.put(100 + i * sizeof(float), measuredAngles[i]);
    EEPROM.put(200 + i * sizeof(float), knownHeights[i]);
}

    tft.fillScreen(ST7735_BLACK);
    tft.setCursor(40,40);
    tft.println("complete");

    delay(5000);

    resetFunc();    
    
}

void loop() {

    static unsigned long buttonPuressStart = 0;
    static bool buttonPressed = false;
    static bool longPressHandled = false;

    if(digitalRead(ButtonPin) == LOW){
     if(!buttonPressed){
        buttonPressed = true;
        longPressHandled = false;
        buttonPuressStart = millis();
     }
   

    if(!longPressHandled && (millis() - buttonPuressStart >= 3000)){
            float currentAngle = readEncoderAngle();
            currentAngle = currentAngle * 360.0f / 4096.0f;
            float measuredHeight = interpolateHeight(currentAngle);

            const float referenceHeight = 5.0f;

            heightOffset = measuredHeight - referenceHeight;
            EEPROM.put(300,heightOffset);

            setInitialAngleFromSensor();

            showMassage("USER CALIBRATION","SET 5mm JIG");
            delay(2000);

            showMassage("SET 5mm JIG","Again");

            calState = CAL_WAIT_RECHECK;
            longPressHandled = true; 
    }  
    }else{
        if(buttonPressed){
            if(calState == CAL_WAIT_RECHECK){
                float currentAngle = readEncoderAngle();
                currentAngle = currentAngle * 360.0f / 4096.0f;
                float measuredHeight = interpolateHeight(currentAngle);

                if(measuredHeight >= 4.95f && measuredHeight <= 5.04f){
                    showMassage("CALIBRATION","SUCCESS");
                    delay(2000);
                    calState = CAL_IDLE;
                }else{
                    showMassage("SET 5mm JIG","");
                    calState = CAL_WAIT_RECHECK;
                    
                }

            }
        }
            buttonPressed = false;
            // longPressHandled = false;
    }    
    


    // battery survey
    updateBatteryStatus(tft);

    //height calucurate        
    float height = updateHeight(); 
    updateHeightDisplay(tft,height,previousHeight);

    // sleep control
    updateSleepStatus(height, TFT_POWER_PIN);
    handleSleepLED(GreenLed);        

    delay(50);
}


