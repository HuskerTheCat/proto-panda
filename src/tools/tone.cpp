/*
   Buzzer tone helper. Trimmed from s3servo (servo motor control removed).
   Created by HA.S, October 10, 2022.
   Released into the public domain.
   mit라이선스라고하고싶은데 arduino-esp32 라이선스 따라가야겠지? 아직 라이선스 잘모르니 그냥 arduino-esp32 이거 사용"했으니 이거 따름
*/
#include "tools/tone.hpp"
#include "tools/logger.hpp"

#if defined(ESP_IDF_VERSION_MAJOR)
  #if ESP_IDF_VERSION_MAJOR >= 5
    #define USE_NEW_LEDC_API 1  // Core 3.x with IDF 5.x
  #else
    #define USE_NEW_LEDC_API 0  // Core 2.x with IDF 4.x
  #endif
#else
  // If ESP_IDF_VERSION_MAJOR is not defined, assume old API
  #define USE_NEW_LEDC_API 0
#endif

ToneESP32::ToneESP32(int pin, int channel) {	
  this->pin = pin;
  this->channel = channel;
  
  #if USE_NEW_LEDC_API
    // New API (Core 3.x): attach with default frequency
    // Don't set frequency yet - will be set in tone()
    #ifdef PWM_Res
      ledcAttach(pin, 5000, PWM_Res);
    #else
      ledcAttach(pin, 5000, 8);  // Default resolution 8-bit
    #endif
  #else
    // Old API (Core 2.x): just attach pin, frequency will be set in tone()
    ledcAttachPin(pin, channel);
  #endif
}

void ToneESP32::tone(int note) {
  #if USE_NEW_LEDC_API
    // New API (Core 3.x): detach and reattach with new frequency
    ledcDetach(pin);
    #ifdef PWM_Res
      ledcAttach(pin, note, PWM_Res);
    #else
      ledcAttach(pin, note, 8);
    #endif
    ledcWrite(pin, 127);
  #else
    // Old API (Core 2.x): use channel-based setup
    #ifdef PWM_Res
      ledcSetup(channel, note, PWM_Res);
    #else
      ledcSetup(channel, note, 8);
    #endif
    ledcWrite(channel, 127);
  #endif
}

void ToneESP32::noTone() {
  #if USE_NEW_LEDC_API
    ledcWrite(pin, 0);
  #else
    ledcWrite(channel, 0);
  #endif
}


