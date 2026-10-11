#pragma once

#include <Arduino.h>

/*
    Minimal APDS-9960 proximity driver, used as the boop sensor.

    Wiring: VCC to 3.3V, SDA/SCL on the shared I2C bus (IO8/IO9),
    INT to any free GPIO (IO48 by default). INT is open drain and goes
    LOW while something is closer than the threshold.
*/

#define APDS9960_ADDR 0x39

class Apds9960{
    public:
        static bool Begin(int threshold, int intPin);
        static bool IsStarted(){
            return s_started;
        }
        static int ReadProximity();
        static void SetThreshold(int threshold);
        static int GetThreshold(){
            return s_threshold;
        }
        static bool IsTriggered();
        static void ClearInterrupt();

    private:
        static bool writeRegister(uint8_t reg, uint8_t value);
        static bool readRegister(uint8_t reg, uint8_t &value);

        static bool s_started;
        static int s_intPin;
        static int s_threshold;
        static bool s_triggered;
};
