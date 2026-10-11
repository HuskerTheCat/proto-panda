#include "drawing/ledstrip.hpp"
#include "tools/logger.hpp"

#define LED_PWM_FREQUENCY 5000
#define LED_PWM_RESOLUTION 8

bool LedStrip::BeginRgb(int pinR, int pinG, int pinB, bool invert){
    m_pins[0] = pinR;
    m_pins[1] = pinG;
    m_pins[2] = pinB;
    m_invert = invert;

    for (int i=0;i<3;i++){
        if (!ledcAttach(m_pins[i], LED_PWM_FREQUENCY, LED_PWM_RESOLUTION)){
            Logger::Error("Failed to attach PWM to led pin %d", m_pins[i]);
            return false;
        }
        ledcWrite(m_pins[i], m_invert ? 255 : 0);
    }

    m_enabled = true;
    Logger::Info("RGB leds started on pins R=%d G=%d B=%d", pinR, pinG, pinB);
    return true;
}

void LedStrip::setMode(int mode, int r, int g, int b, int speed){
    if (mode < LED_MODE_OFF || mode > LED_MODE_LAST){
        mode = LED_MODE_OFF;
    }
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    m_state.mode = (LedMode)mode;
    m_state.r = constrain(r, 0, 255);
    m_state.g = constrain(g, 0, 255);
    m_state.b = constrain(b, 0, 255);
    m_state.speed = speed > 0 ? speed : 2000;
    xSemaphoreGive(m_mutex);
}

void LedStrip::setColor(int r, int g, int b){
    setMode(LED_MODE_SOLID, r, g, b, m_state.speed);
}

void LedStrip::setBrightness(uint8_t amount){
    m_brightness = amount;
    m_gentlyTurnOn = false;
}

void LedStrip::GentlySeBrightness(uint8_t bright, uint8_t rate, uint8_t startAmount){
    m_brightness = startAmount;
    m_targetBrightness = bright;
    m_turnOnRate = rate == 0 ? 1 : rate;
    m_gentlyTurnOn = true;
}

int LedStrip::StackBehavior(){
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    m_stateStack.push(m_state);
    int size = m_stateStack.size();
    xSemaphoreGive(m_mutex);
    return size;
}

int LedStrip::PopBehavior(){
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    if (!m_stateStack.empty()){
        m_state = m_stateStack.top();
        m_stateStack.pop();
    }
    int size = m_stateStack.size();
    xSemaphoreGive(m_mutex);
    return size;
}

void LedStrip::hsvToRgb(uint8_t h, uint8_t &r, uint8_t &g, uint8_t &b){
    uint8_t region = h / 43;
    uint8_t remainder = (h - (region * 43)) * 6;
    uint8_t q = 255 - remainder;
    uint8_t t = remainder;
    switch (region){
        case 0:  r = 255; g = t;   b = 0;   break;
        case 1:  r = q;   g = 255; b = 0;   break;
        case 2:  r = 0;   g = 255; b = t;   break;
        case 3:  r = 0;   g = q;   b = 255; break;
        case 4:  r = t;   g = 0;   b = 255; break;
        default: r = 255; g = 0;   b = q;   break;
    }
}

void LedStrip::Update(){
    if (!m_enabled){
        return;
    }

    if (m_gentlyTurnOn){
        int next = m_brightness + m_turnOnRate;
        if (next >= m_targetBrightness){
            next = m_targetBrightness;
            m_gentlyTurnOn = false;
        }
        m_brightness = next;
    }

    xSemaphoreTake(m_mutex, portMAX_DELAY);
    LedState state = m_state;
    xSemaphoreGive(m_mutex);

    uint32_t phase = millis() % state.speed;
    uint8_t r = state.r, g = state.g, b = state.b;
    uint16_t level = 255;

    switch (state.mode){
        case LED_MODE_OFF:
            level = 0;
            break;
        case LED_MODE_SOLID:
            break;
        case LED_MODE_PULSE:{
            // Triangle wave, squared so the dim end lingers like breathing
            uint32_t half = state.speed / 2;
            uint32_t tri = phase < half ? (phase * 255) / half : ((state.speed - phase) * 255) / half;
            level = (tri * tri) / 255;
            break;
        }
        case LED_MODE_RAINBOW:
            hsvToRgb((phase * 255) / state.speed, r, g, b);
            break;
        case LED_MODE_BLINK:
            level = phase < state.speed / 2 ? 255 : 0;
            break;
    }

    level = (level * m_brightness) / 255;
    m_outR = (r * level) / 255;
    m_outG = (g * level) / 255;
    m_outB = (b * level) / 255;
}

void LedStrip::Display(){
    if (!m_enabled){
        return;
    }
    uint8_t out[3] = {m_outR, m_outG, m_outB};
    for (int i=0;i<3;i++){
        ledcWrite(m_pins[i], m_invert ? 255 - out[i] : out[i]);
    }
}
