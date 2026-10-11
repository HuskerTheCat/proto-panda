#include "tools/hardwareconfig.hpp"
#include "tools/oledscreen.hpp"
#include "tools/logger.hpp"
#include "tools/devices.hpp"
#include "tools/psrammap.hpp"
#include "tools/logger.hpp"
#include "lua/luainterface.hpp"

#include "tools/displays/hub75.hpp"

#if PANDA_SD_MODE == 1
#include <SD.h>
#elif PANDA_SD_MODE == 2
#include <SD_MMC.h>
#else
#error "NO SD_MODE Mode defined (set PANDA_SD_MODE to 1 for SD or 2 for SD_MMC)"
#endif


int HardwareConfig::HardwareCanvasWidth = DEFAULT_CANVAS_WIDTH;
int HardwareConfig::HardwareCanvasHeight = DEFAULT_CANVAS_HEIGHT;

const uint8_t invalidPins[] = {
    I2C_SDA,
    I2C_SLC,
    SPI_CS,
    SPI_MOSI,
    SPI_MISO,
    SPI_SCK,
    EDIT_MODE_PIN,
    #ifdef PIN_ENABLE_REGULATOR
        PIN_ENABLE_REGULATOR,
    #endif
    #ifdef USE_BUZZER
        BUZZER_PIN,
    #endif
    #ifdef USE_PIN_BATTERY_IN
        PIN_USB_BATTERY_IN,
    #endif
};


int HardwareConfig::checkInvalidPin(int pin){
    for (int i=0;i<sizeof(invalidPins);i++){
        if (invalidPins[i] == pin){
            char errMsg[100];
            sprintf(errMsg, "Allocated pin %d but its restricted", pin);
            OledScreen::CriticalFail(errMsg);
        }
    }
    return pin;
}

void HardwareConfig::loadAndParseDisplay(JsonObject displayInfo){
    if (!displayInfo.containsKey("type")){
        OledScreen::CriticalFail("Display dont have a type");
        return;
    }

    if (!displayInfo["enabled"]) {
        Devices::Display = new EmptyDisplay();
        return;
    }

    std::string displayType = displayInfo["type"].as<const char*>();
    if (displayType == "hub75"){
        loadHub75AndStart(displayInfo["hub75"], false);
    }else{
        OledScreen::CriticalFail("Display type in hardware.json is not supported");
    }
    if (!Devices::Display){
        Devices::Display = new EmptyDisplay();
    }

    if (displayInfo.containsKey("mirrorOtherHalf")) Devices::Display->mirrorHalf = checkInvalidPin(displayInfo["mirrorOtherHalf"]);
}



void HardwareConfig::loadViews(JsonObject config, BaseDisplay* display, uint16_t defaultWidth, uint16_t defaultHeight) {
    if (!config.containsKey("views") || !config["views"].is<JsonArray>()) {
        return;
    }

    JsonArray views = config["views"].as<JsonArray>();

    for (JsonObject v : views) {
        if (!v.containsKey("x") || !v.containsKey("y")) {
            OledScreen::CriticalFail("View missing required 'x' or 'y' field");
            return;
        }
        if (!v.containsKey("canvas_x") || !v.containsKey("canvas_y")) {
            OledScreen::CriticalFail("View missing required 'canvas_x' or 'canvas_y' field");
            return;
        }

        uint16_t x        = v["x"];
        uint16_t y        = v["y"];
        uint16_t canvas_x = v["canvas_x"];
        uint16_t canvas_y = v["canvas_y"];
        uint16_t w        = v.containsKey("width")  ? (uint16_t)v["width"]  : defaultWidth;
        uint16_t h        = v.containsKey("height") ? (uint16_t)v["height"] : defaultHeight;
        bool flipH        = v.containsKey("flip_horizontal") ? (bool)v["flip_horizontal"] : false;
        bool flipV        = v.containsKey("flip_vertical") ? (bool)v["flip_vertical"] : false;
        Logger::Info("Added view: %d %d  ->  %d %d   (%d,%d)", x, y, canvas_x, canvas_y, w, h);
        display->view.addView(x, y, canvas_x, canvas_y, w, h, flipH, flipV);
    }
}



void HardwareConfig::loadHub75AndStart(JsonObject hub75, bool compatibilityMode){
    JsonObject pins = hub75;
    if (!hub75.containsKey("pins")){
        if (!hub75.containsKey("dma_r1")){
            OledScreen::CriticalFail("Missing 'pins' in hub75 display");
        }
    }
    HUB75_I2S_CFG panelConfig;

    panelConfig.double_buff = true; // Turn of double buffer
    panelConfig.clkphase = false;
    panelConfig.gpio.r1 = DMA_GPIO_R1;
    panelConfig.gpio.g1 = DMA_GPIO_G1;
    panelConfig.gpio.b1 = DMA_GPIO_B1;
    panelConfig.gpio.r2 = DMA_GPIO_R2;
    panelConfig.gpio.g2 = DMA_GPIO_G2;
    panelConfig.gpio.b2 = DMA_GPIO_B2;
    panelConfig.gpio.a = DMA_GPIO_A;
    panelConfig.gpio.b = DMA_GPIO_B;
    panelConfig.gpio.c = DMA_GPIO_C;
    panelConfig.gpio.d = DMA_GPIO_D;
    panelConfig.gpio.lat = DMA_GPIO_LAT;
    panelConfig.gpio.oe = DMA_GPIO_OE;
    panelConfig.gpio.clk = DMA_GPIO_CLK;
    panelConfig.setPixelColorDepthBits(12);
    panelConfig.i2sspeed = HUB75_I2S_CFG::HZ_20M;
    HardwareCanvasWidth = 64;
    HardwareCanvasHeight = 32;

    pins = hub75["pins"];

    if (pins.containsKey("dma_r1")) panelConfig.gpio.r1 = checkInvalidPin(pins["dma_r1"]);
    if (pins.containsKey("dma_g1")) panelConfig.gpio.g1 = checkInvalidPin(pins["dma_g1"]);
    if (pins.containsKey("dma_b1")) panelConfig.gpio.b1 = checkInvalidPin(pins["dma_b1"]);
    if (pins.containsKey("dma_r2")) panelConfig.gpio.r2 = checkInvalidPin(pins["dma_r2"]);
    if (pins.containsKey("dma_g2")) panelConfig.gpio.g2 = checkInvalidPin(pins["dma_g2"]);
    if (pins.containsKey("dma_b2")) panelConfig.gpio.b2 = checkInvalidPin(pins["dma_b2"]);
    if (pins.containsKey("dma_a")) panelConfig.gpio.a = checkInvalidPin(pins["dma_a"]);
    if (pins.containsKey("dma_b")) panelConfig.gpio.b = checkInvalidPin(pins["dma_b"]);
    if (pins.containsKey("dma_c")) panelConfig.gpio.c = checkInvalidPin(pins["dma_c"]);
    if (pins.containsKey("dma_d")) panelConfig.gpio.d = checkInvalidPin(pins["dma_d"]);
    if (pins.containsKey("dma_lat")) panelConfig.gpio.lat = checkInvalidPin(pins["dma_lat"]);
    if (pins.containsKey("dma_oe")) panelConfig.gpio.oe = checkInvalidPin(pins["dma_oe"]);
    if (pins.containsKey("dma_clk")) panelConfig.gpio.clk = checkInvalidPin(pins["dma_clk"]);


    if (hub75.containsKey("width")){
        panelConfig.mx_width = hub75["width"];
    }else{
        if (!compatibilityMode){
            OledScreen::CriticalFail("Missing 'width' in display");
        }
        panelConfig.mx_height = HardwareConfig::HardwareCanvasWidth;
    }
    
    if (hub75.containsKey("height")) {
        panelConfig.mx_height = hub75["height"];
    }else{
        if (!compatibilityMode){
            OledScreen::CriticalFail("Missing 'height' in display");
        }
        panelConfig.mx_height = HardwareConfig::HardwareCanvasHeight;
    }

    if (hub75.containsKey("panels")) {
        panelConfig.chain_length = hub75["panels"];
    }else{
        if (!compatibilityMode){
            OledScreen::CriticalFail("Missing 'panels' in display");
        }
        panelConfig.chain_length = 2;
    }

    if (hub75.containsKey("colordepth")) {
        panelConfig.setPixelColorDepthBits(hub75["colordepth"]);
    }

    StartDmaDisplay(panelConfig);

    if (hub75.containsKey("views") && hub75["views"].is<JsonArray>()) {
        Logger::Info("Loading views!");
        loadViews(hub75, Devices::Display, panelConfig.mx_width, panelConfig.mx_height);
    }
    return;
}

bool HardwareConfig::LoadConfigs(){
    
    
    File conf = PANDA_SD.open( "/hardware.json" );
    if( !conf ) {
        OledScreen::CriticalFail("Can't open hardware.jsonn");
        for(;;){}
        return false;
    }

    SpiRamAllocator allocator;
    JsonDocument  hardwareConfigJson(&allocator);
    auto err = deserializeJson( hardwareConfigJson, conf );
    conf.close();
    if( err ) {
       char miniHBuffer[1000];
       sprintf(miniHBuffer, "hardware.json.json:\n%s", err.c_str());
       OledScreen::CriticalFail(miniHBuffer);
       for(;;){}
       return false;
    }

    if (hardwareConfigJson.containsKey("canvas_width")) {
        HardwareConfig::HardwareCanvasWidth = hardwareConfigJson["canvas_width"].as<int>();
    }
    if (hardwareConfigJson.containsKey("canvas_height")) {
        HardwareConfig::HardwareCanvasHeight = hardwareConfigJson["canvas_height"].as<int>();
    }

    Logger::Info("Canvas size will be %dx%d", HardwareConfig::HardwareCanvasWidth, HardwareConfig::HardwareCanvasHeight);

    if (hardwareConfigJson.containsKey("display")) {
        Logger::Info("Loading display info");
        loadAndParseDisplay(hardwareConfigJson["display"]);
    }else if (hardwareConfigJson.containsKey("hub75")) { //Backward compatibility
        loadHub75AndStart(hardwareConfigJson["hub75"], true);
        Devices::Display->mirrorHalf = true;
    }else{
        Devices::Display = new EmptyDisplay();
        Logger::Info("NO DISPLAY DEFINED!");
    }


    hardwareConfigJson.clear();
    return true;
}

bool HardwareConfig::StartDmaDisplay(HUB75_I2S_CFG &panelConfig){
    if (Devices::Display != nullptr){
        Logger::Info("DMA display is already started.");
        return false;
    }
    //Todo check avaliable ram
    Devices::Display = new Hub75Display(panelConfig);
    if (Devices::Display == nullptr){
        Logger::Info("Failed to start DMA display");
        return false;
    }

    Devices::Display->begin();
    Devices::Display->setBrightness8(1);
    Devices::Display->clearScreen();
    Devices::Display->flipDma();
    Devices::Display->clearScreen();
    Devices::Display->flipDma();
    Logger::Info("DMA display initialized!");
    Devices::CalculateMemmoryUsageDifference("Dma display");

    return true;
}
