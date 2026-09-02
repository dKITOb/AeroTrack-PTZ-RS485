// ============================================
// UNIFIED CONFIGURATION - RS485 VERSION
// AeroTrack PTZ Controller v1.0
// ============================================

#ifndef CONFIG_H
#define CONFIG_H

// ===== FIRMWARE VERSION =====
#define FIRMWARE_VERSION "AeroTrack v1.0 RS485"
#define FIRMWARE_DATE "2024-01-20"

// ===== DEBUG =====
#define DEBUG_MODE 1
#define LOG_LEVEL 3  // 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG

// ============================================
// ===== BLOCK 1: CONTROLLER =====
// ============================================

#ifdef BLOCK_1_CONTROLLER

// === PINS ===
#define PIN_PAN_SERVO 14
#define PIN_TILT_SERVO 15
#define PIN_CHANNEL_FWD 5
#define PIN_CHANNEL_BACK 6

// === RS485 PINS ===
#define RS485_RX_PIN 17
#define RS485_TX_PIN 18
#define RS485_DE_PIN 4
#define RS485_BAUD 4800

// === COMPASS (I2C) ===
#define PIN_I2C_SDA 21
#define PIN_I2C_SCL 22

// === SERVO LIMITS ===
#define SERVO_PAN_MIN -180
#define SERVO_PAN_MAX 180
#define SERVO_TILT_MIN -30
#define SERVO_TILT_MAX 60

// === MODULES (enable/disable) ===
#define MODULE_SERVO_CONTROL 1
#define MODULE_COMPASS 1
#define MODULE_FREQUENCY 1
#define MODULE_TELEMETRY 1

// === TIMEOUTS ===
#define WATCHDOG_TIMEOUT 10000
#define HEARTBEAT_INTERVAL 5000
#define IMPULSE_DURATION 200

#endif // BLOCK_1_CONTROLLER

// ============================================
// ===== BLOCK 2: REMOTE CONTROL =====
// ============================================

#ifdef BLOCK_2_REMOTE

// === TFT PINS ===
#define PIN_TFT_SCLK 12
#define PIN_TFT_MOSI 11
#define PIN_TFT_CS 10
#define PIN_TFT_DC 9
#define PIN_TFT_RST 8
#define PIN_TFT_LEDA 7

// === RS485 PINS ===
#define RS485_RX_PIN 17
#define RS485_TX_PIN 18
#define RS485_DE_PIN 4
#define RS485_BAUD 4800

// === JOYSTICK ===
#define PIN_VRX 5
#define PIN_VRY 6
#define PIN_SW 3

// === CHANNELS ===
#define NUM_CHANNELS 14
struct Channel { const char* name; uint16_t freq; };
static const Channel CHANNELS_TABLE[] = {
  {"A1", 5865}, {"A2", 5845}, {"A3", 5825}, {"A4", 5805},
  {"A5", 5785}, {"A6", 5765}, {"A7", 5745}, {"A8", 5725},
  {"1", 3300}, {"2", 3320}, {"3", 3340}, {"4", 3360},
  {"5", 3380}, {"6", 3400}
};

// === SERVO MODES ===
#define MODE_BOTH 0
#define MODE_TILT 1
#define MODE_PAN 2
#define MODE_CHANNELS 3

// === JOYSTICK ===
#define STEP_X 1
#define STEP_Y 1
#define SPEED_DELAY 200
#define JOYSTICK_DEADZONE 300
#define CHANNEL_SWITCH_THRESHOLD 400

// === UI ===
#define MENU_TIMEOUT 5000
#define LINK_TIMEOUT 1000

// === MODULES (enable/disable) ===
#define MODULE_SERVO_CONTROL 1
#define MODULE_COMPASS 1
#define MODULE_FREQUENCY 1
#define MODULE_TELEMETRY 1

#endif // BLOCK_2_REMOTE

// ============================================
// ===== COMMON SETTINGS =====
// ============================================

#define EEPROM_SIZE 64
#define EEPROM_MAGIC 0xAA

#endif // CONFIG_H
