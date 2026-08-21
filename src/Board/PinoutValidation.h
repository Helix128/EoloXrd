#ifndef EOLO_BOARD_PINOUT_VALIDATION_H
#define EOLO_BOARD_PINOUT_VALIDATION_H

#ifndef EOLO_PINOUT_COMPLETE
  #error "El pinout activo debe definir un mapa completo"
#endif

#if EOLO_SAME_USED_PIN(RS485_RX_PIN, RS485_TX_PIN) || \
    EOLO_SAME_USED_PIN(RS485_RX_PIN, RS485_DE_RE_PIN) || \
    EOLO_SAME_USED_PIN(RS485_TX_PIN, RS485_DE_RE_PIN)
  #error "Pinout invalido: RS485 tiene pines duplicados"
#endif

#if EOLO_SAME_USED_PIN(MOTOR_PWM_PIN_0, MOTOR_PWM_PIN_1)
  #error "Pinout invalido: motores PWM duplican GPIO"
#endif

#if EOLO_SAME_USED_PIN(WAIT_SW0_PIN, WAIT_SW1_PIN) || \
    EOLO_SAME_USED_PIN(WAIT_SW0_PIN, DURATION_SW0_PIN) || \
    EOLO_SAME_USED_PIN(WAIT_SW0_PIN, DURATION_SW1_PIN) || \
    EOLO_SAME_USED_PIN(WAIT_SW1_PIN, DURATION_SW0_PIN) || \
    EOLO_SAME_USED_PIN(WAIT_SW1_PIN, DURATION_SW1_PIN) || \
    EOLO_SAME_USED_PIN(DURATION_SW0_PIN, DURATION_SW1_PIN)
  #error "Pinout invalido: switches DIP duplican GPIO"
#endif

#if EOLO_INPUT_ONLY_PIN(RS485_TX_PIN) || EOLO_INPUT_ONLY_PIN(RS485_DE_RE_PIN)
  #error "Pinout invalido: RS485 TX y DE/RE no pueden usar GPIO34-39"
#endif

#if EOLO_INPUT_ONLY_PIN(MOTOR_PWM_PIN_0) || EOLO_INPUT_ONLY_PIN(MOTOR_PWM_PIN_1) || EOLO_INPUT_ONLY_PIN(MOTOR_POWER_PIN)
  #error "Pinout invalido: control de motor no puede usar GPIO34-39"
#endif

#if EOLO_INPUT_ONLY_PIN(PT_TX) || EOLO_INPUT_ONLY_PIN(PPH_PWR_PIN) || EOLO_INPUT_ONLY_PIN(MODEM_PWR_PIN) || EOLO_INPUT_ONLY_PIN(MODEM_RX_PIN)
  #error "Pinout invalido: salidas de perifericos/modem no pueden usar GPIO34-39"
#endif

#if defined(EOLO_TARGET_DRON) && PPH_PWR_PIN == 4
  #error "EOLO Dron usa GPIO4 como RS485 DE/RE; no lo configures como PPH_PWR_PIN"
#endif

#if defined(EOLO_TARGET_DRON) && MODEM_PWR_PIN == 13
  #error "EOLO Dron usa GPIO13 como DIP4; no lo configures como modem"
#endif

#if defined(FEATURE_FLOW_AFM07) && \
    (RS485_RX_PIN < 0 || RS485_TX_PIN < 0 || RS485_DE_RE_PIN < 0)
  #error "FEATURE_FLOW_AFM07 requiere un bus RS485 completo"
#endif

#if (defined(FEATURE_MOTOR_PWM) || defined(FEATURE_MOTOR_PWM_POWER_PIN)) && \
    (MOTOR_PWM_PIN_0 < 0 || MOTOR_PWM_PIN_COUNT < 1)
  #error "El motor PWM activo requiere al menos un GPIO PWM"
#endif

#if defined(FEATURE_HEADLESS) && \
    (WAIT_SW0_PIN < 0 || WAIT_SW1_PIN < 0 || \
     DURATION_SW0_PIN < 0 || DURATION_SW1_PIN < 0)
  #error "FEATURE_HEADLESS requiere los cuatro switches de captura"
#endif

#if defined(FEATURE_NTC) && NTC_PIN < 0
  #error "FEATURE_NTC requiere NTC_PIN configurado"
#endif

#if defined(FEATURE_NTC) && EOLO_SAME_USED_PIN(NTC_PIN, BATTERY_ADC_PIN)
  #error "Pinout invalido: NTC_PIN y BATTERY_ADC_PIN duplican GPIO"
#endif

#if defined(FEATURE_NTC) && !EOLO_INPUT_ONLY_PIN(NTC_PIN)
  #warning "NTC_PIN recomendado en GPIO34-39 para entrada ADC-only"
#endif

#if defined(FEATURE_NEOPIXEL) && NEOPIXEL_PIN < 0
  #error "FEATURE_NEOPIXEL requiere NEOPIXEL_PIN configurado"
#endif

#if defined(FEATURE_NEOPIXEL) && \
    (EOLO_SAME_USED_PIN(NEOPIXEL_PIN, MOTOR_PWM_PIN_0) || \
     EOLO_SAME_USED_PIN(NEOPIXEL_PIN, MOTOR_PWM_PIN_1))
  #error "Pinout invalido: NeoPixel duplica GPIO de motor PWM"
#endif

#if defined(FEATURE_PLANTOWER) && (PT_RX < 0 || PT_TX < 0)
  #error "FEATURE_PLANTOWER requiere PT_RX y PT_TX configurados"
#endif

#if defined(FEATURE_PLANTOWER) && EOLO_SAME_USED_PIN(PT_RX, PT_TX)
  #error "Pinout invalido: Plantower RX/TX duplican GPIO"
#endif

#if defined(FEATURE_PLANTOWER) && \
    (EOLO_SAME_USED_PIN(PT_RX, MOTOR_PWM_PIN_0) || \
     EOLO_SAME_USED_PIN(PT_RX, MOTOR_PWM_PIN_1) || \
     EOLO_SAME_USED_PIN(PT_TX, MOTOR_PWM_PIN_0) || \
     EOLO_SAME_USED_PIN(PT_TX, MOTOR_PWM_PIN_1))
  #error "Pinout invalido: Plantower duplica GPIO de motor PWM"
#endif

#if defined(FEATURE_PLANTOWER) && !defined(FEATURE_DUAL_BATTERY) && \
    (EOLO_SAME_USED_PIN(PT_RX, BATTERY_ADC_PIN) || EOLO_SAME_USED_PIN(PT_TX, BATTERY_ADC_PIN))
  #error "Pinout invalido: Plantower duplica GPIO de bateria ADC"
#endif

#if defined(FEATURE_PLANTOWER) && defined(FEATURE_HEADLESS) && \
    (EOLO_SAME_USED_PIN(PT_RX, WAIT_SW0_PIN) || \
     EOLO_SAME_USED_PIN(PT_RX, WAIT_SW1_PIN) || \
     EOLO_SAME_USED_PIN(PT_RX, DURATION_SW0_PIN) || \
     EOLO_SAME_USED_PIN(PT_RX, DURATION_SW1_PIN) || \
     EOLO_SAME_USED_PIN(PT_TX, WAIT_SW0_PIN) || \
     EOLO_SAME_USED_PIN(PT_TX, WAIT_SW1_PIN) || \
     EOLO_SAME_USED_PIN(PT_TX, DURATION_SW0_PIN) || \
     EOLO_SAME_USED_PIN(PT_TX, DURATION_SW1_PIN))
  #error "Pinout invalido: Plantower duplica GPIO de switches headless"
#endif

#if defined(FEATURE_MODEM) && \
    (MODEM_PWR_PIN < 0 || MODEM_RX_PIN < 0 || MODEM_TX_PIN < 0)
  #error "FEATURE_MODEM requiere pines PWR/RX/TX configurados"
#endif

#if defined(FEATURE_PLANTOWER) && defined(FEATURE_MODEM) && \
    (EOLO_SAME_USED_PIN(PT_RX, MODEM_RX_PIN) || \
     EOLO_SAME_USED_PIN(PT_RX, MODEM_TX_PIN) || \
     EOLO_SAME_USED_PIN(PT_TX, MODEM_RX_PIN) || \
     EOLO_SAME_USED_PIN(PT_TX, MODEM_TX_PIN))
  #error "Pinout invalido: Plantower duplica GPIO de modem"
#endif

#endif
