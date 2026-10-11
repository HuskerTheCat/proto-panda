#include "config.hpp"

#include <SPI.h>
#include <string>


#include "tools/sensors.hpp"
#include "tools/devices.hpp"
#include "tools/oledscreen.hpp"
#include "tools/hardwareconfig.hpp"
#include "tools/storage.hpp"
#include "tools/logger.hpp"
#include "tools/fft.hpp"
#include "lua/luainterface.hpp"


#include "drawing/framerepository.hpp"
#include "drawing/animation.hpp"
#include "drawing/framebuffer.hpp"
#include "drawing/ledstrip.hpp"
#include "drawing/icons/icons.hpp"


#include "editmode/editmode.hpp"

#include "bluetooth/ble_client.hpp"

#include "drawing/modelanimation/keyframeplayer.hpp"


LedStrip g_leds;
FrameRepository g_frameRepo;
#ifdef ENABLE_BLE
BleManager g_remoteControls;
#endif
Animation g_animation;
#ifdef ENABLE_LUA
LuaInterface g_lua;
#endif
TaskHandle_t g_secondCore;
#ifdef ENABLE_EDIT_MODE
EditMode g_editMode;
#endif
ModelDict g_models;
ModelHandler g_modelHandler;
FFT g_fft;
KeyframePlayer g_kf;

FrameBuffer g_frameBuffer;

void second_loop(void*);

void setup() {
  /*
    Startup regulator pins and shoot it low asap.
  */
  #ifdef USE_ENABLE_PIN
    digitalWrite(PIN_ENABLE_REGULATOR, LOW);
    pinMode(PIN_ENABLE_REGULATOR, OUTPUT);
    digitalWrite(PIN_ENABLE_REGULATOR, LOW);
  #endif
  #ifdef ENABLE_EDIT_MODE
    #if EDIT_ENABLE_LOGIC_LEVEL == 1
      pinMode(EDIT_MODE_PIN, INPUT_PULLDOWN);
    #else
      pinMode(EDIT_MODE_PIN, INPUT_PULLUP);
    #endif
    #ifdef USE_BOOT_PIN_FOR_EDIT_MODE
        pinMode(0, INPUT_PULLUP); //Boot pin is always plugged in to a high impedance to reach here
    #endif
  #endif
  #ifdef USE_PIN_BATTERY_IN
    pinMode(PIN_USB_BATTERY_IN, INPUT);
  #endif

  Devices::Begin();
  Serial.begin(115200);
  Serial.printf("Starting proto panda v%s!\n", PANDA_VERSION);

  Logger::Allocate();

  Devices::BuzzerTone(2220);
  delay(200);
  Devices::BuzzerNoTone();
  Devices::I2CScan();
  Devices::BuzzerTone(1220);
  delay(200);
  Devices::BuzzerNoTone();
 
  OledScreen::Start();
  Sensors::Start();

  Devices::CalculateMemmoryUsage(); 
  
  #ifdef ENABLE_EDIT_MODE
  g_editMode.CheckBeginEditMode();

  if (g_editMode.IsOnEditMode()){
    Devices::BuzzerTone(220);
    delay(200);
    Devices::BuzzerTone(220);
    delay(200);
    Devices::BuzzerNoTone();
    return;
  }
  #endif

  Devices::CalculateMemmoryUsage(); 

  while (!Storage::Begin()){
    OledScreen::display.clearDisplay();
    OledScreen::display.drawBitmap(0,0, icon_sd, 128, 64, 1);
    OledScreen::display.display();
    delay(500);
    OledScreen::display.clearDisplay();
    OledScreen::display.display();
  }

  
  Devices::CalculateMemmoryUsage(); 
  HardwareConfig::LoadConfigs();

  g_animation.Allocate();
  g_modelHandler.Allocate();


  
  Devices::CalculateMemmoryUsageDifference("Storage");
  Logger::Begin();
  Devices::DisplayResetInfo();
  Devices::StartAvaliableDevices();
  
  Devices::CalculateMemmoryUsageDifference("Devices");
  if (!g_frameRepo.Begin()){
    OledScreen::CriticalFail("Frame repository has failed! If restarting does not solve, its a hardware problem.");
    for(;;){}
  }
  Devices::CalculateMemmoryUsageDifference("Frame repo");

  if (!g_frameBuffer.Allocate()){
    OledScreen::CriticalFail("Frame buffer has failed! Probally not enough ram!");
    for(;;){}
  }

  Devices::CalculateMemmoryUsageDifference("Frame buffer");

  #ifdef ENABLE_LUA
  if (!g_lua.Start()){
    OledScreen::CriticalFail("Failed to initialize Lua!");
    for(;;){}
  }
  Devices::CalculateMemmoryUsageDifference("Lua");
  

  if (!g_lua.LoadFile("/init.lua")){
    OledScreen::CriticalFail("Failed to load init.lua");
    Devices::BuzzerTone(300);
    delay(1500);
    Devices::BuzzerNoTone();
    for(;;){}
  }
  #endif

  Devices::CalculateMemmoryUsageDifference("init.lua");

  OledScreen::SetConsoleMode(false);
  OledScreen::display.setCursor(0,0);
  OledScreen::display.setTextSize(2);
  OledScreen::display.printf("Starting\nLua");
  OledScreen::display.display();
  OledScreen::display.setTextSize(1);
  #ifdef ENABLE_LUA
  Logger::Info("Starting Lua");
  g_lua.CallFunction("onSetup");
  #endif
  Devices::BuzzerTone(150);
  delay(100);

  Devices::BuzzerNoTone();
  Devices::CalculateMemmoryUsageDifference("onSetup");

  g_frameRepo.displayFFATInfo();
  Serial.printf("Running upon %d\n", xPortGetCoreID());

  #ifndef SINGLE_CORE_RUN
  xTaskCreatePinnedToCore(second_loop, "second loop", 10000, NULL, ( 2 | portPRIVILEGE_BIT ), &g_secondCore, 0);
  Devices::CalculateMemmoryUsageDifference("second loop");
  #endif
   
  Devices::BuzzerTone(880);
  delay(100);
  #ifdef ENABLE_LUA
  g_lua.CallFunction("onPreflight");
  Devices::BuzzerNoTone();
  #endif
  
  
  Devices::CalculateMemmoryUsageDifference("completed setup");
  digitalWrite(PIN_ENABLE_REGULATOR, HIGH);
}

void second_loop(void*){
  #ifndef SINGLE_CORE_RUN
  for( ;; )
  { 
    Devices::BeginAutoFrame();
    
    if (g_fft.isManaged()){
      g_fft.update();
    }
    g_animation.Update(Devices::getAutoDeltaTime());
    vTaskDelay(1);

    g_leds.Update();
    if (g_leds.IsManaged()){
      g_leds.Display();
    }
    vTaskDelay(1);
    
    Devices::EndAutoFrame();
  }
  #endif
}



void loop() {
  Devices::BeginFrame();
  #ifdef ENABLE_EDIT_MODE
  if (g_editMode.IsOnEditMode()){
    g_editMode.LoopEditMode();
    return;
  }
  #endif

  if (Devices::AutoCheckPowerLevel() && !Devices::CheckPowerLevel()){
    Devices::WaitForPower();
    return;
  }
  
  
  Devices::ReadSensors();
  #ifdef ENABLE_BLE
  g_remoteControls.update();
  #endif
  #ifdef ENABLE_BLE
  g_remoteControls.sendUpdatesToLua();
  #endif
  #ifdef ENABLE_LUA
  g_lua.CallFunctionT("onLoop", Devices::getDeltaTime());
  #endif
  #ifdef SINGLE_CORE_RUN
  if (g_fft.isManaged()){
    g_fft.update();
  }
  g_animation.Update(g_frameRepo.takeFile());
  g_frameRepo.freeFile();
  g_leds.Update();
  if (g_leds.IsManaged()){
    g_leds.Display();
  }
  #endif
  Devices::EndFrame();  
}
