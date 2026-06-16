#ifndef CONFIG_H
#define CONFIG_H

#include <string.h>
#include <stdint.h>
#include "Board/Pinout.h"
#include "Utility/SerialOutput.h"
#include "Utility/DebugFlags.h"

// CONFIGURACIONES GENERALES DEL PROGRAMA

// Helpers de logueo
#ifndef FILENAME
  #define FILENAME (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

#define LOG_OUT_F(fmt, ...) do { \
    stdOut.printf(fmt, ##__VA_ARGS__); \
} while(0)

#define LOG_OUT(...) do { \
    stdOut.print(__VA_ARGS__); \
} while(0)

#define LOG_OUT_LN(...) do { \
    stdOut.println(__VA_ARGS__); \
} while(0)
 
#define LOG_F(fmt, ...) do { \
    LOG_OUT("["); \
    LOG_OUT(FILENAME); \
    LOG_OUT(":"); \
    LOG_OUT(__LINE__); \
    LOG_OUT("]["); \
    LOG_OUT(__func__); \
    LOG_OUT("] "); \
    LOG_OUT_F(fmt, ##__VA_ARGS__); \
} while(0)

#define LOG_LN(msg) do { \
    LOG_OUT("["); \
    LOG_OUT(FILENAME); \
    LOG_OUT(":"); \
    LOG_OUT(__LINE__); \
    LOG_OUT("]["); \
    LOG_OUT(__func__); \
    LOG_OUT("] "); \
    LOG_OUT_LN(msg); \
} while(0)

#define LOG_P(msg) do { \
    LOG_OUT("["); \
    LOG_OUT(FILENAME); \
    LOG_OUT(":"); \
    LOG_OUT(__LINE__); \
    LOG_OUT("]["); \
    LOG_OUT(__func__); \
    LOG_OUT("] "); \
    LOG_OUT(msg); \
} while(0)

// Habilitar pruebas de componentes al inicio (desactivar para booteo rápido)
#define CHECK_SENSORS false

// Modo barebones (para debug sin tener el EOLO completo)
// simula sensores
#define BAREBONES false

// simula encoder
#define SERIAL_INPUT false

// Modelo de pantalla a usar en U8G2 (default U8G2_SSD1309_128x64_NONAME2_F_HW_I2C)
//#define DisplayModel U8G2_SSD1309_128X64_NONAME2_F_HW_I2C
#define DisplayModel U8G2_SSD1306_128X64_NONAME_F_HW_I2C

#define I2C_CLOCK 150000 

// Dirección I2C del driver de input (ATTINY85)
#define ATTINY_ADDRESS 8

// Comandos para el ATTINY85
#define CMD_RESET_COUNTER 0x01
#define CMD_RESET_BUTTON 0x02

#define MINUTE 60UL
#define HOUR 3600UL
#define DAY 86400UL

#ifndef DRONE_TARGET_FLOW_LPM
  #define DRONE_TARGET_FLOW_LPM 5.0f
#endif

#ifndef AFM07_FLOW_DIVISOR
  #define AFM07_FLOW_DIVISOR 100.0f
#endif

#ifndef NTC_MOTOR_OVERHEAT_HIGH_C
  #define NTC_MOTOR_OVERHEAT_HIGH_C 70.0f
#endif

#ifndef NTC_MOTOR_OVERHEAT_LOW_C
  #define NTC_MOTOR_OVERHEAT_LOW_C 60.0f
#endif

static_assert(NTC_MOTOR_OVERHEAT_LOW_C < NTC_MOTOR_OVERHEAT_HIGH_C,
              "NTC_MOTOR_OVERHEAT_LOW_C debe ser menor que NTC_MOTOR_OVERHEAT_HIGH_C");

#ifndef NTC_MOTOR_OVERHEAT_LOG_INTERVAL_MS
  #define NTC_MOTOR_OVERHEAT_LOG_INTERVAL_MS 5000UL
#endif

#ifndef MOTOR_RAMP_STEP
  #define MOTOR_RAMP_STEP 0
#endif

#ifndef MOTOR_RAMP_INTERVAL_MS
  #define MOTOR_RAMP_INTERVAL_MS 8
#endif

#ifndef MOTOR_PWM_RESOLUTION_BITS
  #if defined(FEATURE_MOTOR_PWM_8BIT)
    #define MOTOR_PWM_RESOLUTION_BITS 8
  #else
    #define MOTOR_PWM_RESOLUTION_BITS 11
  #endif
#endif

#if MOTOR_PWM_RESOLUTION_BITS < 1 || MOTOR_PWM_RESOLUTION_BITS > 15
  #error "MOTOR_PWM_RESOLUTION_BITS debe estar entre 1 y 15"
#endif

#ifndef FLOW_PID_INTERVAL_MS
  #define FLOW_PID_INTERVAL_MS 100
#endif

#ifndef FLOW_PID_DEADBAND
  #define FLOW_PID_DEADBAND 0.08f
#endif

#ifndef FLOW_PID_KP
  #define FLOW_PID_KP 80.0f
#endif

#ifndef FLOW_PID_KI
  #define FLOW_PID_KI 8.0f
#endif

#ifndef FLOW_PID_KD
  #define FLOW_PID_KD 6.0f
#endif

#ifndef FLOW_PID_INTEGRAL_LIMIT
  #define FLOW_PID_INTEGRAL_LIMIT 30.0f
#endif

#ifndef FLOW_PID_MAX_STEP
  #define FLOW_PID_MAX_STEP 32
#endif

#ifndef FLOW_PID_BASE_PWM
  #define FLOW_PID_BASE_PWM 300
#endif

#ifndef FLOW_PID_FILTER_ALPHA
  #define FLOW_PID_FILTER_ALPHA 0.30f
#endif

#ifndef FLOW_PID_MIN_ACTIVE
  #define FLOW_PID_MIN_ACTIVE 0.20f
#endif

#ifndef FLOW_PID_MAX_DT_MS
  #define FLOW_PID_MAX_DT_MS 1000
#endif

#ifndef FLOW_PID_SENSOR_STALE_MS
  #define FLOW_PID_SENSOR_STALE_MS 1200
#endif

#ifndef FLOW_PID_DISCOVERY_THRESHOLD_LPM
  #define FLOW_PID_DISCOVERY_THRESHOLD_LPM 0.1f
#endif

#ifndef FLOW_PID_DISCOVERY_STEP
  #define FLOW_PID_DISCOVERY_STEP 
#endif

#ifndef FLOW_PID_DISCOVERY_INTERVAL_MS
  #define FLOW_PID_DISCOVERY_INTERVAL_MS 150
#endif

#define DRONE_DURATION_INFINITE UINT32_MAX

#if defined(EOLO_TARGET_DRON) && \
    (DURATION_SW0_PIN >= 34 || DURATION_SW1_PIN >= 34) && \
    !defined(DRONE_SWITCHES_HAVE_EXTERNAL_PULLS)
  #error "GPIO34-39 no tienen pull-up/down interno para switches de dron"
#endif

#define FONT_BOLD u8g2_font_helvB10_tf
#define FONT_REGULAR u8g2_font_helvR10_tf

#define FONT_BOLD_S u8g2_font_helvB08_tf
#define FONT_REGULAR_S u8g2_font_helvR08_tf

#define FONT_BOLD_L u8g2_font_helvB12_tf
#define FONT_REGULAR_L u8g2_font_helvR12_tf

#define MODEM_POWER_ALWAYS_ON 1
#define MODEM_POWER_ON_DEMAND 2

#ifndef MODEM_POWER_MODE
  #define MODEM_POWER_MODE MODEM_POWER_ALWAYS_ON
#endif

#if MODEM_POWER_MODE != MODEM_POWER_ALWAYS_ON && MODEM_POWER_MODE != MODEM_POWER_ON_DEMAND
  #error "MODEM_POWER_MODE debe ser MODEM_POWER_ALWAYS_ON o MODEM_POWER_ON_DEMAND"
#endif

// Validación de combinaciones inválidas de feature flags
#if defined(FEATURE_FLOW_AFM07) && defined(FEATURE_FLOW_FS3000)
  #error "Conflicto! define solo un sensor de flujo (FEATURE_FLOW_AFM07 o FEATURE_FLOW_FS3000)"
#endif

#if defined(FEATURE_FLOW_CALIBRATION) && defined(FEATURE_FLOW_PID)
  #error "Conflicto! define solo un modo de flujo (FEATURE_FLOW_CALIBRATION o FEATURE_FLOW_PID)"
#endif

// FEATURE_FLOW_CALIBRATION: curva fija PWM->flujo para equipos estacionarios.
// FEATURE_FLOW_PID: control dinámico de flujo para dron.
#if !defined(FEATURE_FLOW_CALIBRATION) && !defined(FEATURE_FLOW_PID)
  #define FEATURE_FLOW_CALIBRATION
#endif

#if defined(FEATURE_MOTOR_PWM) && defined(FEATURE_MOTOR_PWM_POWER_PIN)
  #error "Conflicto! define solo un tipo de motor (FEATURE_MOTOR_PWM o FEATURE_MOTOR_PWM_POWER_PIN)"
#endif

#if !defined(FEATURE_MOTOR_PWM) && !defined(FEATURE_MOTOR_PWM_POWER_PIN)
  #define FEATURE_MOTOR_PWM
#endif

#endif // CONFIG_H
