#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <RTClib.h>
#include <ModbusMaster.h>
#include <Adafruit_BME280.h>
#include <BluetoothSerial.h>

// =====================================================
// Salida de depuracion dual: USB Serial + Bluetooth Classic (SPP).
// Permite ver el log sin el cable USB conectado (que en este PCB
// tambien carga la bateria, contaminando las mediciones de consumo).
// Empareja "EoloDrone" desde Windows y abre el puerto COM que crea.
// Nota: en el modulo WROOM-32U (conector U.FL) esto requiere una
// antena externa conectada; sin ella el radio no es visible ni por
// Bluetooth ni por WiFi (asi se detecto el problema real).
// =====================================================
BluetoothSerial SerialBT;

class DualPrint : public Print {
 public:
  size_t write(uint8_t c) override {
    Serial.write(c);
    if (SerialBT.hasClient()) {
      SerialBT.write(c);
    }
    return 1;
  }

  size_t write(const uint8_t *buffer, size_t size) override {
    Serial.write(buffer, size);
    if (SerialBT.hasClient()) {
      SerialBT.write(buffer, size);
    }
    return size;
  }
};

DualPrint dbg;

// =====================================================
// Ultimo error: cualquier modulo puede reportar aqui su fallo mas
// reciente con setLastError(). Se muestra en la ultima fila de la
// OLED (ver updateOLEDEncoder) ademas de loguearse por dbg. No se
// limpia solo -- queda visible hasta que algo lo reemplace.
// =====================================================
static char lastErrorMsg[24] = "";

static void setLastError(const char *msg) {
  // Solo loguea cuando el mensaje cambia, para que un error que se
  // repite (ej. AFM07 sin respuesta cada 500ms) no inunde el log; la
  // pantalla igual lo mantiene visible en la ultima fila.
  if (strcmp(lastErrorMsg, msg) != 0) {
    dbg.printf("ERROR: %s\n", msg);
  }
  snprintf(lastErrorMsg, sizeof(lastErrorMsg), "%s", msg);
}

// =====================================================
// Test: Tarjeta SD (SPI estandar)
// CS  -> GPIO5
// SCK -> GPIO18 (VSPI, por defecto)
// MISO-> GPIO19 (VSPI, por defecto)
// MOSI-> GPIO23 (VSPI, por defecto)
// =====================================================
#define SD_CS_PIN 5

static void printCardType(uint8_t cardType) {
  switch (cardType) {
    case CARD_MMC:  dbg.println("Tipo: MMC"); break;
    case CARD_SD:   dbg.println("Tipo: SDSC"); break;
    case CARD_SDHC: dbg.println("Tipo: SDHC"); break;
    default:        dbg.println("Tipo: desconocido"); break;
  }
}

static void testSDWriteRead() {
  const char *path = "/eolo_test.txt";

  File f = SD.open(path, FILE_WRITE);
  if (!f) {
    setLastError("SD: fallo escritura");
    return;
  }
  f.println("EoloDrone SD test OK");
  f.close();
  dbg.println("Escritura OK");

  f = SD.open(path);
  if (!f) {
    setLastError("SD: fallo lectura");
    return;
  }
  dbg.print("Contenido leido: ");
  while (f.available()) {
    dbg.write(f.read());
  }
  f.close();
}

static void setupSD() {
  dbg.println("--- Test SD ---");
  if (!SD.begin(SD_CS_PIN)) {
    setLastError("SD: no monta (CS/cableado)");
    return;
  }

  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    setLastError("SD: sin tarjeta");
    return;
  }
  printCardType(cardType);

  uint64_t cardSizeMB = SD.cardSize() / (1024 * 1024);
  dbg.printf("Tamano de la tarjeta: %lluMB\n", cardSizeMB);

  testSDWriteRead();
}

// =====================================================
// Definicion de buses I2C (compartida entre modulos)
// I2C1 (Wire):  SDA=GPIO21, SCL=GPIO22 -> OLED, RTC, BME280
// I2C2 (Wire1): SDA=GPIO32, SCL=GPIO33 -> Encoder
// =====================================================
#define I2C1_SDA_PIN 21
#define I2C1_SCL_PIN 22
#define I2C1_FREQ_HZ 100000

#define I2C2_SDA_PIN 32
#define I2C2_SCL_PIN 33
#define I2C2_FREQ_HZ 10000

// Se inicializan ambos buses juntos, lo antes posible en setup(), para
// que cualquier cosa que los use despues (escaneo, setup de un modulo
// especifico, en el orden que sea) los encuentre ya listos. Antes el
// begin() de cada bus quedaba atado al setup() de un periferico en
// particular, y al reordenar modulos entre buses el escaneo terminaba
// corriendo antes de que el bus real estuviera inicializado.
static void setupI2CBuses() {
  Wire.begin(I2C1_SDA_PIN, I2C1_SCL_PIN, I2C1_FREQ_HZ);
  Wire1.begin(I2C2_SDA_PIN, I2C2_SCL_PIN, I2C2_FREQ_HZ);
}

// =====================================================
// Test: Encoder via I2C2 (esclavo ATmega328)
// =====================================================
#define ENCODER_I2C_ADDR 0x08
#define ENCODER_INIT_MAX_RETRIES 10
#define ENCODER_INIT_RETRY_DELAY_MS 300

// Definidas mas abajo, junto con el resto del codigo de cada modulo
static void updateOLEDEncoder(uint8_t dir, uint8_t cnt8, uint8_t btn);
static void updateOLEDSettings(uint8_t contrast);
static void updateOLEDEncoderRetry(uint8_t attempt, uint8_t maxAttempts);
static void setPumpDuty(uint8_t duty);

// Menu de settings: el boton del encoder alterna entre la vista normal
// y el menu; dentro del menu, el mismo giro del encoder (de momento
// solo hay un ajuste: contraste de la OLED) reemplaza el control de
// la bomba mientras se esta configurando.
enum UiMode { UI_NORMAL, UI_SETTINGS_CONTRAST };
static UiMode uiMode = UI_NORMAL;

static bool readEncoder(uint8_t &dir, uint8_t &cnt8, uint8_t &btn) {
  Wire1.requestFrom(ENCODER_I2C_ADDR, 3);
  if (Wire1.available() < 3) {
    return false;
  }
  dir  = Wire1.read();
  cnt8 = Wire1.read();
  btn  = Wire1.read();
  return true;
}

// El encoder a veces no respondia al primer intento (I2C2 se vio
// intermitente). En vez de arrancar a ciegas, se reintenta detectarlo
// por direccion antes de seguir, mostrando el numero de intento en la
// OLED para que quede visible si esta costando engancharlo.
static void setupEncoderInit() {
  for (uint8_t attempt = 1; attempt <= ENCODER_INIT_MAX_RETRIES; attempt++) {
    Wire1.beginTransmission(ENCODER_I2C_ADDR);
    uint8_t err = Wire1.endTransmission();
    if (err == 0) {
      dbg.printf("Encoder detectado (intento %u/%u)\n", attempt, ENCODER_INIT_MAX_RETRIES);
      return;
    }

    updateOLEDEncoderRetry(attempt, ENCODER_INIT_MAX_RETRIES);
    delay(ENCODER_INIT_RETRY_DELAY_MS);
  }

  char msg[24];
  snprintf(msg, sizeof(msg), "Encoder no detectado (%u)", ENCODER_INIT_MAX_RETRIES);
  setLastError(msg);
}

// Ultima lectura valida; si el encoder no responde este ciclo, se
// mantiene el valor anterior (no se pisa con datos invalidos).
static uint8_t encLastDir = 0;
static uint8_t encLastPos = 0;
static uint8_t encLastBtn = 0;

static void loopEncoderPoll() {
  static unsigned long tPoll = 0;

  if (millis() - tPoll >= 200) {
    tPoll = millis();

    uint8_t dir, cnt8, btn;
    if (readEncoder(dir, cnt8, btn)) {
      encLastDir = dir;
      encLastPos = cnt8;
      encLastBtn = btn;

      if (btn) {
        uiMode = (uiMode == UI_NORMAL) ? UI_SETTINGS_CONTRAST : UI_NORMAL;
      }

      if (uiMode == UI_SETTINGS_CONTRAST) {
        updateOLEDSettings(cnt8);
      } else {
        setPumpDuty(cnt8);
        updateOLEDEncoder(dir, cnt8, btn);
      }
    }
  }
}

// =====================================================
// Test: RTC DS3231 via I2C1 (mismo bus que la OLED, addr 0x68)
// =====================================================
RTC_DS3231 rtc;
static bool rtcOk = false;

static void setupRTC() {
  dbg.println("--- Test RTC DS3231 ---");
  if (!rtc.begin(&Wire)) {
    setLastError("RTC: no detectado (0x68)");
    return;
  }
  rtcOk = true;

  if (rtc.lostPower()) {
    dbg.println("AVISO: el DS3231 perdio la hora (bateria de respaldo?)");
  }
}

// =====================================================
// Test: BME280 via I2C1 (mismo bus que OLED y RTC, addr 0x76/0x77)
// =====================================================
Adafruit_BME280 bme;
static bool bmeOk = false;

// Ultima lectura valida (se conserva si el BME280 no se actualiza).
static float bmeLastTempC = 0.0f;
static float bmeLastHumPct = 0.0f;
static float bmeLastPresHpa = 0.0f;

static void setupBME280() {
  dbg.println("--- Test BME280 ---");
  if (!bme.begin(0x76, &Wire) && !bme.begin(0x77, &Wire)) {
    setLastError("BME280: no detectado");
    return;
  }
  bmeOk = true;
}

static void loopBME280Update() {
  static unsigned long tRead = 0;

  if (!bmeOk) {
    return;
  }

  if (millis() - tRead >= 1000) {
    tRead = millis();

    bmeLastTempC = bme.readTemperature();
    bmeLastHumPct = bme.readHumidity();
    bmeLastPresHpa = bme.readPressure() / 100.0f;
  }
}

// =====================================================
// Test: Nivel de bateria 18650 (2S) via divisor de voltaje
// GPIO35 (ADC1_CH7, pin de solo entrada) -> nodo del divisor
// Divisor: R1=220k (Vbat -> nodo), R2=100k (nodo -> GND) (valores E12)
// Razon 3.2:1 => Vbat = 3.2 * Vpin (a 8.4V full charge, Vpin=2.625V,
// seguro para el ADC del ESP32; con el divisor simetrico anterior,
// pensado para 1S, hubiera llegado a 4.2V y daniado el pin)
// =====================================================
#define BATTERY_ADC_PIN 35
#define BATTERY_DIVIDER_RATIO 3.2f

struct BatteryPoint {
  float voltage;
  uint8_t percent;
};

// Curva de descarga tipica de un pack LiIon 18650 2S (no lineal: la
// mayor parte de la carga esta en el "plateau" 7.4-7.8V). Son los
// puntos de una celda 1S x2, asumiendo celdas parejas/balanceadas.
static const BatteryPoint BATTERY_CURVE[] = {
  {8.40f, 100}, {8.30f, 95}, {8.22f, 90}, {8.16f, 85}, {8.04f, 80},
  {7.96f, 75},  {7.90f, 70}, {7.82f, 65}, {7.74f, 60}, {7.70f, 55},
  {7.68f, 50},  {7.64f, 45}, {7.60f, 40}, {7.58f, 35}, {7.54f, 30},
  {7.50f, 25},  {7.46f, 20}, {7.42f, 15}, {7.38f, 10}, {7.22f, 5},
  {6.54f, 0},
};
#define BATTERY_CURVE_LEN (sizeof(BATTERY_CURVE) / sizeof(BATTERY_CURVE[0]))

static uint8_t batteryVoltageToPercent(float v) {
  if (v >= BATTERY_CURVE[0].voltage) {
    return 100;
  }
  if (v <= BATTERY_CURVE[BATTERY_CURVE_LEN - 1].voltage) {
    return 0;
  }

  for (uint8_t i = 0; i < BATTERY_CURVE_LEN - 1; i++) {
    if (v <= BATTERY_CURVE[i].voltage && v >= BATTERY_CURVE[i + 1].voltage) {
      float ratio = (v - BATTERY_CURVE[i + 1].voltage) /
                    (BATTERY_CURVE[i].voltage - BATTERY_CURVE[i + 1].voltage);
      return BATTERY_CURVE[i + 1].percent +
             (uint8_t)(ratio * (BATTERY_CURVE[i].percent - BATTERY_CURVE[i + 1].percent));
    }
  }
  return 0;
}

static float battLastVoltage = 0.0f;
static uint8_t battLastPercent = 0;

static void loopBatteryUpdate() {
  static unsigned long tRead = 0;

  if (millis() - tRead >= 1000) {
    tRead = millis();

    uint32_t pinMilliVolts = analogReadMilliVolts(BATTERY_ADC_PIN);
    battLastVoltage = (pinMilliVolts / 1000.0f) * BATTERY_DIVIDER_RATIO;
    battLastPercent = batteryVoltageToPercent(battLastVoltage);
  }
}

// =====================================================
// Test: Bomba de vacio PWM (motor DC via N-FET, low-side)
// GPIO26 -> Gate del N-FET
// Motor: + -> +12V, - -> Drain del N-FET (Source a GND)
// El duty cycle objetivo viene directo de la posicion del encoder
// (Pos8, 0-255), pero se aplica con una rampa: un salto brusco (ej.
// 0 a 255) provoca un pico de corriente en el motor que hunde el
// riel de alimentacion y resetea el ESP32 (brownout). La rampa limita
// cuanto puede cambiar el duty real por ciclo de sondeo (200ms).
// =====================================================
#define PUMP_PWM_PIN 26
#define PUMP_PWM_CHANNEL 0
#define PUMP_PWM_FREQ_HZ 20000
#define PUMP_PWM_RES_BITS 8
#define PUMP_RAMP_STEP 12  // maximo cambio de duty por ciclo (~200ms)
#define PUMP_MAX_DUTY 255   // tope de seguridad ~85%

static uint8_t pumpCurrentDuty = 0;

static void setupPump() {
  ledcSetup(PUMP_PWM_CHANNEL, PUMP_PWM_FREQ_HZ, PUMP_PWM_RES_BITS);
  ledcAttachPin(PUMP_PWM_PIN, PUMP_PWM_CHANNEL);
  ledcWrite(PUMP_PWM_CHANNEL, 0);
}

static void setPumpDuty(uint8_t targetDuty) {
  if (targetDuty > PUMP_MAX_DUTY) {
    targetDuty = PUMP_MAX_DUTY;
  }

  if (pumpCurrentDuty < targetDuty) {
    uint8_t delta = targetDuty - pumpCurrentDuty;
    pumpCurrentDuty += (delta < PUMP_RAMP_STEP) ? delta : PUMP_RAMP_STEP;
  } else if (pumpCurrentDuty > targetDuty) {
    uint8_t delta = pumpCurrentDuty - targetDuty;
    pumpCurrentDuty -= (delta < PUMP_RAMP_STEP) ? delta : PUMP_RAMP_STEP;
  }
  ledcWrite(PUMP_PWM_CHANNEL, pumpCurrentDuty);
}

// =====================================================
// Test: Flujometro AFM07 (gas mass flowmeter) via RS485/Modbus RTU
// Transceptor MAX3485 (half-duplex):
//   DI (TX)     -> GPIO4
//   RO (RX)     -> GPIO17
//   DE+RE (dir) -> GPIO16
// UART2 del ESP32. Direccion Modbus por defecto del AFM07: 0x01,
// baudrate por defecto: 9600. Registro 0x0000 = caudal instantaneo,
// funcion 03 (read holding registers), valor x10 (ej. 218 = 21.8 L/min).
// =====================================================
#define RS485_DI_PIN 4
#define RS485_RO_PIN 17
#define RS485_DE_RE_PIN 16

#define AFM07_SLAVE_ADDR 2
#define AFM07_BAUD 4800
#define AFM07_FLOW_REG 0x0000

ModbusMaster afmNode;

static void afmPreTransmission() {
  digitalWrite(RS485_DE_RE_PIN, HIGH);
}

static void afmPostTransmission() {
  digitalWrite(RS485_DE_RE_PIN, LOW);
}

static void setupAFM07() {
  dbg.println("--- Test Flujometro AFM07 (RS485) ---");

  pinMode(RS485_DE_RE_PIN, OUTPUT);
  digitalWrite(RS485_DE_RE_PIN, LOW);

  Serial2.begin(AFM07_BAUD, SERIAL_8N1, RS485_RO_PIN, RS485_DI_PIN);

  afmNode.begin(AFM07_SLAVE_ADDR, Serial2);
  afmNode.preTransmission(afmPreTransmission);
  afmNode.postTransmission(afmPostTransmission);
}

static float afmLastFlowLmin = 0.0f;

static void loopAFM07Poll() {
  static unsigned long tPoll = 0;

  if (millis() - tPoll >= 500) {
    tPoll = millis();

    uint8_t result = afmNode.readHoldingRegisters(AFM07_FLOW_REG, 1);
    if (result == afmNode.ku8MBSuccess) {
      afmLastFlowLmin = afmNode.getResponseBuffer(0) / 100.0f;
    } else {
      setLastError("AFM07: sin respuesta");
    }
  }
}

// =====================================================
// Tabla consolidada por Serial: una fila por ciclo con el ultimo
// valor conocido de cada modulo. Si un sensor no se actualizo en este
// ciclo, se repite su ultima lectura valida (no queda en blanco).
// =====================================================
#define STATUS_TABLE_PERIOD_MS 500
#define STATUS_TABLE_HEADER_EVERY 20  // reimprime el encabezado cada N filas

static void printStatusTableHeader() {
  dbg.println("   Hora  | Pos | Dir | Btn | Bomba | Flujo  |  Temp |  Hum  |  Presion | Bateria");
  dbg.println("---------+-----+-----+-----+-------+--------+-------+-------+----------+---------");
}

static void loopStatusTable() {
  static unsigned long tPrint = 0;
  static uint8_t rowCount = 0;

  if (millis() - tPrint < STATUS_TABLE_PERIOD_MS) {
    return;
  }
  tPrint = millis();

  if (rowCount % STATUS_TABLE_HEADER_EVERY == 0) {
    printStatusTableHeader();
  }
  rowCount++;

  char hora[10] = " --:--:--";
  if (rtcOk) {
    DateTime now = rtc.now();
    snprintf(hora, sizeof(hora), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  }

  char temp[8] = "  ---";
  char hum[8] = "  ---";
  char pres[10] = "   ---";
  if (bmeOk) {
    snprintf(temp, sizeof(temp), "%5.1f", bmeLastTempC);
    snprintf(hum, sizeof(hum), "%5.1f", bmeLastHumPct);
    snprintf(pres, sizeof(pres), "%7.1f", bmeLastPresHpa);
  }

  dbg.printf(" %8s | %3u | %3s | %3s | %4u%% | %6.1f | %s | %s | %s | %4.2fV %3u%%\n",
                hora, encLastPos, encLastDir ? "CCW" : "CW", encLastBtn ? "SI" : "-",
                (pumpCurrentDuty * 100) / 255, afmLastFlowLmin, temp, hum, pres,
                battLastVoltage, battLastPercent);
}

// =====================================================
// I2C Scanner (uno por bus)
// =====================================================
static void scanI2CBus(TwoWire &bus, const char *label) {
  dbg.printf("--- Escaneo %s ---\n", label);
  uint8_t found = 0;

  for (uint8_t addr = 1; addr < 127; addr++) {
    bus.beginTransmission(addr);
    uint8_t error = bus.endTransmission();

    if (error == 0) {
      dbg.printf("  Dispositivo en 0x%02X\n", addr);
      found++;
    }
  }

  if (found == 0) {
    dbg.println("  Ningun dispositivo encontrado");
  } else {
    dbg.printf("  Total: %u dispositivo(s)\n", found);
  }
}

// =====================================================
// Test: OLED 1.3" I2C1 (SH1106 128x64, I2C por hardware)
// Bus I2C1 (Wire): SDA=GPIO21, SCL=GPIO22
// Nota: con el driver SSD1306 se veia un borde blanco a la derecha y
// texto cortado a la izquierda -> desfase tipico de SH1106 (RAM de 132
// columnas, panel de 128) manejado con el driver equivocado.
// =====================================================
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

static void setupOLED() {
  // u8g2 maneja su propio reloj I2C y por defecto usa 100kHz al hacer
  // begin(), pisando el que le pasamos a Wire.begin() si no se lo
  // indicamos explicitamente antes.
  u8g2.setBusClock(I2C1_FREQ_HZ);
  u8g2.begin();
  u8g2.setContrast(255);
  u8g2.clearBuffer();
  u8g2.sendBuffer();
}

static void showSplashScreen() {
  const char *text = "Eolo dron";

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB10_tr);
  int x = (128 - u8g2.getStrWidth(text)) / 2;
  u8g2.drawStr(x, 38, text);
  u8g2.sendBuffer();
}

static void updateOLEDEncoderRetry(uint8_t attempt, uint8_t maxAttempts) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 9, "EoloDrone");
  u8g2.drawHLine(0, 12, 128);

  u8g2.drawStr(0, 30, "Buscando encoder...");

  char line[24];
  snprintf(line, sizeof(line), "Intento: %u/%u", attempt, maxAttempts);
  u8g2.drawStr(0, 44, line);

  u8g2.sendBuffer();
}

#define BOOT_TOTAL_STEPS 8

// Barra de progreso durante las verificaciones iniciales de setup().
// "step" es el chequeo que esta por correr (1-based), "label" lo que
// se esta probando en ese momento.
static void updateOLEDBootProgress(uint8_t step, const char *label) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 9, "EoloDrone");
  u8g2.drawHLine(0, 12, 128);

  u8g2.drawStr(0, 28, label);

  int barWidth = (step * 128) / BOOT_TOTAL_STEPS;
  u8g2.drawFrame(0, 38, 128, 10);
  u8g2.drawBox(0, 38, barWidth, 10);

  char line[12];
  snprintf(line, sizeof(line), "%u/%u", step, BOOT_TOTAL_STEPS);
  int x = 128 - u8g2.getStrWidth(line);
  u8g2.drawStr(x, 60, line);

  u8g2.sendBuffer();
}

static void updateOLEDEncoder(uint8_t dir, uint8_t cnt8, uint8_t btn) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);

  u8g2.drawStr(0, 9, "EoloDrone");

  u8g2.drawHLine(0, 12, 128);

  char line[24];

  if (rtcOk) {
    DateTime now = rtc.now();
    snprintf(line, sizeof(line), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
    int x = 128 - u8g2.getStrWidth(line);
    u8g2.drawStr(x, 9, line);
  }

  snprintf(line, sizeof(line), "P:%u D:%s%s", cnt8, dir ? "CCW" : "CW", btn ? " *B*" : "");
  u8g2.drawStr(0, 19, line);

  snprintf(line, sizeof(line), "B:%u%% F:%.1fL/m",
           (pumpCurrentDuty * 100) / 255, afmLastFlowLmin);
  u8g2.drawStr(0, 29, line);

  if (bmeOk) {
    snprintf(line, sizeof(line), "%.1f|%.1f|%.1f", bme.readTemperature(),
             bme.readHumidity(), bme.readPressure() / 100.0f);
    u8g2.drawStr(0, 39, line);
  }

  snprintf(line, sizeof(line), "Bat:%.2fV %u%%", battLastVoltage, battLastPercent);
  u8g2.drawStr(0, 49, line);

  if (lastErrorMsg[0] != '\0') {
    snprintf(line, sizeof(line), "!%s", lastErrorMsg);
    u8g2.drawStr(0, 59, line);
  }

  u8g2.sendBuffer();
}

static void updateOLEDSettings(uint8_t contrast) {
  u8g2.setContrast(contrast);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 9, "Settings");
  u8g2.drawHLine(0, 12, 128);

  char line[24];
  snprintf(line, sizeof(line), "Contraste: %u", contrast);
  u8g2.drawStr(0, 28, line);

  int barWidth = (contrast * 128) / 255;
  u8g2.drawFrame(0, 38, 128, 10);
  u8g2.drawBox(0, 38, barWidth, 10);

  u8g2.drawStr(0, 60, "Boton: volver");

  u8g2.sendBuffer();
}

// =====================================================
// Setup / Loop
// =====================================================
void setup() {
  Serial.begin(115200);
  SerialBT.begin("EoloDrone");
  dbg.println("\n=== EoloDrone - Demo perifericos ===");

  setupI2CBuses();

  // Splash screen, lo mas temprano posible en el arranque
  setupOLED();
  showSplashScreen();
  delay(2000);

  updateOLEDBootProgress(1, "Encoder...");
  setupEncoderInit();

  updateOLEDBootProgress(2, "Tarjeta SD...");
  setupSD();

  updateOLEDBootProgress(3, "Escaneo I2C1...");
  scanI2CBus(Wire, "I2C1");

  updateOLEDBootProgress(4, "Escaneo I2C2...");
  scanI2CBus(Wire1, "I2C2");

  updateOLEDBootProgress(5, "RTC DS3231...");
  setupRTC();

  updateOLEDBootProgress(6, "BME280...");
  setupBME280();

  updateOLEDBootProgress(7, "Bomba PWM...");
  setupPump();

  updateOLEDBootProgress(8, "AFM07 (RS485)...");
  setupAFM07();
}

void loop() {
  loopBME280Update();
  loopBatteryUpdate();
  loopEncoderPoll();
  loopAFM07Poll();
  loopStatusTable();
}
