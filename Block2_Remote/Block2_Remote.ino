// ============================================
// BLOCK 2: REMOTE CONTROL WITH MENU
// ESP32-S3-N16R8 + TFT ST7735 + TTL RS485
// Modular Architecture with On-Screen Menu Button
// ============================================

#define BLOCK_2_REMOTE
#include "../config.h"

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <EEPROM.h>

// ============================================
// ===== GLOBAL OBJECTS =====
// ============================================

Adafruit_ST7735 tft = Adafruit_ST7735(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_MOSI, PIN_TFT_SCLK, PIN_TFT_RST);
HardwareSerial rs485(2);  // Serial2 для RS485

char packetBuffer[256];
int packetIndex = 0;

// ============================================
// ===== MODULE STATES =====
// ============================================

struct ModuleStates {
  bool servo_control = MODULE_SERVO_CONTROL;
  bool compass = MODULE_COMPASS;
  bool frequency = MODULE_FREQUENCY;
  bool telemetry = MODULE_TELEMETRY;
};

ModuleStates modules;

// ============================================
// ===== GLOBAL VARIABLES =====
// ============================================

// Servo
int servoX = 0, servoY = 0;
int targetX = 0, targetY = 0;
float smoothX = 0, smoothY = 0;
const float SMOOTHING_FACTOR = 0.3f;
unsigned long lastStepTimeX = 0, lastStepTimeY = 0;

// Compass
int compassAngle = 0;
bool compassOk = false;
bool calibrated = false;

// Joystick
int joystickCenterX = 2048, joystickCenterY = 2048;
bool joystickCalibrated = false;

// Channels
int currentChannel = 0;

// Mode
int currentMode = MODE_BOTH;

// UI State
bool moduleMenuActive = false;
int moduleSelection = 0;
unsigned long lastMenuActivity = 0;

// Link status
unsigned long lastResponseTime = 0;
bool linkLost = false;

// Screen update
bool needUpdate = true;
int lastServoX = -999, lastServoY = -999, lastCompass = -999;
bool lastCompassOk = false;
int lastChannel = -1, lastMode = -1;
bool lastLinkStatus = false;

// ============================================
// ===== UI: BUTTONS ON SCREEN =====
// ============================================

struct ScreenButton {
  int x, y, w, h;
  const char* label;
  
  void draw(Adafruit_ST7735& display, bool highlight = false) {
    uint16_t borderColor = highlight ? ST7735_WHITE : ST7735_YELLOW;
    uint16_t bgColor = highlight ? ST7735_YELLOW : ST7735_BLACK;
    uint16_t textColor = highlight ? ST7735_BLACK : ST7735_WHITE;
    
    display.fillRect(x, y, w, h, bgColor);
    display.drawRect(x, y, w, h, borderColor);
    display.setTextColor(textColor);
    display.setTextSize(1);
    
    int textX = x + 3;
    int textY = y + 3;
    display.setCursor(textX, textY);
    display.print(label);
  }
};

ScreenButton btnMenu = {140, 0, 28, 14, "MNU"};

// ============================================
// ===== MODULE: JOYSTICK CALIBRATION =====
// ============================================

void calibrateJoystick() {
  Serial.println("[CALIB] Joystick calibration...");
  
  tft.fillScreen(ST7735_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ST7735_YELLOW);
  tft.setCursor(10, 30);
  tft.print("CALIBRATE");
  tft.setTextSize(1);
  tft.setCursor(10, 60);
  tft.print("Don't touch!");
  
  long sumX = 0, sumY = 0;
  int samples = 100;
  
  for (int i = 0; i < samples; i++) {
    sumX += analogRead(PIN_VRX);
    sumY += analogRead(PIN_VRY);
    delay(5);
  }
  
  joystickCenterX = sumX / samples;
  joystickCenterY = sumY / samples;
  joystickCalibrated = true;
  
  Serial.printf("[CALIB] Center: X=%d, Y=%d\n", joystickCenterX, joystickCenterY);
  
  tft.fillScreen(ST7735_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(ST7735_GREEN);
  tft.setCursor(10, 30);
  tft.print("CALIB OK!");
  delay(1000);
}

// ============================================
// ===== RS485 COMMUNICATION =====
// ============================================

void sendPacket(const char* data) {
  digitalWrite(RS485_DE_PIN, HIGH);
  delay(1);
  
  rs485.write('$');
  rs485.print(data);
  rs485.write('*');
  
  rs485.flush();
  delayMicroseconds(1000);
  
  digitalWrite(RS485_DE_PIN, LOW);
  
  if (DEBUG_MODE) Serial.printf("[TX] $%s*\n", data);
}

bool receivePacket(char* output) {
  while (rs485.available()) {
    char c = rs485.read();
    
    if (c == '$') {
      packetIndex = 0;
      return false;
    }
    
    if (c == '*') {
      packetBuffer[packetIndex] = '\0';
      strcpy(output, packetBuffer);
      packetIndex = 0;
      return true;
    }
    
    if (packetIndex < 255) {
      packetBuffer[packetIndex++] = c;
    }
  }
  return false;
}

// ============================================
// ===== MODULE: JOYSTICK READING =====
// ============================================

void readJoystickServo() {
  if (!joystickCalibrated) return;
  
  int rawX = analogRead(PIN_VRX);
  int rawY = analogRead(PIN_VRY);
  
  int diffX = rawX - joystickCenterX;
  int diffY = rawY - joystickCenterY;
  
  if (abs(diffX) < JOYSTICK_DEADZONE) diffX = 0;
  if (abs(diffY) < JOYSTICK_DEADZONE) diffY = 0;
  
  if (currentMode == MODE_BOTH) {
    if (diffX == 0 && diffY == 0) {
      targetX = round(smoothX);
      targetY = round(smoothY);
      return;
    }
    if (diffX != 0) targetX += (diffX > 0 ? STEP_X : -STEP_X);
    if (diffY != 0) targetY += (diffY > 0 ? STEP_Y : -STEP_Y);
    
  } else if (currentMode == MODE_TILT) {
    if (diffY == 0) {
      targetY = round(smoothY);
      return;
    }
    if (diffY != 0) targetY += (diffY > 0 ? STEP_Y : -STEP_Y);
    targetX = 0;
    
  } else if (currentMode == MODE_PAN) {
    if (diffX == 0) {
      targetX = round(smoothX);
      return;
    }
    if (diffX != 0) targetX += (diffX > 0 ? STEP_X : -STEP_X);
    targetY = 0;
  }
  
  targetX = constrain(targetX, -180, 180);
  targetY = constrain(targetY, -30, 60);
}

void readJoystickChannel() {
  if (!joystickCalibrated) return;
  
  static unsigned long lastSwitch = 0;
  const unsigned long switchDelay = 300;
  
  int rawX = analogRead(PIN_VRX);
  int diffX = rawX - joystickCenterX;
  
  if (abs(diffX) < CHANNEL_SWITCH_THRESHOLD) return;
  
  if (millis() - lastSwitch > switchDelay) {
    int direction = 0;
    
    if (diffX > 0) {
      currentChannel = (currentChannel + 1) % NUM_CHANNELS;
      direction = 1;
    } else {
      currentChannel = (currentChannel - 1 + NUM_CHANNELS) % NUM_CHANNELS;
      direction = -1;
    }
    
    sendChannelCommand(direction);
    lastSwitch = millis();
    needUpdate = true;
  }
}

void updateServoPosition() {
  unsigned long currentTime = millis();
  
  if (targetX != smoothX && currentTime - lastStepTimeX >= SPEED_DELAY) {
    if (smoothX < targetX) smoothX += STEP_X;
    else if (smoothX > targetX) smoothX -= STEP_X;
    lastStepTimeX = currentTime;
  }
  
  if (targetY != smoothY && currentTime - lastStepTimeY >= SPEED_DELAY) {
    if (smoothY < targetY) smoothY += STEP_Y;
    else if (smoothY > targetY) smoothY -= STEP_Y;
    lastStepTimeY = currentTime;
  }
  
  static float currentSmoothX = 0, currentSmoothY = 0;
  currentSmoothX += (smoothX - currentSmoothX) * SMOOTHING_FACTOR;
  currentSmoothY += (smoothY - currentSmoothY) * SMOOTHING_FACTOR;
  
  int newX = constrain(round(currentSmoothX), -180, 180);
  int newY = constrain(round(currentSmoothY), -30, 60);
  
  if (newX != servoX || newY != servoY) {
    servoX = newX;
    servoY = newY;
    needUpdate = true;
  }
}

// ============================================
// ===== MODULE: MODE CYCLING =====
// ============================================

void cycleMode() {
  currentMode = (currentMode + 1) % 4;
  
  targetX = 0;
  targetY = 0;
  smoothX = 0;
  smoothY = 0;
  servoX = 0;
  servoY = 0;
  
  Serial.printf("[MODE] %s\n", currentMode == 0 ? "TILT+PAN" : 
                               currentMode == 1 ? "TILT" :
                               currentMode == 2 ? "PAN" : "CHANNELS");
  needUpdate = true;
}

// ============================================
// ===== PROTOCOL: COMMANDS =====
// ============================================

void sendServoCommand() {
  char packet[64];
  snprintf(packet, sizeof(packet), "M%d,%d,0", servoX, servoY);
  sendPacket(packet);
}

void sendChannelCommand(int direction) {
  char packet[16];
  snprintf(packet, sizeof(packet), "C%d", direction);
  sendPacket(packet);
}

void sendModuleCommand(const char* moduleName, int state) {
  char packet[64];
  snprintf(packet, sizeof(packet), "MOD%s,%d", moduleName, state);
  sendPacket(packet);
}

void parseStatusPacket(const char* packet) {
  int pan, tilt, compass, ok, cal;
  if (sscanf(packet, "A=%d,%d,C=%d,OK=%d,Cal=%d", &pan, &tilt, &compass, &ok, &cal) == 5) {
    servoX = pan;
    servoY = tilt;
    compassAngle = compass;
    compassOk = (ok != 0);
    calibrated = (cal != 0);
    needUpdate = true;
  }
}

// ============================================
// ===== UI: MAIN SCREEN =====
// ============================================

void drawStaticLabels() {
  tft.setTextSize(1);
  tft.setTextColor(ST7735_YELLOW);
  
  tft.setCursor(0, 0); tft.print("Status:");
  tft.setCursor(0, 16); tft.print("X:");
  tft.setCursor(0, 32); tft.print("Y:");
  tft.setCursor(0, 48); tft.print("C:");
  tft.setCursor(0, 64); tft.print("CH:");
  tft.setCursor(0, 80); tft.print("Mode:");
  tft.setCursor(0, 96); tft.print("Cal:");
  
  tft.drawRect(55, 18, 80, 10, ST7735_WHITE);
  tft.drawRect(55, 34, 80, 10, ST7735_WHITE);
  
  btnMenu.draw(tft);
}

void updateLinkStatus() {
  bool currentLink = !linkLost;
  if (currentLink != lastLinkStatus) {
    tft.fillRect(40, 0, 80, 14, ST7735_BLACK);
    tft.setCursor(40, 0);
    tft.setTextColor(currentLink ? ST7735_GREEN : ST7735_RED);
    tft.print(currentLink ? "LINK OK" : "NO LINK");
    lastLinkStatus = currentLink;
  }
}

void updateServoX() {
  if (servoX != lastServoX) {
    tft.fillRect(15, 16, 40, 14, ST7735_BLACK);
    tft.setTextColor(ST7735_WHITE);
    tft.setCursor(15, 16);
    tft.print(servoX);
    lastServoX = servoX;
  }
}

void updateServoY() {
  if (servoY != lastServoY) {
    tft.fillRect(15, 32, 40, 14, ST7735_BLACK);
    tft.setTextColor(ST7735_WHITE);
    tft.setCursor(15, 32);
    tft.print(servoY);
    lastServoY = servoY;
  }
}

void updateCompass() {
  if (compassAngle != lastCompass || compassOk != lastCompassOk) {
    tft.fillRect(15, 48, 110, 14, ST7735_BLACK);
    tft.setCursor(15, 48);
    
    if (compassOk) {
      tft.setTextColor(ST7735_WHITE);
      tft.print(compassAngle);
      tft.print("°");
    } else {
      tft.setTextColor(ST7735_RED);
      tft.print("---");
    }
    
    lastCompass = compassAngle;
    lastCompassOk = compassOk;
  }
}

void updateChannel() {
  if (currentChannel != lastChannel) {
    tft.fillRect(25, 64, 100, 14, ST7735_BLACK);
    tft.setCursor(25, 64);
    tft.setTextColor(ST7735_WHITE);
    tft.print(CHANNELS_TABLE[currentChannel].name);
    tft.print(" ");
    tft.print(CHANNELS_TABLE[currentChannel].freq);
    lastChannel = currentChannel;
  }
}

void updateMode() {
  if (currentMode != lastMode) {
    tft.fillRect(30, 80, 80, 14, ST7735_BLACK);
    tft.setCursor(30, 80);
    tft.setTextColor(ST7735_CYAN);
    tft.print(currentMode == 0 ? "TILT+PAN" : 
              currentMode == 1 ? "TILT" :
              currentMode == 2 ? "PAN" : "CHANNELS");
    lastMode = currentMode;
  }
}

// ============================================
// ===== UI: MODULE MANAGER MENU =====
// ============================================

void drawModuleMenu() {
  static int lastSelection = -1;
  
  if (lastSelection == -1) {
    tft.fillScreen(ST7735_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(ST7735_WHITE);
    tft.setCursor(0, 0);
    tft.print("Module Manager:");
    tft.setCursor(0, 20);
    tft.print("1. Servo");
    tft.setCursor(0, 32);
    tft.print("2. Compass");
    tft.setCursor(0, 44);
    tft.print("3. Frequency");
    tft.setCursor(0, 56);
    tft.print("4. Telemetry");
    tft.setCursor(0, 76);
    tft.print("Press SW to toggle");
    tft.setCursor(0, 88);
    tft.print("Joystick Y to select");
    lastSelection = moduleSelection;
  }
  
  if (moduleSelection != lastSelection) {
    tft.fillRect(0, 20 + lastSelection * 12, 7, 12, ST7735_BLACK);
    tft.fillRect(0, 20 + moduleSelection * 12, 7, 12, ST7735_YELLOW);
    lastSelection = moduleSelection;
  }
  
  // Show states
  tft.fillRect(90, 20, 58, 12, ST7735_BLACK);
  tft.setTextColor(modules.servo_control ? ST7735_GREEN : ST7735_RED);
  tft.setCursor(90, 20);
  tft.print(modules.servo_control ? "[ON]" : "[OFF]");
  
  tft.fillRect(90, 32, 58, 12, ST7735_BLACK);
  tft.setTextColor(modules.compass ? ST7735_GREEN : ST7735_RED);
  tft.setCursor(90, 32);
  tft.print(modules.compass ? "[ON]" : "[OFF]");
  
  tft.fillRect(90, 44, 58, 12, ST7735_BLACK);
  tft.setTextColor(modules.frequency ? ST7735_GREEN : ST7735_RED);
  tft.setCursor(90, 44);
  tft.print(modules.frequency ? "[ON]" : "[OFF]");
  
  tft.fillRect(90, 56, 58, 12, ST7735_BLACK);
  tft.setTextColor(modules.telemetry ? ST7735_GREEN : ST7735_RED);
  tft.setCursor(90, 56);
  tft.print(modules.telemetry ? "[ON]" : "[OFF]");
}

void toggleModule() {
  if (moduleSelection == 0) {
    modules.servo_control = !modules.servo_control;
    sendModuleCommand("SERVO", modules.servo_control);
  } else if (moduleSelection == 1) {
    modules.compass = !modules.compass;
    sendModuleCommand("COMPASS", modules.compass);
  } else if (moduleSelection == 2) {
    modules.frequency = !modules.frequency;
    sendModuleCommand("FREQ", modules.frequency);
  } else if (moduleSelection == 3) {
    modules.telemetry = !modules.telemetry;
    sendModuleCommand("TELEM", modules.telemetry);
  }
}

// ============================================
// ===== SETUP =====
// ============================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println();
  Serial.println("╔═══════════════════════════════════════════╗");
  Serial.println("║  BLOCK 2: REMOTE WITH MODULE MANAGER    ║");
  Serial.println("║     TTL RS485 Communication v1.0         ║");
  Serial.println("╚═══════════════════════════════════════════╝");
  Serial.println();
  
  pinMode(PIN_SW, INPUT_PULLUP);
  
  // === TFT ===
  pinMode(PIN_TFT_LEDA, OUTPUT);
  digitalWrite(PIN_TFT_LEDA, HIGH);
  
  pinMode(PIN_TFT_RST, OUTPUT);
  digitalWrite(PIN_TFT_RST, HIGH);
  delay(100);
  digitalWrite(PIN_TFT_RST, LOW);
  delay(100);
  digitalWrite(PIN_TFT_RST, HIGH);
  delay(100);
  
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(0);
  
  // === RS485 ===
  pinMode(RS485_DE_PIN, OUTPUT);
  digitalWrite(RS485_DE_PIN, LOW);
  
  rs485.begin(RS485_BAUD, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
  Serial.printf("[✓] RS485 initialized at %d baud\n", RS485_BAUD);
  
  // === EEPROM ===
  EEPROM.begin(EEPROM_SIZE);
  
  // === JOYSTICK ===
  calibrateJoystick();
  
  tft.fillScreen(ST7735_BLACK);
  drawStaticLabels();
  
  lastResponseTime = millis();
  
  Serial.println();
  Serial.println("╔═══════════════════════════════════════════╗");
  Serial.println("║         SYSTEM READY                      ║");
  Serial.println("╚═══════════════════════════════════════════╝");
}

// ============================================
// ===== MAIN LOOP =====
// ============================================

void loop() {
  // === JOYSTICK READING ===
  if (!moduleMenuActive) {
    if (currentMode != MODE_CHANNELS) {
      readJoystickServo();
      updateServoPosition();
    } else {
      readJoystickChannel();
    }
  }
  
  // === BUTTON HANDLING ===
  bool btn = !digitalRead(PIN_SW);
  static bool btnPrev = false;
  static unsigned long btnDownTime = 0;
  
  int yVal = analogRead(PIN_VRY);
  static unsigned long lastMove = 0;
  
  // === MODULE MENU ===
  if (moduleMenuActive) {
    drawModuleMenu();
    
    if (yVal < 1500 && millis() - lastMove > 200) {
      moduleSelection = (moduleSelection - 1 + 4) % 4;
      lastMove = millis();
    }
    if (yVal > 2500 && millis() - lastMove > 200) {
      moduleSelection = (moduleSelection + 1) % 4;
      lastMove = millis();
    }
    
    if (btn && !btnPrev) {
      toggleModule();
      btnPrev = btn;
      delay(100);
      btnPrev = !digitalRead(PIN_SW);
      return;
    }
    
    if (!btn && btnPrev) {
      moduleMenuActive = false;
      tft.fillScreen(ST7735_BLACK);
      drawStaticLabels();
      needUpdate = true;
    }
  }
  
  // === SHORT PRESS: CYCLE MODE ===
  if (btn && !btnPrev) {
    btnDownTime = millis();
  }
  
  if (!btn && btnPrev && (millis() - btnDownTime) > 50 && (millis() - btnDownTime) < 1000) {
    cycleMode();
  }
  
  btnPrev = btn;
  
  // === SEND SERVO COMMAND ===
  static unsigned long lastSend = 0;
  if (millis() - lastSend >= 100) {
    sendServoCommand();
    lastSend = millis();
  }
  
  // === RECEIVE STATUS ===
  char receivedStatus[256];
  if (receivePacket(receivedStatus)) {
    lastResponseTime = millis();
    linkLost = false;
    parseStatusPacket(receivedStatus);
  }
  
  // === WATCHDOG ===
  if (millis() - lastResponseTime > LINK_TIMEOUT) {
    if (!linkLost) {
      linkLost = true;
      needUpdate = true;
    }
  }
  
  // === UPDATE SCREEN ===
  if (needUpdate && !moduleMenuActive) {
    updateLinkStatus();
    updateServoX();
    updateServoY();
    updateCompass();
    updateChannel();
    updateMode();
    
    btnMenu.draw(tft);
    needUpdate = false;
  }
  
  delay(10);
}
