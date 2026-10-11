#include "tools/apds9960.hpp"
#include "tools/logger.hpp"

#include <Wire.h>

#define APDS9960_ENABLE     0x80
#define APDS9960_PILT       0x89
#define APDS9960_PIHT       0x8B
#define APDS9960_PERS       0x8C
#define APDS9960_PPULSE     0x8E
#define APDS9960_CONTROL    0x8F
#define APDS9960_CONFIG2    0x90
#define APDS9960_ID         0x92
#define APDS9960_PDATA      0x9C
#define APDS9960_PICLEAR    0xE5

#define APDS9960_ENABLE_PON  0x01
#define APDS9960_ENABLE_PEN  0x04
#define APDS9960_ENABLE_PIEN 0x20

// Reading has to drop this far below the threshold to count as released.
// Stops the boop from flickering when a hand hovers right at the edge.
#define APDS9960_HYSTERESIS 10

bool Apds9960::s_started = false;
int Apds9960::s_intPin = -1;
int Apds9960::s_threshold = 50;
bool Apds9960::s_triggered = false;

bool Apds9960::writeRegister(uint8_t reg, uint8_t value){
    Wire.beginTransmission(APDS9960_ADDR);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

bool Apds9960::readRegister(uint8_t reg, uint8_t &value){
    Wire.beginTransmission(APDS9960_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0){
        return false;
    }
    if (Wire.requestFrom((uint8_t)APDS9960_ADDR, (uint8_t)1) != 1){
        return false;
    }
    value = Wire.read();
    return true;
}

bool Apds9960::Begin(int threshold, int intPin){
    uint8_t id = 0;
    if (!readRegister(APDS9960_ID, id)){
        Logger::Error("APDS-9960 not found on I2C. Check SDA/SCL wiring (IO8/IO9)");
        return false;
    }
    // 0xAB is the genuine part, 0xA8 and 0x9C show up on common clones
    if (id != 0xAB && id != 0xA8 && id != 0x9C){
        Logger::Info("APDS-9960 reported unexpected id 0x%02X, trying anyway", id);
    }

    s_intPin = intPin;
    if (s_intPin >= 0){
        pinMode(s_intPin, INPUT_PULLUP);
    }

    bool ok = true;
    ok &= writeRegister(APDS9960_ENABLE, 0x00);         // Power down while configuring
    ok &= writeRegister(APDS9960_PPULSE, 0x87);         // 16us pulses, 8 pulses per reading
    ok &= writeRegister(APDS9960_CONTROL, 0x08);        // 100mA LED, 4x proximity gain
    ok &= writeRegister(APDS9960_CONFIG2, 0x01);        // No LED boost
    ok &= writeRegister(APDS9960_PERS, 0x20);           // INT after 2 readings in a row past the threshold
    ok &= writeRegister(APDS9960_PILT, 0);
    SetThreshold(threshold);

    uint8_t enable = APDS9960_ENABLE_PON | APDS9960_ENABLE_PEN;
    if (s_intPin >= 0){
        enable |= APDS9960_ENABLE_PIEN;
    }
    ok &= writeRegister(APDS9960_ENABLE, enable);
    ClearInterrupt();

    if (!ok){
        Logger::Error("Failed to configure APDS-9960");
        return false;
    }

    s_started = true;
    Logger::Info("APDS-9960 started, threshold=%d int pin=%d", s_threshold, s_intPin);
    return true;
}

int Apds9960::ReadProximity(){
    if (!s_started){
        return 0;
    }
    uint8_t value = 0;
    if (!readRegister(APDS9960_PDATA, value)){
        return 0;
    }
    return value;
}

void Apds9960::SetThreshold(int threshold){
    s_threshold = constrain(threshold, 1, 255);
    writeRegister(APDS9960_PIHT, s_threshold);
}

void Apds9960::ClearInterrupt(){
    Wire.beginTransmission(APDS9960_ADDR);
    Wire.write(APDS9960_PICLEAR);
    Wire.endTransmission();
}

bool Apds9960::IsTriggered(){
    if (!s_started){
        return false;
    }

    // With INT wired, skip the I2C read entirely while nothing is close
    if (s_intPin >= 0 && !s_triggered && digitalRead(s_intPin) == HIGH){
        return false;
    }

    int proximity = ReadProximity();
    if (s_triggered){
        if (proximity < s_threshold - APDS9960_HYSTERESIS){
            s_triggered = false;
            ClearInterrupt();
        }
    }else if (proximity >= s_threshold){
        s_triggered = true;
    }else if (s_intPin >= 0){
        // INT fired but the reading is already back under the threshold
        ClearInterrupt();
    }
    return s_triggered;
}
