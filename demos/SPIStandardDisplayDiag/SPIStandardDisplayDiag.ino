/*
 * ============================================================
 *  EOLO MP (Codename Eolo Grande) — Demo Secuencial  v24
 * ============================================================
 *  Plataforma : ESP32 WROOM32D
 *
 *  NOVEDAD v23/v24 — anemometro ultrasonico Comwintop (RS485):
 *   Comparte el MISMO bus RS485 que el AFM07 (mismos pines, mismo
 *   baudrate 4800) — solo cambia la direccion de esclavo Modbus
 *   (0x01 vs 0x02 del AFM07), asi que no hizo falta hardware
 *   nuevo. Se agrego una funcion generica de lectura de holding
 *   registers (modbus_leerRegistros) reutilizando el CRC16 y el
 *   control DE/RE ya existentes. Registro 0 = velocidad (x100 =
 *   m/s), registro 1 = direccion en grados. Nueva pagina 5/5 en
 *   el OLED, y columnas Viento(m/s)/Dir° sumadas a la tabla del
 *   modo interactivo (v24).
 *
 *  NOVEDAD v22 — control de bombas con el encoder:
 *   El giro del encoder ajusta el PWM (0-255) de la bomba
 *   seleccionada, en pasos de PUMP_ENCODER_STEP (4 por defecto).
 *   El boton del encoder (flanco de subida) alterna cual bomba
 *   esta seleccionada (1 <-> 2). La bomba seleccionada se marca
 *   con un "*" en la pagina 4/5 del OLED. Los comandos de Serial
 *   (1:<val>, 2:<val>, 0) siguen funcionando igual, en paralelo —
 *   ambos metodos escriben al mismo setPumpDuty().
 *   NOTA: si se usa el comando 'r' (reset contador encoder), se
 *   resetea tambien g_encPosAnterior para evitar que el salto de
 *   posicion se interprete como un giro brusco y mueva la bomba
 *   de golpe.
 *
 *  NOVEDAD v21 — OLED con rotacion de pantallas:
 *   En vez de una sola pantalla fija, el OLED ahora rota entre 5
 *   paginas cada OLED_PAGE_INTERVAL_MS (3000ms por defecto):
 *    1/5 Encoder + Energia (posicion/dir/boton + fuente/voltaje activo)
 *    2/5 BME280 (temp/hum/presion)
 *    3/5 SEN0233 (PM1.0/PM2.5/PM10 + temp/hum)
 *    4/5 Flujo AFM07 + estado de bombas
 *    5/5 Anemometro Comwintop (velocidad + direccion)
 *   La hora/log queda fija abajo en todas las paginas. Los datos
 *   de cada pagina se siguen actualizando en vivo (cada 200ms)
 *   aunque no sea su turno de mostrarse — solo cambia cual pagina
 *   esta visible en el instante dado.
 *
 *  ETAPA 2 — OLED (SPI) + Encoder I2C + Power + AFM07 + Anemometro +
 *            Bombas + BME280 + SEN0233 + RTC + SD
 *  (Set completo de perifericos del Eolo MP para esta etapa)
 *
 *  BUG CORREGIDO (v20) — bombas arrancaban a full al energizar:
 *   El usuario reporto que al encender el sistema, las bombas
 *   arrancaban a full potencia por ~1s, se apagaban, y recien
 *   funcionaban bien (con rampa) cuando el codigo llegaba a
 *   demo_bombas(). Causa: los pines 14/25 quedaban flotando (sin
 *   pinMode ni ledcAttachPin) durante Serial.begin + banner + 5s
 *   de warmup de perifericos + I2C + OLED + RS485 + SEN0233 — un
 *   MOSFET sin pull-down en el gate puede interpretar un pin
 *   flotante como "encendido". Se soluciono moviendo la
 *   inicializacion de las bombas (pinMode+digitalWrite(LOW)+
 *   ledcAttachPin+ledcWrite(0)) a las primerisimas lineas de
 *   setup(), antes de cualquier otra cosa. Sigue existiendo una
 *   ventana muy breve (entre el power-on fisico real y que el
 *   codigo llegue a esas lineas) que el software NO puede cubrir
 *   — si el glitch persiste tras este fix, la solucion definitiva
 *   es agregar una resistencia de pull-down fisica en el gate de
 *   cada MOSFET de bomba.
 *
 *  Componentes en esta etapa:
 *   - 1x OLED 2.42" — SPI de hardware (4-wire), ver historial abajo
 *   - 1x Arduino Pro Mini esclavo I2C (encoder + boton), addr 0x08
 *   - 1x Microcontrolador gestor de energia (I2C esclavo, addr 0x0A):
 *     conmuta entre entrada DC y 2 baterias de respaldo, reporta
 *     fuente activa + 3 voltajes (DC/Batt1/Batt2)
 *   - 1x AFM07 — sensor de flujo, Modbus RTU / RS485, addr 0x02
 *   - 1x Anemometro ultrasonico Comwintop — RS485 Modbus RTU,
 *     addr 0x01, MISMO bus fisico que el AFM07
 *   - 2x Bomba DC (MOSFET low-side, PWM directo, sin rampa/NTC)
 *   - 1x BME280 — temperatura/humedad/presion, I2C (bus principal)
 *   - 1x DFRobot SEN0233 — PM1.0/2.5/10 + Temp/Hum, UART (Serial2),
 *     protocolo Plantower, mismo N-FET de enable que el resto
 *   - 1x RTC DS3231 — I2C
 *   - 1x Modulo SD — SPI (log CSV, comparte bus VSPI con el OLED)
 *   - 1x Enable general de perifericos (N-FET, IO4)
 *
 *  Mapa de pines (confirmado por usuario):
 *   I2C principal (ProMini/Power/BME280/RTC): SDA=21  SCL=22
 *   OLED (SPI, bus VSPI compartido con SD):
 *     SCK=18  MISO=19  MOSI=23  (defaults VSPI, iguales a la SD)
 *     CS=27  DC=15  RES=2
 *   RS485 (Serial1, compartido AFM07 + Anemometro):
 *     DI=33  RO=35  DE/RE=26  |  Baud=4800 8N1
 *     AFM07 addr=0x02  |  Anemometro addr=0x01
 *   SEN0233 (Serial2):      RX2=IO34 (PLANTOWER_RX)  TX2=IO32 (PLANTOWER_TX)
 *   Bomba 1 (PWM/MOSFET):   IO14
 *   Bomba 2 (PWM/MOSFET):   IO25
 *   SD (VSPI):              CS=5  (SCK/MISO/MOSI compartidos con OLED)
 *   Enable perifericos:     IO4  (N-FET Gate, HIGH = encendido)
 *
 *  PARCHE DE SOFTWARE (encoder invertido):
 *   El encoder esta mal conectado en el PCB del Eolo MP (fase A/B
 *   cruzada), por lo que reporta movimientos invertidos. Se corrige
 *   en software con el flag ENCODER_INVERTIDO (ver mas abajo), que
 *   invierte tanto la direccion (CW<->CCW) como el signo de la
 *   posicion. Poner en 0 si se corrige el cableado en una revision
 *   futura del PCB.
 *
 *  HISTORIAL DEL PROBLEMA DE OLED (I2C, resuelto pasando a SPI en v17):
 *   El OLED quedaba recurrentemente en estado "muerto" (panel
 *   apagado, pero el controlador seguia con ACK en el bus I2C) en
 *   TODOS los prototipos. Investigacion, en orden:
 *    1) Conector/soldadura fisica del panel — descartado.
 *    2) Clock stretching de los ATmega328P compartiendo el bus I2C
 *       — se aislo el OLED a bus DEDICADO por hardware (Wire1) y el
 *       problema EMPEORO (100% de fallas en vez de intermitente).
 *    3) Bug del periferico Wire1 (I2C_NUM_1) del core esp32 2.0.17
 *       — confirmado al cambiar a I2C por SOFTWARE (bit-bang, mismos
 *       pines) y encender la pantalla de inmediato. Pero seguia
 *       fallando intermitentemente con el tiempo (bit-bang no fue
 *       solucion 100% estable tampoco).
 *   DECISION FINAL: migrar el OLED de I2C a SPI de hardware por
 *   completo, eliminando la variable I2C del problema. El modulo
 *   OLED usado soporta ambos modos (jumpers de resistencia en la
 *   placa, revisar posicion R9/R0/R1/R2 si se reutiliza otro modulo).
 *   PINES SPI: CS=27 y DC=15 reutilizan los pines de MOTOR_DRIVER_2
 *   y MOTOR_DRIVER_4 del conector, que en este PCB NO estan poblados
 *   (confirmado por el usuario) — no hay conflicto real. RES=IO2 se
 *   rescato del circuito del NeoPixel, que esta variante del Eolo MP
 *   NO lleva poblado; tiene un pulldown de 1K existente por temas de
 *   auto-flasheo del ESP32, pero no interfiere (solo importa durante
 *   el instante de boot, antes de que el codigo tome control del pin).
 *   Con este pinout las 2 BOMBAS (IO14/IO25) quedan intactas.
 *   NOTA: durante el debug aislado del OLED por SPI aparecio un bug
 *   distinto (no relacionado a lo anterior): el sketch de test
 *   standalone se habia armado sin encender el N-FET de perifericos
 *   (IO4), causando brillo tenue por alimentacion parasita via los
 *   diodos ESD de las lineas SPI. Este demo completo SI enciende
 *   IO4 correctamente desde siempre, asi que no aplica aca — se deja
 *   anotado por si se arman mas tests aislados a futuro.
 *
 *  DIAGNOSTICO DISPONIBLE (comandos por Serial en modo interactivo):
 *   d → fuerza "Entire Display ON" (0xA5) — bypassea la RAM de video
 *       para testear si el panel/charge-pump puede prender pixeles.
 *       Usa u8g2.sendF(), generico para cualquier bus (I2C o SPI).
 *   n → vuelve a modo normal (0xA4).
 *   c → limpia el bit OSF del RTC (sin tocar fecha/hora).
 *
 *  Librerias (Arduino Library Manager):
 *   - U8g2
 *   - RTClib (Adafruit)
 *   - Adafruit_BME280 (+ Adafruit_Sensor, Adafruit BusIO)
 *   - SD (incluida en core ESP32)
 * ============================================================
 */

#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <RTClib.h>
#include <U8g2lib.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

// ── Pines ────────────────────────────────────────────────────
#define PIN_SDA              21   // Bus I2C principal (encoder/power/RTC)
#define PIN_SCL              22
#define PIN_PERIF_ENABLE      4   // Gate del N-FET que alimenta a los perifericos
#define PIN_SD_CS             5   // CS de la SD (VSPI: SCK=18 MISO=19 MOSI=23, compartido con OLED)

#define PIN_OLED_CS          27   // OLED SPI — reutiliza MOTOR_DRIVER_2 (sin poblar)
#define PIN_OLED_DC          15   // OLED SPI — reutiliza MOTOR_DRIVER_4 (sin poblar)
#define PIN_OLED_RES          2   // OLED SPI — rescatado del NeoPixel (sin poblar en esta variante)
#define PIN_SPI_SCK          18   // VSPI default, compartido con SD
#define PIN_SPI_MISO         19   // VSPI default, compartido con SD (OLED no lo usa, solo firma la SD)
#define PIN_SPI_MOSI         23   // VSPI default, compartido con SD

#define PIN_RS485_DI         33   // TX del ESP32 -> DI del MAX485
#define PIN_RS485_RO         35   // RX del ESP32 <- RO del MAX485
#define PIN_RS485_DERE       26   // DE y RE unidos a este pin

#define PIN_PMS_RX           34   // RX2 <- TX del SEN0233 (PLANTOWER_RX en el esquematico)
#define PIN_PMS_TX           32   // TX2 -> RX del SEN0233 (PLANTOWER_TX en el esquematico)

#define PIN_PUMP1            14
#define PIN_PUMP2            25

// ── Bombas (PWM directo, sin rampa) ─────────────────────────
#define PUMP_PWM_FREQ    20000
#define PUMP_PWM_RES         8
#define PUMP1_CH             0
#define PUMP2_CH             1
#define PUMP_MAX_LEDC       255

// ── I2C — frecuencia reducida ────────────────────────────────
// Se aplica al bus I2C principal (encoder/power/RTC). Historial:
// era mitigacion cuando el OLED tambien vivia en I2C; ya no aplica
// al OLED (migrado a SPI en v17), pero se mantiene para el bus
// principal por robustez general con los esclavos ATmega328P.
#define I2C_CLOCK_HZ    50000

// ── Enable perifericos ───────────────────────────────────────
#define PERIF_WARMUP_MS    5000  // Ampliado (antes 500ms) para dar mas margen de estabilizacion
// NOTA: por ahora usamos el mismo criterio que el warmup del PMS en el
// Eolo Express (encendido continuo desde setup). Ajustar si el Eolo MP
// requiere mas tiempo de estabilizacion para OLED/encoder.

// ── Encoder remoto — ProMini esclavo I2C ────────────────────
#define ENCODER_I2C_ADDR   0x08
#define CMD_RESET_COUNTER  0x01
#define CMD_RESET_BUTTON   0x02

// PARCHE DE SOFTWARE: encoder conectado invertido en el PCB del Eolo MP
// (fase A/B cruzada). Se invierte direccion y signo de posicion en
// software para no depender de un cambio de hardware/firmware.
#define ENCODER_INVERTIDO   1

struct {
  int8_t  pos = 0;
  uint8_t dir = 0;     // 0=CW 1=CCW
  bool    btn = false;
} g_enc;

// ── Gestor de energia — microcontrolador esclavo I2C (0x0A) ─
// Administra la conmutacion entre entrada DC y dos baterias de
// respaldo (MOSFETs de potencia). Reporta por I2C: fuente activa
// + los 3 voltajes medidos. Paquete de 14 bytes armado en su
// requestEvent(): [activeMosfet(1)][dcV(4)][batt1V(4)][batt2V(4)][activeMosfet(1)]
#define POWER_I2C_ADDR       0x0A
#define POWER_PACKET_SIZE    14

struct {
  uint8_t activeMosfet = 0;   // 0=Error/ninguno 1=DC 2=Bateria1 3=Bateria2
  float   dcVoltage    = 0;
  float   batt1Voltage = 0;
  float   batt2Voltage = 0;
  bool    ok            = false;
} g_power;

const char* power_nombreFuente(uint8_t code) {
  switch (code) {
    case 1:  return "DC";
    case 2:  return "BATT1";
    case 3:  return "BATT2";
    default: return "ERROR";
  }
}

// ── OLED ─────────────────────────────────────────────────────
// Migrado a SPI de hardware (ver historial completo en el header).
// Constructor 4-wire HW SPI: usa el bus SPI por defecto (VSPI),
// compartido con la SD (cada uno con su propio CS).
U8G2_SSD1309_128X64_NONAME0_F_4W_HW_SPI u8g2(U8G2_R0,
  /* cs=*/    PIN_OLED_CS,
  /* dc=*/    PIN_OLED_DC,
  /* reset=*/ PIN_OLED_RES);
#define OLED_I2C_ADDR   0x3C  // ya no aplica (SPI no tiene direccion), se deja solo por si se vuelve a I2C

// ── RTC ──────────────────────────────────────────────────────
RTC_DS3231 rtc;
bool rtcOK = false;

// ── BME280 — temp/hum/presion, I2C (bus principal) ──────────
Adafruit_BME280 bme;
bool bmeOK = false;

// ── AFM07 — sensor de flujo, Modbus RTU / RS485 ─────────────
#define AFM07_ADDR        0x02   // Direccion esclavo Modbus (confirmado por usuario)
#define AFM07_BAUD        4800   // Baudrate configurado en el sensor (confirmado por usuario)
HardwareSerial RS485Serial(1);

// ── DFRobot SEN0233 — PM1.0/2.5/10 + Temp/Hum, UART (Serial2) ─
// Protocolo Plantower (32 bytes), mismo N-FET de enable (IO4) que
// el resto de perifericos. Pines confirmados por el usuario segun
// esquematico de la placa hija: RX2=IO34 (ESP32 recibe del sensor),
// TX2=IO32 (ESP32 transmite al sensor).
HardwareSerial PMSSerial(2);
#define PMS_WARMUP_MS    3000   // Margen tras estabilizar alimentacion antes de leer

struct {
  uint16_t pm1_0 = 0, pm2_5 = 0, pm10 = 0;
  float    tempC = 0, humPct = 0;   // SEN0233 trae T/H propios en el mismo frame
  bool pmsOK = false;
} g_pms;

// ── Logging ──────────────────────────────────────────────────
#define LOG_INTERVAL_MS    5000    // Guardar en CSV cada 5s (etapa de test: mas frecuente)
char     g_logFilename[32] = "";
bool     g_logActivo       = false;
uint32_t g_lastLog         = 0;

// ── Estado ───────────────────────────────────────────────────
uint32_t g_lastLectura = 0;
#define DISPLAY_INTERVAL_MS 200   // Refresco rapido para ver el encoder responder

int g_pump1LEDC = 0;
int g_pump2LEDC = 0;

// ── Control de bombas por encoder ───────────────────────────
// El giro del encoder ajusta el PWM de la bomba seleccionada;
// el boton alterna cual bomba esta seleccionada (1 <-> 2).
#define PUMP_ENCODER_STEP  4   // cuanto PWM cambia por "click" del encoder
uint8_t g_pumpSeleccionada = 1;
int8_t  g_encPosAnterior   = 0;   // usado por encoder_controlarBombas(); se resetea junto con 'r'

// ── Rotacion de pantallas OLED ───────────────────────────────
#define OLED_PAGE_INTERVAL_MS  3000   // Tiempo que se muestra cada pantalla
#define OLED_NUM_PAGES         5
uint8_t  g_oledPage        = 0;
uint32_t g_lastPageSwitch  = 0;

// Ultima lectura de flujo (guardada en global para poder mostrarla
// en el OLED sin depender de la variable local de leerYMostrar())
float g_ultimoFlujo = 0;
enum { FLUJO_OK, FLUJO_TIMEOUT, FLUJO_ERR_CRC } g_flujoEstado = FLUJO_TIMEOUT;

// ============================================================
//  UTILIDADES
// ============================================================
void printHeader(const char* t) {
  Serial.println(F("\n╔══════════════════════════════════════════╗"));
  Serial.print(  F("║  ")); Serial.print(t);
  Serial.println(F("\n╚══════════════════════════════════════════╝"));
}

// ============================================================
//  ENCODER — lectura y comandos
// ============================================================
bool encoder_leer() {
  uint8_t n = Wire.requestFrom((uint8_t)ENCODER_I2C_ADDR, (uint8_t)3);
  if (n < 3) return false;
  uint8_t dir  = Wire.read();
  uint8_t cnt8 = Wire.read();
  uint8_t btn  = Wire.read();

#if ENCODER_INVERTIDO
  dir  = dir ? 0 : 1;              // CW <-> CCW
  cnt8 = (uint8_t)(-(int8_t)cnt8); // invierte el signo de la posicion
#endif

  g_enc.dir = dir;
  g_enc.pos = (int8_t)cnt8;
  g_enc.btn = (btn != 0);
  return true;
}

void encoder_enviarComando(uint8_t cmd) {
  Wire.beginTransmission(ENCODER_I2C_ADDR);
  Wire.write(cmd);
  Wire.endTransmission();
}

// ============================================================
//  GESTOR DE ENERGIA — lectura del paquete de 14 bytes
// ============================================================
bool power_leer() {
  uint8_t n = Wire.requestFrom((uint8_t)POWER_I2C_ADDR, (uint8_t)POWER_PACKET_SIZE);
  if (n < POWER_PACKET_SIZE) { g_power.ok = false; return false; }

  uint8_t buf[POWER_PACKET_SIZE];
  for (int i = 0; i < POWER_PACKET_SIZE; i++) buf[i] = Wire.read();

  g_power.activeMosfet = buf[0];
  memcpy(&g_power.dcVoltage,    &buf[1], 4);
  memcpy(&g_power.batt1Voltage, &buf[5], 4);
  memcpy(&g_power.batt2Voltage, &buf[9], 4);
  // buf[13] trae activeMosfet repetido (lo manda el esclavo dos veces);
  // no lo usamos, pero podria servir a futuro para validar consistencia.

  g_power.ok = true;
  return true;
}

// ============================================================
//  BOMBAS — control directo por PWM (sin rampa, sin proteccion termica)
// ============================================================
void setPumpDuty(uint8_t pumpIndex, int target) {
  target = constrain(target, 0, PUMP_MAX_LEDC);
  if (pumpIndex == 1) {
    g_pump1LEDC = target;
    ledcWrite(PUMP1_CH, target);
  } else if (pumpIndex == 2) {
    g_pump2LEDC = target;
    ledcWrite(PUMP2_CH, target);
  }
}

// Traduce los movimientos del encoder a control de bombas:
// el giro ajusta el PWM de la bomba seleccionada, el boton
// (flanco de subida) alterna cual bomba esta seleccionada.
void encoder_controlarBombas(bool encoderOK) {
  if (!encoderOK) return;

  static bool botonAnterior = false;

  int8_t delta = g_enc.pos - g_encPosAnterior;
  g_encPosAnterior = g_enc.pos;

  if (delta != 0) {
    int actual = (g_pumpSeleccionada == 1) ? g_pump1LEDC : g_pump2LEDC;
    setPumpDuty(g_pumpSeleccionada, actual + delta * PUMP_ENCODER_STEP);
  }

  if (g_enc.btn && !botonAnterior) {
    g_pumpSeleccionada = (g_pumpSeleccionada == 1) ? 2 : 1;
    Serial.printf("  → Bomba seleccionada por encoder: %d\n", g_pumpSeleccionada);
  }
  botonAnterior = g_enc.btn;
}

// ============================================================
//  AFM07 — Modbus RTU / RS485
// ============================================================
uint16_t crc16Modbus(const uint8_t* d, uint8_t len) {
  uint16_t crc = 0xFFFF;
  for (uint8_t i = 0; i < len; i++) {
    crc ^= d[i];
    for (uint8_t j = 0; j < 8; j++)
      crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
  }
  return crc;
}

void rs485Transmit(bool enable) {
  digitalWrite(PIN_RS485_DERE, enable ? HIGH : LOW);
}

// Lee caudal instantaneo (reg 0x0000, escala x10 -> L/min)
// Retorna -1.0 en timeout, -2.0 en error de CRC
float afm07_leerFlujo() {
  while (RS485Serial.available()) RS485Serial.read();

  uint8_t t[8] = { AFM07_ADDR, 0x03, 0x00, 0x00, 0x00, 0x01, 0, 0 };
  uint16_t crc = crc16Modbus(t, 6);
  t[6] = crc & 0xFF; t[7] = crc >> 8;

  rs485Transmit(true);
  delayMicroseconds(500);
  RS485Serial.write(t, 8);
  RS485Serial.flush();
  delayMicroseconds(500);
  rs485Transmit(false);

  uint32_t t0 = millis();
  while (RS485Serial.available() < 7 && millis() - t0 < 300) delay(1);
  if (RS485Serial.available() < 7) return -1.0F;

  uint8_t r[7];
  for (int i = 0; i < 7; i++) r[i] = RS485Serial.read();
  if (crc16Modbus(r, 5) != (r[5] | (uint16_t)r[6] << 8)) return -2.0F;

  return ((uint16_t)(r[3] << 8) | r[4]) / 10.0F;  // escala x10 segun datasheet AFM07
}

// ============================================================
//  Modbus generico — lectura de holding registers (0x03)
// ============================================================
// Comparte el mismo bus RS485 (RS485Serial, mismos pines y
// baudrate 4800) que el AFM07 — cada dispositivo tiene su propia
// direccion de esclavo, asi que no hace falta hardware extra.
bool modbus_leerRegistros(uint8_t slaveId, uint16_t startReg, uint16_t count, uint16_t* out) {
  while (RS485Serial.available()) RS485Serial.read();

  uint8_t req[8] = {
    slaveId, 0x03,
    (uint8_t)(startReg >> 8), (uint8_t)(startReg & 0xFF),
    (uint8_t)(count >> 8),    (uint8_t)(count & 0xFF),
    0, 0
  };
  uint16_t crc = crc16Modbus(req, 6);
  req[6] = crc & 0xFF; req[7] = crc >> 8;

  rs485Transmit(true);
  delayMicroseconds(500);
  RS485Serial.write(req, 8);
  RS485Serial.flush();
  delayMicroseconds(500);
  rs485Transmit(false);

  const uint8_t expectedLen = 5 + (count * 2);
  uint8_t resp[64];
  uint8_t idx = 0;
  uint32_t t0 = millis();
  while ((millis() - t0) < 300 && idx < expectedLen) {
    if (RS485Serial.available()) resp[idx++] = RS485Serial.read();
  }
  if (idx != expectedLen) return false;

  uint16_t crcRecibido  = resp[idx - 2] | ((uint16_t)resp[idx - 1] << 8);
  uint16_t crcCalculado = crc16Modbus(resp, idx - 2);
  if (crcRecibido != crcCalculado) return false;
  if (resp[0] != slaveId || resp[1] != 0x03 || resp[2] != count * 2) return false;

  for (uint16_t i = 0; i < count; i++) {
    uint8_t off = 3 + (i * 2);
    out[i] = (resp[off] << 8) | resp[off + 1];
  }
  return true;
}

// ============================================================
//  Anemometro ultrasonico Comwintop — RS485 Modbus RTU
// ============================================================
#define ANEM_ADDR        0x01   // Direccion esclavo (distinta del AFM07 = 0x02)
#define ANEM_START_REG   0x0000
#define ANEM_REG_COUNT   2      // reg0=velocidad(x100 m/s), reg1=direccion(grados)

struct {
  float    velocidadMs = 0;
  uint16_t direccionDeg = 0;
  bool     ok = false;
} g_anem;

bool anemometro_leer() {
  uint16_t regs[2];
  if (!modbus_leerRegistros(ANEM_ADDR, ANEM_START_REG, ANEM_REG_COUNT, regs)) {
    g_anem.ok = false;
    return false;
  }
  g_anem.velocidadMs  = regs[0] / 100.0F;
  g_anem.direccionDeg = regs[1];
  g_anem.ok = true;
  return true;
}

// ============================================================
//  DFRobot SEN0233 — PM2.5 + Temp/Hum — UART (protocolo Plantower)
// ============================================================
// Frame de 32 bytes. Header 0x42 0x4D (mismo que PMS estandar), con
// T/H propias en bytes [24:25] y [26:27] (formato fijo punto
// decimal /10, ej. 235 -> 23.5).
bool pms_leerFrame() {
  while (PMSSerial.available() >= 32) {
    if (PMSSerial.peek() != 0x42) { PMSSerial.read(); continue; }

    uint8_t buf[32];
    if (PMSSerial.available() < 32) return false;
    for (int i = 0; i < 32; i++) buf[i] = PMSSerial.read();
    if (buf[0] != 0x42 || buf[1] != 0x4D) continue;

    uint16_t sum = 0;
    for (int i = 0; i < 30; i++) sum += buf[i];
    uint16_t checksum = (buf[30] << 8) | buf[31];
    if (sum != checksum) {
      g_pms.pmsOK = false;
      continue;
    }

    g_pms.pm1_0 = (buf[10] << 8) | buf[11];
    g_pms.pm2_5 = (buf[12] << 8) | buf[13];
    g_pms.pm10  = (buf[14] << 8) | buf[15];

    uint16_t tempRaw = (buf[24] << 8) | buf[25];
    uint16_t humRaw  = (buf[26] << 8) | buf[27];
    g_pms.tempC  = tempRaw / 10.0F;
    g_pms.humPct = humRaw  / 10.0F;

    g_pms.pmsOK = true;
    return true;
  }
  return false;
}

// ============================================================
//  DEMOS
// ============================================================
void demo_scanI2C() {
  printHeader("1. SCANNER I2C");

  Serial.println(F("  Bus PRINCIPAL (Wire, SDA=21 SCL=22):"));
  uint8_t found = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.printf("    0x%02X", a);
      if (a == ENCODER_I2C_ADDR)  Serial.print("  ← ProMini Encoder");
      if (a == POWER_I2C_ADDR)    Serial.print("  ← Gestor de Energia");
      if (a == 0x68 || a == 0x69) Serial.print("  ← DS3231 RTC");
      if (a == 0x57)              Serial.print("  ← DS3231 EEPROM");
      Serial.println(); found++;
    }
    delay(5);
  }
  if (!found) Serial.println("    ⚠  Ningún dispositivo.");
  else        Serial.printf("    Total: %d\n", found);

  Serial.println(F("  OLED: ahora por SPI (no I2C) — verificar visualmente en el paso 2."));
}

void demo_oled() {
  printHeader("2. OLED 2.42\" (U8G2 / SSD1309 SPI)");
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 12, "EOLO MP");
  u8g2.drawHLine(0, 14, 128);
  u8g2.drawStr(0, 28, "Test OLED OK");
  u8g2.drawStr(0, 40, "Patron: rectangulo");
  u8g2.drawFrame(0, 45, 128, 15);
  u8g2.drawBox(4, 49, 30, 7);
  u8g2.sendBuffer();
  Serial.println("  ✓ Buffer enviado. Verificar visualmente el display.");
  delay(1500);
}

void demo_encoder() {
  printHeader("3. ENCODER REMOTO (ProMini I2C 0x08)");
  bool ok = encoder_leer();
  if (ok) {
    Serial.printf("  Posicion: %d  |  Dir: %s  |  Boton: %s\n",
      g_enc.pos, g_enc.dir == 0 ? "CW" : "CCW", g_enc.btn ? "PRESIONADO" : "libre");
  } else {
    Serial.println("  ⚠  ProMini no respondio en 0x08.");
  }
}

void demo_power() {
  printHeader("4. GESTOR DE ENERGIA (I2C 0x0A)");
  bool ok = power_leer();
  if (ok) {
    Serial.printf("  Fuente activa: %s (codigo %d)\n",
      power_nombreFuente(g_power.activeMosfet), g_power.activeMosfet);
    Serial.printf("  DC:    %.2f V\n", g_power.dcVoltage);
    Serial.printf("  Batt1: %.2f V\n", g_power.batt1Voltage);
    Serial.printf("  Batt2: %.2f V\n", g_power.batt2Voltage);
    if (g_power.activeMosfet == 0) {
      Serial.println("  ⚠  Ninguna fuente por encima del umbral (14.0V) — posible falla de alimentacion.");
    }
  } else {
    Serial.println("  ⚠  Gestor de energia no respondio en 0x0A.");
  }
}

void demo_afm07() {
  printHeader("5. SENSOR DE FLUJO AFM07 (RS485 Modbus RTU)");
  Serial.printf("  Slave: 0x%02X  |  Baud: %d  |  8N1\n", AFM07_ADDR, AFM07_BAUD);
  for (int i = 1; i <= 3; i++) {
    float f = afm07_leerFlujo();
    Serial.printf("  Lectura #%d: ", i);
    if      (f == -1.0F) Serial.println("TIMEOUT");
    else if (f == -2.0F) Serial.println("ERROR CRC");
    else                 Serial.printf("%.1f L/min\n", f);
    delay(200);
  }
}

void demo_anemometro() {
  printHeader("6. ANEMOMETRO ULTRASONICO COMWINTOP (RS485)");
  Serial.printf("  Slave: 0x%02X  |  Baud: %d  |  8N1  (mismo bus que AFM07)\n", ANEM_ADDR, AFM07_BAUD);
  for (int i = 1; i <= 3; i++) {
    bool ok = anemometro_leer();
    Serial.printf("  Lectura #%d: ", i);
    if (ok) Serial.printf("%.2f m/s (%.2f km/h)  |  Direccion: %u°\n",
              g_anem.velocidadMs, g_anem.velocidadMs * 3.6F, g_anem.direccionDeg);
    else    Serial.println("SIN RESPUESTA (timeout o CRC)");
    delay(200);
  }
}

void demo_bombas() {
  printHeader("7. BOMBAS DC (MOSFET + PWM)");
  Serial.println(F("  Barrido rapido 0 -> 255 -> 0 en ambas bombas (sin rampa)"));
  int pasos[] = {0, 64, 128, 192, 255, 128, 0};
  for (int v : pasos) {
    setPumpDuty(1, v);
    setPumpDuty(2, v);
    Serial.printf("  PWM: %d\n", v);
    delay(400);
  }
}

void demo_bme280() {
  printHeader("8. SENSOR BME280");
  bmeOK = bme.begin(0x76) || bme.begin(0x77);
  if (!bmeOK) {
    Serial.println("  ⚠  BME280 no encontrado (0x76/0x77). Continuando sin el sensor.");
    return;
  }
  Serial.printf("  Temperatura:  %.2f °C\n",  bme.readTemperature());
  Serial.printf("  Humedad:      %.2f %%\n",  bme.readHumidity());
  Serial.printf("  Presión:      %.2f hPa\n", bme.readPressure() / 100.0F);
}

void demo_pms() {
  printHeader("9. SENSOR DFRobot SEN0233 (PM2.5 + Temp/Hum)");
  Serial.println(F("  Esperando frames validos (hasta 5s)..."));
  uint32_t t0 = millis();
  bool ok = false;
  while (millis() - t0 < 5000) {
    if (pms_leerFrame()) { ok = true; break; }
    delay(10);
  }
  if (ok) {
    Serial.printf("  PM1.0:  %u ug/m3\n", g_pms.pm1_0);
    Serial.printf("  PM2.5:  %u ug/m3\n", g_pms.pm2_5);
    Serial.printf("  PM10:   %u ug/m3\n", g_pms.pm10);
    Serial.printf("  Temp:   %.1f C\n",   g_pms.tempC);
    Serial.printf("  Hum:    %.1f %%\n",  g_pms.humPct);
  } else {
    Serial.println("  ⚠  Sin datos del sensor (revisar cableado UART / N-FET).");
  }
}

// ============================================================
//  DS3231 — acceso directo al registro de estado (diagnostico OSF)
// ============================================================
#define DS3231_I2C_ADDR    0x68
#define DS3231_REG_STATUS  0x0F
#define DS3231_OSF_BIT     0x80

uint8_t ds3231_leerStatusRaw() {
  Wire.beginTransmission(DS3231_I2C_ADDR);
  Wire.write(DS3231_REG_STATUS);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)DS3231_I2C_ADDR, (uint8_t)1);
  if (Wire.available()) return Wire.read();
  return 0xFF; // 0xFF = no respondio el dispositivo
}

// Limpia UNICAMENTE el bit OSF (latch de "oscilador detenido").
// No toca fecha/hora ni ningun otro bit del registro de estado.
void ds3231_limpiarOSF() {
  uint8_t status = ds3231_leerStatusRaw();
  status &= ~DS3231_OSF_BIT;

  Wire.beginTransmission(DS3231_I2C_ADDR);
  Wire.write(DS3231_REG_STATUS);
  Wire.write(status);
  Wire.endTransmission();
}

void demo_rtc() {
  printHeader("10. RTC DS3231");
  rtcOK = rtc.begin(&Wire);
  if (!rtcOK) { Serial.println("  ✗ DS3231 no encontrado."); return; }

  uint8_t statusRaw = ds3231_leerStatusRaw();
  Serial.printf("  Registro STATUS (0x0F) crudo: 0x%02X  (OSF=%d)\n",
    statusRaw, (statusRaw & DS3231_OSF_BIT) ? 1 : 0);

  if (rtc.lostPower()) {
    Serial.println("  ⚠  OSF activo → el RTC perdio la nocion del tiempo.");
    Serial.println("  ℹ  No se ajusta la hora automaticamente en este demo.");
    Serial.println("  ℹ  Usar la demo de seteo de hora para presetear el RTC.");
  } else {
    Serial.println("  ✓  OSF en 0 → el RTC mantuvo la hora correctamente.");
  }

  DateTime now = rtc.now();
  const char* d[] = {"Dom", "Lun", "Mar", "Mie", "Jue", "Vie", "Sab"};
  Serial.printf("  Fecha:    %s %04d-%02d-%02d\n",
    d[now.dayOfTheWeek()], now.year(), now.month(), now.day());
  Serial.printf("  Hora:     %02d:%02d:%02d\n", now.hour(), now.minute(), now.second());
  Serial.printf("  Temp RTC: %.2f °C\n", rtc.getTemperature());
}

void demo_sdInfo() {
  printHeader("11. TARJETA SD — CARACTERÍSTICAS");
  if (!SD.begin(PIN_SD_CS)) { Serial.println("  ✗ SD no inicializada."); return; }
  const char* tp[] = {"NONE", "MMC", "SDSC", "SDHC/SDXC", "UNKNOWN"};
  uint8_t t = SD.cardType();
  Serial.printf("  Tipo:      %s\n",   t <= 4 ? tp[t] : tp[4]);
  Serial.printf("  Capacidad: %llu MB\n", SD.cardSize()  / (1024ULL * 1024));
  Serial.printf("  Usado:     %llu MB\n", SD.usedBytes() / (1024ULL * 1024));
}

// ============================================================
//  LOG CSV — SD (requiere RTC ya inicializado)
// ============================================================
void log_iniciar() {
  printHeader("12. LOG CSV — SD");

  if (!rtcOK) {
    Serial.println("  ✗ LOG: RTC no disponible, no se puede generar el nombre de archivo.");
    g_logActivo = false;
    return;
  }

  DateTime now = rtc.now();
  snprintf(g_logFilename, sizeof(g_logFilename),
    "/LOG_%04d%02d%02d_%02d%02d%02d.CSV",
    now.year(), now.month(), now.day(),
    now.hour(), now.minute(), now.second());

  File f = SD.open(g_logFilename, FILE_WRITE);
  if (!f) {
    Serial.printf("  ✗ LOG: no se pudo crear %s\n", g_logFilename);
    g_logActivo = false;
    return;
  }

  f.println("datetime,enc_pos,enc_dir,enc_btn,power_source,dc_v,batt1_v,batt2_v,flujo_lmin,pump1_ledc,pump2_ledc,bme_temp_c,bme_hum,bme_pres_hpa,pm1_0,pm2_5,pm10,wind_ms,wind_deg,rtc_temp_c");
  f.close();

  g_logActivo = true;
  Serial.printf("  ✓ LOG: archivo creado → %s\n", g_logFilename);
}

void log_escribirFila(DateTime dt, int8_t encPos, uint8_t encDir, bool encBtn, float flujo,
                       float bTemp, float bHum, float bPres, float rtcTemp) {
  if (!g_logActivo) return;

  File f = SD.open(g_logFilename, FILE_APPEND);
  if (!f) {
    Serial.println("  ⚠ LOG: no se pudo abrir el archivo para escribir.");
    g_logActivo = false;
    return;
  }

  char linea[280];
  snprintf(linea, sizeof(linea),
    "%04d-%02d-%02d %02d:%02d:%02d,%d,%d,%d,%s,%.2f,%.2f,%.2f,%.2f,%d,%d,%.1f,%.1f,%.1f,%u,%u,%u,%.2f,%u,%.1f",
    dt.year(), dt.month(), dt.day(), dt.hour(), dt.minute(), dt.second(),
    encPos, encDir, encBtn ? 1 : 0,
    power_nombreFuente(g_power.activeMosfet),
    g_power.dcVoltage, g_power.batt1Voltage, g_power.batt2Voltage,
    flujo, g_pump1LEDC, g_pump2LEDC, bTemp, bHum, bPres,
    g_pms.pm1_0, g_pms.pm2_5, g_pms.pm10,
    g_anem.velocidadMs, g_anem.direccionDeg, rtcTemp);

  f.println(linea);
  f.close();
}

// ============================================================
//  OLED — pantallas en vivo (modo interactivo, rotan cada 3s)
// ============================================================
// Valores de BME280 cacheados (se leen una vez por ciclo en
// leerYMostrar() y se reusan aca, para no duplicar trafico I2C)
float g_bmeTemp = 0, g_bmeHum = 0, g_bmePres = 0;

void oled_dibujarHoraLog() {
  char line[32];
  if (rtcOK) {
    DateTime now = rtc.now();
    snprintf(line, sizeof(line), "%02d:%02d:%02d %s",
      now.hour(), now.minute(), now.second(),
      g_logActivo ? "LOG:ON" : "LOG:--");
  } else {
    snprintf(line, sizeof(line), "RTC n/d");
  }
  u8g2.drawStr(0, 62, line);
}

void oled_pagina_encoderPower() {
  char line[32];
  u8g2.drawStr(0, 9, "1/5 Encoder+Energia");
  u8g2.drawHLine(0, 11, 128);

  snprintf(line, sizeof(line), "Pos:%d Dir:%s", g_enc.pos, g_enc.dir == 0 ? "CW" : "CCW");
  u8g2.drawStr(0, 23, line);

  snprintf(line, sizeof(line), "Boton: %s", g_enc.btn ? "PRESIONADO" : "libre");
  u8g2.drawStr(0, 34, line);

  if (g_power.ok) {
    float vActiva = g_power.activeMosfet == 1 ? g_power.dcVoltage :
                    g_power.activeMosfet == 2 ? g_power.batt1Voltage :
                    g_power.activeMosfet == 3 ? g_power.batt2Voltage : 0.0F;
    snprintf(line, sizeof(line), "Pwr:%s %.1fV",
      power_nombreFuente(g_power.activeMosfet), vActiva);
  } else {
    snprintf(line, sizeof(line), "Pwr: n/d");
  }
  u8g2.drawStr(0, 45, line);
}

void oled_pagina_ambiente() {
  char line[32];
  u8g2.drawStr(0, 9, "2/5 BME280");
  u8g2.drawHLine(0, 11, 128);

  if (bmeOK) {
    snprintf(line, sizeof(line), "Temp: %.1f C", g_bmeTemp);
    u8g2.drawStr(0, 23, line);
    snprintf(line, sizeof(line), "Hum:  %.1f %%", g_bmeHum);
    u8g2.drawStr(0, 34, line);
    snprintf(line, sizeof(line), "Pres: %.1f hPa", g_bmePres);
    u8g2.drawStr(0, 45, line);
  } else {
    u8g2.drawStr(0, 23, "BME280 n/d");
  }
}

void oled_pagina_particulas() {
  char line[32];
  u8g2.drawStr(0, 9, "3/5 SEN0233 (PM)");
  u8g2.drawHLine(0, 11, 128);

  if (g_pms.pmsOK) {
    snprintf(line, sizeof(line), "PM1.0:%u PM2.5:%u", g_pms.pm1_0, g_pms.pm2_5);
    u8g2.drawStr(0, 23, line);
    snprintf(line, sizeof(line), "PM10: %u", g_pms.pm10);
    u8g2.drawStr(0, 34, line);
    snprintf(line, sizeof(line), "T:%.1fC H:%.1f%%", g_pms.tempC, g_pms.humPct);
    u8g2.drawStr(0, 45, line);
  } else {
    u8g2.drawStr(0, 23, "SEN0233 n/d");
  }
}

void oled_pagina_flujoBombas() {
  char line[32];
  u8g2.drawStr(0, 9, "4/5 Flujo+Bombas");
  u8g2.drawHLine(0, 11, 128);

  if      (g_flujoEstado == FLUJO_OK)      snprintf(line, sizeof(line), "Flujo: %.1f L/min", g_ultimoFlujo);
  else if (g_flujoEstado == FLUJO_TIMEOUT) snprintf(line, sizeof(line), "Flujo: TIMEOUT");
  else                                     snprintf(line, sizeof(line), "Flujo: ERR_CRC");
  u8g2.drawStr(0, 23, line);

  snprintf(line, sizeof(line), "%sP1:%3d  %sP2:%3d",
    g_pumpSeleccionada == 1 ? "*" : " ", g_pump1LEDC,
    g_pumpSeleccionada == 2 ? "*" : " ", g_pump2LEDC);
  u8g2.drawStr(0, 34, line);

  u8g2.drawStr(0, 45, "(*=seleccionada x encoder)");
}

void oled_pagina_viento() {
  char line[32];
  u8g2.drawStr(0, 9, "5/5 Anemometro");
  u8g2.drawHLine(0, 11, 128);

  if (g_anem.ok) {
    snprintf(line, sizeof(line), "Vel: %.2f m/s", g_anem.velocidadMs);
    u8g2.drawStr(0, 23, line);
    snprintf(line, sizeof(line), "     %.1f km/h", g_anem.velocidadMs * 3.6F);
    u8g2.drawStr(0, 34, line);
    snprintf(line, sizeof(line), "Direccion: %u°", g_anem.direccionDeg);
    u8g2.drawStr(0, 45, line);
  } else {
    u8g2.drawStr(0, 23, "Anemometro n/d");
  }
}

void oled_dibujarPagina() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);

  switch (g_oledPage) {
    case 0: oled_pagina_encoderPower(); break;
    case 1: oled_pagina_ambiente();     break;
    case 2: oled_pagina_particulas();   break;
    case 3: oled_pagina_flujoBombas();  break;
    case 4: oled_pagina_viento();       break;
  }

  oled_dibujarHoraLog();
  u8g2.sendBuffer();
}



// ============================================================
//  OLED — diagnostico crudo (bypass de U8G2)
// ============================================================

// Envia un comando crudo al controlador SSD1309 usando la capa
// generica de u8g2 (u8g2.sendF), que funciona igual sea el
// transporte HW I2C (Wire/Wire1) o SW I2C (bit-bang) — asi el
// diagnostico sirve sin importar que build de OLED este activo.
void oled_comandoRaw(uint8_t cmd) {
  u8g2.sendF("c", cmd);
  Serial.printf("  [OLED] comando 0x%02X enviado (via u8g2, transporte actual: SPI)\n", cmd);
}

// Fuerza TODOS los pixeles a encendido, ignorando la RAM de video.
// Si esto no ilumina la pantalla, la falla es de la etapa de potencia/
// charge-pump o del panel (conector FPC, soldadura, etc.), no del
// dibujo por software.
void oled_testEntireDisplayOn(bool encender) {
  oled_comandoRaw(encender ? 0xA5 : 0xA4);  // 0xA5=Entire Display ON, 0xA4=normal (usa RAM)
}

// ============================================================
//  MODO INTERACTIVO
// ============================================================
void imprimirAyuda() {
  Serial.println(F("\n════════════════════════════════════════════════════"));
  Serial.println(F("  MODO INTERACTIVO — OLED + Encoder + Energia + Flujo + Bombas + BME280 + SEN0233 + RTC + SD"));
  Serial.println(F("════════════════════════════════════════════════════"));
  Serial.println(F("  Comandos por Serial:"));
  Serial.println(F("    r   → reset contador del encoder"));
  Serial.println(F("    b   → reset flag de boton"));
  Serial.println(F("    c   → limpiar bit OSF del RTC (sin tocar fecha/hora)"));
  Serial.println(F("    d   → test OLED: forzar TODOS los pixeles ON (bypass RAM)"));
  Serial.println(F("    n   → OLED: volver a modo normal (usa RAM de video)"));
  Serial.println(F("    1:<0-255>   → set PWM bomba 1   (ej: 1:200)"));
  Serial.println(F("    2:<0-255>   → set PWM bomba 2   (ej: 2:100)"));
  Serial.println(F("    0           → ambas bombas a 0"));
  Serial.println(F("    [Encoder]   → gira: ajusta PWM de la bomba seleccionada"));
  Serial.println(F("                boton: alterna bomba seleccionada (1 <-> 2)"));
  Serial.println(F("════════════════════════════════════════════════════"));
  if (g_logActivo) Serial.printf("  Log CSV activo → %s\n", g_logFilename);
  else              Serial.println(F("  ⚠ Log CSV NO disponible (RTC o SD no detectados)"));
  Serial.println(F("════════════════════════════════════════════════════\n"));
  Serial.println(F("  Pos  | Dir | Boton      | Fuente | DC(V) | B1(V) | B2(V) | Flujo(L/m) | P1  | P2  | BME-T | BME-H | BME-P  | PM2.5 | PM10 | Viento(m/s) | Dir° | Hora     | RTC(°C)"));
  Serial.println(F("  -----|-----|------------|--------|-------|-------|-------|------------|-----|-----|-------|-------|--------|-------|------|-------------|------|----------|--------"));
}

void procesarSerialInput() {
  if (!Serial.available()) return;
  String s = Serial.readStringUntil('\n');
  s.trim();
  if (s.length() == 0) return;

  if (s == "r") {
    encoder_enviarComando(CMD_RESET_COUNTER);
    g_encPosAnterior = 0; // evita un salto brusco de PWM en encoder_controlarBombas()
    Serial.println(F("  → Contador de encoder reiniciado."));
  } else if (s == "b") {
    encoder_enviarComando(CMD_RESET_BUTTON);
    Serial.println(F("  → Flag de boton reiniciado."));
  } else if (s == "c") {
    if (!rtcOK) {
      Serial.println(F("  ⚠ RTC no disponible, no se puede limpiar OSF."));
    } else {
      ds3231_limpiarOSF();
      uint8_t status = ds3231_leerStatusRaw();
      Serial.printf("  → OSF limpiado. STATUS ahora: 0x%02X  (OSF=%d)\n",
        status, (status & DS3231_OSF_BIT) ? 1 : 0);
    }
  } else if (s == "d") {
    oled_testEntireDisplayOn(true);
    Serial.println(F("  → OLED: comando 0xA5 enviado (Entire Display ON)."));
    Serial.println(F("     ¿Se ilumina TODA la pantalla en blanco/gris parejo?"));
  } else if (s == "n") {
    oled_testEntireDisplayOn(false);
    Serial.println(F("  → OLED: comando 0xA4 enviado (modo normal, usa RAM)."));
  } else if (s == "0") {
    setPumpDuty(1, 0);
    setPumpDuty(2, 0);
    Serial.println(F("  → Ambas bombas detenidas."));
  } else if (s.indexOf(':') > 0) {
    int sep = s.indexOf(':');
    int pumpIndex = s.substring(0, sep).toInt();
    int val = constrain(s.substring(sep + 1).toInt(), 0, 255);
    if (pumpIndex == 1 || pumpIndex == 2) {
      setPumpDuty(pumpIndex, val);
      Serial.printf("  → Bomba %d = %d\n", pumpIndex, val);
    } else {
      Serial.println(F("  ⚠ Bomba invalida. Usar 1 o 2."));
    }
  } else {
    Serial.println(F("  ⚠ Comando invalido. Usar 'r', 'b', 'c', 'd', 'n', '1:<0-255>', '2:<0-255>' o '0'."));
  }
}

void leerYMostrar() {
  if (millis() - g_lastLectura < DISPLAY_INTERVAL_MS) return;
  g_lastLectura = millis();

  bool ok = encoder_leer();
  encoder_controlarBombas(ok);
  power_leer();
  float flujo = afm07_leerFlujo();

  char flujoStr[12];
  if      (flujo == -1.0F) snprintf(flujoStr, sizeof(flujoStr), "TIMEOUT");
  else if (flujo == -2.0F) snprintf(flujoStr, sizeof(flujoStr), "ERR_CRC");
  else                     snprintf(flujoStr, sizeof(flujoStr), "%.1f", flujo);

  float bTemp = bmeOK ? bme.readTemperature() : NAN;
  float bHum  = bmeOK ? bme.readHumidity()    : NAN;
  float bPres = bmeOK ? bme.readPressure() / 100.0F : NAN;
  g_bmeTemp = bTemp; g_bmeHum = bHum; g_bmePres = bPres; // cache para el OLED

  g_ultimoFlujo = flujo;
  g_flujoEstado = (flujo == -1.0F) ? FLUJO_TIMEOUT : (flujo == -2.0F) ? FLUJO_ERR_CRC : FLUJO_OK;

  anemometro_leer(); // mismo bus RS485 que el AFM07; actualiza g_anem

  pms_leerFrame(); // actualiza g_pms si llega un frame nuevo; conserva el ultimo valor si no

  DateTime now;
  float rtcTemp = NAN;
  if (rtcOK) {
    now = rtc.now();
    rtcTemp = rtc.getTemperature();
    Serial.printf("  %4d | %3s | %-10s | %-6s | %5.2f | %5.2f | %5.2f | %10s | %3d | %3d | ",
      g_enc.pos, g_enc.dir == 0 ? "CW" : "CCW",
      !ok ? "SIN RESP." : (g_enc.btn ? "PRESIONADO" : "libre"),
      power_nombreFuente(g_power.activeMosfet),
      g_power.dcVoltage, g_power.batt1Voltage, g_power.batt2Voltage,
      flujoStr, g_pump1LEDC, g_pump2LEDC);
    if (bmeOK) Serial.printf("%5.1f | %5.1f | %6.1f | ", bTemp, bHum, bPres);
    else       Serial.print(" n/d  |  n/d  |   n/d  | ");
    if (g_pms.pmsOK) Serial.printf("%5u | %4u | ", g_pms.pm2_5, g_pms.pm10);
    else             Serial.print(" n/d  |  n/d | ");
    if (g_anem.ok) Serial.printf("%11.2f | %4u | ", g_anem.velocidadMs, g_anem.direccionDeg);
    else           Serial.print("        n/d | n/d  | ");
    Serial.printf("%02d:%02d:%02d | %5.1f\n", now.hour(), now.minute(), now.second(), rtcTemp);
  } else {
    Serial.printf("  %4d | %3s | %-10s | %-6s | %5.2f | %5.2f | %5.2f | %10s | %3d | %3d | ",
      g_enc.pos, g_enc.dir == 0 ? "CW" : "CCW",
      !ok ? "SIN RESP." : (g_enc.btn ? "PRESIONADO" : "libre"),
      power_nombreFuente(g_power.activeMosfet),
      g_power.dcVoltage, g_power.batt1Voltage, g_power.batt2Voltage,
      flujoStr, g_pump1LEDC, g_pump2LEDC);
    if (bmeOK) Serial.printf("%5.1f | %5.1f | %6.1f | ", bTemp, bHum, bPres);
    else       Serial.print(" n/d  |  n/d  |   n/d  | ");
    if (g_pms.pmsOK) Serial.printf("%5u | %4u | ", g_pms.pm2_5, g_pms.pm10);
    else             Serial.print(" n/d  |  n/d | ");
    if (g_anem.ok) Serial.printf("%11.2f | %4u | ", g_anem.velocidadMs, g_anem.direccionDeg);
    else           Serial.print("        n/d | n/d  | ");
    Serial.println("n/d      |   n/d");
  }

  if (millis() - g_lastPageSwitch >= OLED_PAGE_INTERVAL_MS) {
    g_lastPageSwitch = millis();
    g_oledPage = (g_oledPage + 1) % OLED_NUM_PAGES;
  }
  oled_dibujarPagina();

  if (rtcOK && millis() - g_lastLog >= LOG_INTERVAL_MS) {
    g_lastLog = millis();
    float flujoLog = (flujo == -1.0F || flujo == -2.0F) ? 0.0F : flujo;
    log_escribirFila(now, g_enc.pos, g_enc.dir, g_enc.btn, flujoLog, bTemp, bHum, bPres, rtcTemp);
  }
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
  // ── Bombas — PRIMERO QUE NADA, antes de Serial/banner/warmup ──
  // Motivo: sin esto, los pines 14/25 quedan flotando durante todo
  // el resto de la inicializacion (Serial, banner, 5s de warmup de
  // perifericos, I2C, OLED, RS485, SEN0233) — varios segundos en
  // los que un MOSFET sin pull-down en el gate puede interpretar
  // el pin flotante como "encendido", arrancando la bomba a full
  // apenas hay alimentacion. Forzar el pin a salida+LOW lo antes
  // posible en el codigo reduce esa ventana al minimo. Sigue
  // existiendo una ventana MUY breve entre el power-on fisico y que
  // el codigo llegue a esta linea que el software no puede controlar
  // — si el glitch persiste, la solucion definitiva es agregar una
  // resistencia de pull-down fisica en el gate de cada MOSFET.
  pinMode(PIN_PUMP1, OUTPUT);
  pinMode(PIN_PUMP2, OUTPUT);
  digitalWrite(PIN_PUMP1, LOW);
  digitalWrite(PIN_PUMP2, LOW);
  ledcSetup(PUMP1_CH, PUMP_PWM_FREQ, PUMP_PWM_RES);
  ledcSetup(PUMP2_CH, PUMP_PWM_FREQ, PUMP_PWM_RES);
  ledcAttachPin(PIN_PUMP1, PUMP1_CH);
  ledcAttachPin(PIN_PUMP2, PUMP2_CH);
  ledcWrite(PUMP1_CH, 0);
  ledcWrite(PUMP2_CH, 0);

  Serial.begin(115200);
  delay(1500);

  Serial.println(F("\n╔══════════════════════════════════════════╗"));
  Serial.println(F("║  EOLO MP — Demo Secuencial v24           ║"));
  Serial.println(F("║  Etapa 2 — set completo de perifericos   ║"));
  Serial.println(F("╚══════════════════════════════════════════╝"));

  // ── Enable general de perifericos ──
  pinMode(PIN_PERIF_ENABLE, OUTPUT);
  digitalWrite(PIN_PERIF_ENABLE, HIGH);
  Serial.println(F("  → Perifericos: N-FET activado (IO4), esperando estabilizacion..."));
  delay(PERIF_WARMUP_MS);

  // ── I2C principal (encoder / power / RTC) ──
  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(I2C_CLOCK_HZ);
  Wire.setTimeOut(50); // ms — evita que un dispositivo ausente/colgado trabe el bus indefinidamente

  // ── OLED (SPI de hardware, bus VSPI compartido con la SD) ──
  SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI);
  bool oledOK = u8g2.begin();
  Serial.printf("  → OLED u8g2.begin(): %s\n", oledOK ? "OK" : "FALLO");
  u8g2.setContrast(255); // maximo brillo (0-255); bajar si se ve muy fuerte
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 20, "Eolo MP");
  u8g2.drawStr(0, 35, "Iniciando...");
  u8g2.sendBuffer();
  delay(500);

  // ── RS485 / AFM07 ──
  pinMode(PIN_RS485_DERE, OUTPUT);
  digitalWrite(PIN_RS485_DERE, LOW);
  RS485Serial.begin(AFM07_BAUD, SERIAL_8N1, PIN_RS485_RO, PIN_RS485_DI);

  // ── SEN0233 (Plantower, mismo N-FET de enable que el resto) ──
  PMSSerial.begin(9600, SERIAL_8N1, PIN_PMS_RX, PIN_PMS_TX);
  delay(PMS_WARMUP_MS); // margen extra de estabilizacion antes de leer frames

  // ── Demos secuenciales ──
  demo_scanI2C();   delay(600);
  demo_oled();      delay(400);
  demo_encoder();   delay(400);
  demo_power();     delay(400);
  demo_afm07();     delay(400);
  demo_anemometro(); delay(400);
  demo_bombas();    delay(400);
  demo_bme280();    delay(400);
  demo_pms();       delay(400);
  demo_rtc();       delay(400);
  demo_sdInfo();    delay(400);

  // ── Iniciar log CSV (requiere RTC ya inicializado) ──
  log_iniciar();

  imprimirAyuda();
  g_lastLectura = millis();
  g_lastLog     = millis();
  g_lastPageSwitch = millis();
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
  procesarSerialInput();
  leerYMostrar();
}
