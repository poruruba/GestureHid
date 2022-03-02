#include <M5StickC.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <BLEHIDDevice.h>
#include <HIDTypes.h>
#include <DFRobot_PAJ7620U2.h>
#include "asciimap.h"

#define LED_PORT  GPIO_NUM_10
const char* deviceName = "GestureHid";
const char* manufacturerName = "M5StickC";

enum MACRO_ASIGN{
  ASIGN_None = 0,
  ASIGN_Right,
  ASIGN_Left,
  ASIGN_Up,
  ASIGN_Down,
  ASIGN_Forward,
  ASIGN_Backward,
  ASIGN_Clockwise,
  ASIGN_Anti_Clockwise,
  ASIGN_Wave,
  ASIGN_BtnA,
  ASIGN_BtnB,
//  ASIGN_BtnC,
};

enum MACRO_TYPE {
  TYPE_TEXT,
  TYPE_KEYMAP,
  TYPE_KEYSET
};

typedef struct{
  MACRO_ASIGN asign;
  const char *title;
  MACRO_TYPE type;
//  union{
    KEYMAP keymap;
    struct{
      uint8_t num_key;
      uint8_t keys[6];
      uint8_t mod;
    } keyset;
    struct{
      const char *text;
    } text;
//  };
} KEYMACRO;
#define MACRO_END { ASIGN_None }

typedef struct{
  const char *title;
  const KEYMACRO *macros;
}MACROPANEL;

const KEYMACRO desktop_macros[] = {
  {
    ASIGN_Left,
    "デスクトップ切替(←)",
    TYPE_KEYMAP,
    .keymap = {
      0x50, KEY_MASK_WIN | KEY_MASK_CTRL
    },
  },
  {
    ASIGN_Right,
    "デスクトップ切替(→)",
    TYPE_KEYMAP,
    .keymap = {
      0x4f, KEY_MASK_WIN | KEY_MASK_CTRL
    },
  },
  {
    ASIGN_BtnA,
    "デスクトップ表示",
    TYPE_KEYMAP,
    .keymap = {
      asciimap_jp['d'].usage, KEY_MASK_WIN
    }
  },
  MACRO_END
};
const KEYMACRO powerpoint_macros[] = {
  {
    ASIGN_Left,
    "前ページ",
    TYPE_KEYMAP,
    .keymap = {
      0x4b, 0 
    },
  },
  {
    ASIGN_Right,
    "次ページ",
    TYPE_KEYMAP,
    .keymap = {
      0x4e, 0 
    },
  },
  {
    ASIGN_BtnA,
    "スライドシー開始",
    TYPE_KEYMAP,
    .keymap = {
      0x3e, 0 
    },
  },
  {
    ASIGN_BtnB,
    "スライドショー終了",
    TYPE_KEYMAP,
    .keymap = {
      0x29, 0
    },
  },
  MACRO_END
};
const KEYMACRO zoom_macros[] = {
  {
    ASIGN_Wave,
    "ミュート",
    TYPE_KEYMAP,
    .keymap = {
      asciimap_jp['a'].usage, KEY_MASK_ALT
    },
  },
  {
    ASIGN_BtnA,
    "手を挙げる",
    TYPE_KEYMAP,
    .keymap = {
      asciimap_jp['y'].usage, KEY_MASK_ALT
    },
  },
  MACRO_END
};
const KEYMACRO none_macros[] = {
  MACRO_END
};

#define NUM_OF_PANEL  4
const MACROPANEL macropanels[NUM_OF_PANEL] = {
  {
    "Desktop",
    desktop_macros
  },
  {
    "PowerPoint",
    powerpoint_macros
  },
  {
    "Zoom",
    zoom_macros
  },
  {
    "None",
    none_macros
  }
};

DFRobot_PAJ7620U2 paj;
BLEHIDDevice* hid;
BLECharacteristic* input;
BLECharacteristic* output;

unsigned char current_panel = 0;
bool ble_connected = false;

KEYMACRO findKeyMacro(uint8_t current_panel, enum MACRO_ASIGN asign);
void sendMacroKey(KEYMACRO macro);
void sendKeys(uint8_t mod, const uint8_t *keys, uint8_t num_key);
void sendKeyString(const char* ptr);

class MyCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer){
    ble_connected = true;
    digitalWrite(LED_PORT, LOW);
    BLE2902* desc = (BLE2902*)input->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
    desc->setNotifications(true);
  }

  void onDisconnect(BLEServer* pServer){
    ble_connected = false;
    digitalWrite(LED_PORT, HIGH);
    BLE2902* desc = (BLE2902*)input->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
    desc->setNotifications(false);
  }
};

void taskServer(void*){
  BLEDevice::init(deviceName);

  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyCallbacks());

  hid = new BLEHIDDevice(pServer);
  input = hid->inputReport(1); // <-- input REPORTID from report map
  output = hid->outputReport(1); // <-- output REPORTID from report map

  hid->manufacturer()->setValue(manufacturerName);

  hid->pnp(0x02, 0xe502, 0xa111, 0x0210);
  hid->hidInfo(0x00,0x02);

    const uint8_t report[] = {
      USAGE_PAGE(1),      0x01,       // Generic Desktop Ctrls
      USAGE(1),           0x06,       // Keyboard
      COLLECTION(1),      0x01,       // Application
      REPORT_ID(1),       0x01,        //   Report ID (1)
      USAGE_PAGE(1),      0x07,       //   Kbrd/Keypad
      USAGE_MINIMUM(1),   0xE0,
      USAGE_MAXIMUM(1),   0xE7,
      LOGICAL_MINIMUM(1), 0x00,
      LOGICAL_MAXIMUM(1), 0x01,
      REPORT_SIZE(1),     0x01,       //   1 byte (Modifier)
      REPORT_COUNT(1),    0x08,
      HIDINPUT(1),           0x02,       //   Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position
      REPORT_COUNT(1),    0x01,       //   1 byte (Reserved)
      REPORT_SIZE(1),     0x08,
      HIDINPUT(1),           0x01,       //   Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position
      REPORT_COUNT(1),    0x06,       //   6 bytes (Keys)
      REPORT_SIZE(1),     0x08,
      LOGICAL_MINIMUM(1), 0x00,
      LOGICAL_MAXIMUM(1), 0x65,       //   101 keys
      USAGE_MINIMUM(1),   0x00,
      USAGE_MAXIMUM(1),   0x65,
      HIDINPUT(1),           0x00,       //   Data,Array,Abs,No Wrap,Linear,Preferred State,No Null Position
      REPORT_COUNT(1),    0x05,       //   5 bits (Num lock, Caps lock, Scroll lock, Compose, Kana)
      REPORT_SIZE(1),     0x01,
      USAGE_PAGE(1),      0x08,       //   LEDs
      USAGE_MINIMUM(1),   0x01,       //   Num Lock
      USAGE_MAXIMUM(1),   0x05,       //   Kana
      HIDOUTPUT(1),          0x02,       //   Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile
      REPORT_COUNT(1),    0x01,       //   3 bits (Padding)
      REPORT_SIZE(1),     0x03,
      HIDOUTPUT(1),          0x01,       //   Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile
      END_COLLECTION(0)
    };

  hid->reportMap((uint8_t*)report, sizeof(report));
  hid->startServices();

  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->setAppearance(HID_KEYBOARD);
  pAdvertising->addServiceUUID(hid->hidService()->getUUID());
  pAdvertising->start();
  hid->setBatteryLevel(7);

  Serial.println("Advertising started!");
  delay(portMAX_DELAY);
};

void setup()
{
  M5.begin(true, true, true);
  Serial.println("setup start");

  M5.Axp.ScreenBreath(9);
  M5.Lcd.setRotation(0);
  M5.Lcd.setTextSize(2);

  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.print("Starting...");

  pinMode(LED_PORT, OUTPUT);
  digitalWrite(LED_PORT, HIGH);

  while(paj.begin() != 0){
    Serial.println("initial PAJ7620U2 failure!");
    delay(500);
  }
  Serial.println("PAJ7620U2 initialized");

  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.print(macropanels[current_panel].title);

  xTaskCreate(taskServer, "server", 20000, NULL, 5, NULL);

  Serial.println("setup end");
}

void loop()
{
  M5.update();

  if( M5.Axp.GetBtnPress() == 2 ){
    current_panel++;
    if( current_panel >= NUM_OF_PANEL )
      current_panel = 0;

    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.print(macropanels[current_panel].title);

    return;
  }

  if( ble_connected ){
    DFRobot_PAJ7620U2::eGesture_t gesture = paj.getGesture();
    if(gesture != paj.eGestureNone ){
    /* 
      * "None","Right","Left", "Up", "Down", "Forward", "Backward", "Clockwise", "Anti-Clockwise", "Wave",
      * "WaveSlowlyDisorder", "WaveSlowlyLeftRight", "WaveSlowlyUpDown", "WaveSlowlyForwardBackward"
    */
      String description  = paj.gestureDescription(gesture);
      Serial.println("Gesture = " + description);

      KEYMACRO macro = { ASIGN_None };
      if( gesture == paj.eGestureRight ){
        macro = findKeyMacro(current_panel, ASIGN_Right );
      }else
      if( gesture == paj.eGestureLeft ){
        macro = findKeyMacro(current_panel, ASIGN_Left );
      }else
      if( gesture == paj.eGestureUp ){
        macro = findKeyMacro(current_panel, ASIGN_Up );
      }else
      if( gesture == paj.eGestureDown ){
        macro = findKeyMacro(current_panel, ASIGN_Down );
      }else
      if( gesture == paj.eGestureForward ){
        macro = findKeyMacro(current_panel, ASIGN_Forward );
      }else
      if( gesture == paj.eGestureBackward ){
        macro = findKeyMacro(current_panel, ASIGN_Backward );
      }else
      if( gesture == paj.eGestureClockwise ){
        macro = findKeyMacro(current_panel, ASIGN_Clockwise );
      }else
      if( gesture == paj.eGestureAntiClockwise ){
        macro = findKeyMacro(current_panel, ASIGN_Anti_Clockwise );
      }else
      if( gesture == paj.eGestureWave ){
        macro = findKeyMacro(current_panel, ASIGN_Wave );
      }

      sendMacroKey(macro);
      delay(100);
    }

    if( M5.BtnA.wasPressed() ){
      Serial.println("M5.BtnA wasPressed");
      KEYMACRO macro = findKeyMacro(current_panel, ASIGN_BtnA );
      sendMacroKey(macro);
      delay(100);
    }
    if( M5.BtnB.wasPressed() ){
      Serial.println("M5.BtnB wasPressed");
      KEYMACRO macro = findKeyMacro(current_panel, ASIGN_BtnB );
      sendMacroKey(macro);
      delay(100);
    }
  }

  delay(1);
}

void sendMacroKey(KEYMACRO macro){
  if( macro.asign == ASIGN_None )
    return;

  if( macro.type == TYPE_TEXT ){
    sendKeyString(macro.text.text);
  }else
  if( macro.type == TYPE_KEYMAP ){
    sendKeys(macro.keymap.modifier, &macro.keymap.usage, 1);
  }else
  if( macro.type == TYPE_KEYSET ){
    sendKeys(macro.keyset.mod, macro.keyset.keys, macro.keyset.num_key);
  }
}

void sendKeys(uint8_t mod, const uint8_t *keys, uint8_t num_key){
  BLE2902* desc = (BLE2902*)input->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
  if( !desc->getNotifications() )
    return;

  uint8_t msg[] = {mod, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  for( int i = 0 ; i < num_key && i < 6 ; i++ ){
    msg[2 + i] = keys[i];
  }

  input->setValue(msg, sizeof(msg));
  input->notify();

  uint8_t msg1[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  input->setValue(msg1, sizeof(msg1));
  input->notify();

  delay(20);
}

void sendKeyString(const char* ptr){
  BLE2902* desc = (BLE2902*)input->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
  if( !desc->getNotifications() )
    return;

  // 1文字ずつHID(BLE)で送信
  while(*ptr){
    if( *ptr >= ASCIIMAP_SIZE_JP ){
      ptr++;
      continue;
    }
    KEYMAP map = asciimap_jp[(uint8_t)*ptr];
    uint8_t msg[] = {map.modifier, 0x00, map.usage, 0x00, 0x00, 0x00, 0x00, 0x00};
    input->setValue(msg, sizeof(msg));
    input->notify();
    ptr++;

    uint8_t msg1[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    input->setValue(msg1, sizeof(msg1));
    input->notify();

    delay(20);
  }
}

KEYMACRO findKeyMacro(uint8_t current_panel, enum MACRO_ASIGN asign){
  int i = 0;
  while(macropanels[current_panel].macros[i].asign != ASIGN_None ){
    if( macropanels[current_panel].macros[i].asign == asign )
      return macropanels[current_panel].macros[i];
    i++;
  }
  
  return { ASIGN_None };
}
