#pragma once

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <ArduinoJson.h>

class BaseDisplay;
class HardwareConfig{
    public:
        static bool LoadConfigs();

        static inline int CanvasWidth(){
            return HardwareCanvasWidth;
        }

        static inline int CanvasHeight(){
            return HardwareCanvasHeight;
        }
        
    private:
        static bool StartDmaDisplay(HUB75_I2S_CFG &panelConfig);
        static void loadAndParseDisplay(JsonObject doc);
        static void loadHub75AndStart(JsonObject doc, bool compatibilityMode);
        static int checkInvalidPin(int pin);
        static void loadViews(JsonObject config, BaseDisplay* display, uint16_t defaultWidth, uint16_t defaultHeight);

        static int HardwareCanvasWidth;
        static int HardwareCanvasHeight;

};


