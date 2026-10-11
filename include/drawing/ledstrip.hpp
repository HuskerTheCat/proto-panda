#pragma once

/*
    Driver for non-addressable 4-wire RGB strips (+12V/5V, R, G, B).

    Each color channel is switched by a logic-level MOSFET driven by a PWM pin.
    Both cheeks share the same three pins, so they always show the same color.
*/

#include "config.hpp"

#include <stack>

enum LedMode{
    LED_MODE_OFF,
    LED_MODE_SOLID,     // Fixed color
    LED_MODE_PULSE,     // Color breathing in and out, speed = period in ms
    LED_MODE_RAINBOW,   // Hue cycling, speed = period in ms
    LED_MODE_BLINK,     // Color on/off, speed = period in ms
    LED_MODE_LAST = LED_MODE_BLINK,
};

struct LedState{
    LedMode mode = LED_MODE_OFF;
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint32_t speed = 2000;
};

class LedStrip {
    public:
        LedStrip():m_brightness(0),m_targetBrightness(0),m_turnOnRate(1),m_outR(0),m_outG(0),m_outB(0),m_enabled(false),m_managed(false),m_gentlyTurnOn(false),m_invert(false),m_mutex(xSemaphoreCreateMutex()){};

        bool BeginRgb(int pinR, int pinG, int pinB, bool invert);

        void Update();
        void Display();

        void setMode(int mode, int r, int g, int b, int speed);
        void setColor(int r, int g, int b);

        uint8_t getBrightness(){
            return m_brightness;
        }
        void setBrightness(uint8_t amount);
        void GentlySeBrightness(uint8_t bright, uint8_t rate = 1, uint8_t startAmount = 0);

        void SetManaged(bool set){
            m_managed = set;
        }
        bool IsManaged(){
            return m_managed;
        }

        int StackBehavior();
        int PopBehavior();

    private:
        static void hsvToRgb(uint8_t h, uint8_t &r, uint8_t &g, uint8_t &b);

        LedState m_state;
        std::stack<LedState> m_stateStack;

        int m_pins[3];
        uint8_t m_brightness, m_targetBrightness, m_turnOnRate;
        uint8_t m_outR, m_outG, m_outB;

        bool m_enabled, m_managed, m_gentlyTurnOn, m_invert;

        SemaphoreHandle_t m_mutex;
};
