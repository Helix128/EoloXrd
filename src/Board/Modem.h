#ifndef MODEM_H
#define MODEM_H

#include <Arduino.h>
#include <RTClib.h>
#include "../Config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define ModemIO Serial1

class Modem
{
public:
  const int powerPin = MODEM_PWR_PIN;
  const int MODEM_RX = 16;
  const int MODEM_TX = 17;
  static constexpr const char *DefaultApn = "gigsky-02";
  const char *apn = DefaultApn;

  Modem() {}

  bool begin()
  {
    ScopedLock lock(*this);
    if (!lock.locked()) return false;
    return beginLocked();
  }

  void end()
  {
    ScopedLock lock(*this);
    if (!lock.locked()) return;
    endLocked();
  }

  bool connect()
  {
    return ensureConnected();
  }

  bool connect(const char *apnOverride)
  {
    return ensureConnected(apnOverride);
  }

  bool ensureConnected()
  {
    return ensureConnected(apn);
  }

  bool ensureConnected(const char *apnOverride)
  {
    ScopedLock lock(*this);
    if (!lock.locked()) return false;
    return ensureConnectedLocked(apnOverride);
  }

  bool isConnected()
  {
    ScopedLock lock(*this);
    if (!lock.locked()) return false;
    return beginLocked() && isConnectedLocked();
  }

  bool openNetwork()
  {
    return openNetwork(apn);
  }

  bool openNetwork(const char *apnOverride)
  {
    ScopedLock lock(*this);
    if (!lock.locked()) return false;
    return beginLocked() && openNetworkLocked(apnOverride);
  }

  bool isSerialStarted()
  {
    ScopedLock lock(*this, pdMS_TO_TICKS(50));
    return lock.locked() && serialStarted;
  }

  bool isPowered()
  {
    ScopedLock lock(*this, pdMS_TO_TICKS(50));
    return lock.locked() && powered;
  }

  const char *lastErrorText()
  {
    return lastError;
  }

  bool rawAT(const char *command, String &response, unsigned long timeout = 5000)
  {
    response = "";
    if (command == nullptr || command[0] == '\0') {
      setLastError("comando AT vacío");
      return false;
    }

    ScopedLock lock(*this);
    if (!lock.locked()) return false;
    if (!beginLocked()) return false;

    response = sendATResponseUntilFinalLocked(command, timeout, MaxRawResponseLength);
    return response.length() > 0 && response.indexOf("ERROR") == -1;
  }

  bool scanOperators(String &response, unsigned long timeout = 180000)
  {
    response = "";
    ScopedLock lock(*this);
    if (!lock.locked()) return false;
    if (!beginLocked()) return false;

    if (!sendATLocked("AT", "OK", 2000)) return false;
    response = sendATResponseUntilFinalLocked("AT+COPS=?", timeout, MaxScanResponseLength);
    return response.indexOf("+COPS:") != -1 && response.indexOf("ERROR") == -1;
  }

  bool selectOperatorAuto()
  {
    ScopedLock lock(*this);
    if (!lock.locked()) return false;
    return beginLocked() && sendATLocked("AT+COPS=0", "OK", 120000);
  }

  bool selectOperatorNumeric(const char *numericCode)
  {
    if (numericCode == nullptr || numericCode[0] == '\0') {
      setLastError("operador vacío");
      return false;
    }

    ScopedLock lock(*this);
    if (!lock.locked()) return false;
    if (!beginLocked()) return false;

    char cmd[40];
    int written = snprintf(cmd, sizeof(cmd), "AT+COPS=1,2,\"%s\"", numericCode);
    if (written < 0 || written >= (int)sizeof(cmd)) {
      setLastError("código de operador demasiado largo");
      return false;
    }
    return sendATLocked(cmd, "OK", 120000);
  }

  void printDiagnostics(Print &out)
  {
    ScopedLock lock(*this);
    if (!lock.locked()) {
      out.println("Modem ocupado");
      return;
    }

    out.printf("power=%s serial=%s at=%s last_error=%s\n",
               powered ? "on" : "off",
               serialStarted ? "on" : "off",
               atReady ? "ready" : "unknown",
               lastError);

    if (!beginLocked()) {
      out.println("Modem apagado/no inicializado. Usa: m begin");
      return;
    }

    printATLocked(out, "AT");
    printATLocked(out, "ATI");
    printATLocked(out, "AT+CPIN?");
    printATLocked(out, "AT+CSQ");
    printATLocked(out, "AT+COPS?");
    printATLocked(out, "AT+CREG?");
    printATLocked(out, "AT+CGREG?");
    printATLocked(out, "AT+CEREG?");
    printATLocked(out, "AT+CGACT?");
    printATLocked(out, "AT+NETOPEN?");
    printATLocked(out, "AT+IPADDR");
  }

  bool get(const char *url, char *respBuffer, int bufferSize)
  {
    ScopedLock lock(*this);
    if (!lock.locked()) return false;
    if (!ensureConnectedLocked(apn)) return false;
    return httpRequestLocked(0, url, nullptr, respBuffer, bufferSize);
  }

  bool post(const char *url, const char *payload, char *respBuffer, int bufferSize)
  {
    ScopedLock lock(*this);
    if (!lock.locked()) return false;
    if (!ensureConnectedLocked(apn)) return false;
    return httpRequestLocked(1, url, payload, respBuffer, bufferSize);
  }

  bool sendAT(const char *command, const char *expected, unsigned long timeout)
  {
    ScopedLock lock(*this);
    if (!lock.locked()) return false;
    return beginLocked() && sendATLocked(command, expected, timeout);
  }

  String sendATResponse(const char *command, unsigned long timeout)
  {
    ScopedLock lock(*this);
    if (!lock.locked()) return "";
    if (!beginLocked()) return "";
    return sendATResponseLocked(command, timeout, MaxResponseLength);
  }

  String sendATResponseUntilFinal(const char *command, unsigned long timeout)
  {
    ScopedLock lock(*this);
    if (!lock.locked()) return "";
    if (!beginLocked()) return "";
    return sendATResponseUntilFinalLocked(command, timeout, MaxResponseLength);
  }

  bool getNetworkTimeUTC(const char *serverHost, DateTime &utcTime)
  {
    if (serverHost == nullptr || serverHost[0] == '\0') {
      setLastError("servidor NTP vacío");
      return false;
    }

    ScopedLock lock(*this);
    if (!lock.locked()) return false;
    if (!ensureConnectedLocked(apn)) return false;

    char cmd[96];
    int written = snprintf(cmd, sizeof(cmd), "AT+CNTP=\"%s\",0", serverHost);
    if (written < 0 || written >= (int)sizeof(cmd)) {
      setLastError("host NTP demasiado largo");
      return false;
    }

    if (!sendATLocked(cmd, "OK", 5000)) return false;

    if (!runNtpSyncLocked())
    {
      LOG_LN("Servidor NTP no respondio correctamente");
      return false;
    }

    String clockResponse = sendATResponseLocked("AT+CCLK?", 5000, MaxResponseLength);
    if (!parseClockResponse(clockResponse, utcTime)) {
      setLastError("respuesta CCLK inválida");
      return false;
    }
    return true;
  }

private:
  static const size_t MaxResponseLength = 1024;
  static const size_t MaxRawResponseLength = 2048;
  static const size_t MaxScanResponseLength = 4096;
  static const size_t MaxLineLength = 256;
  static const size_t WaitBufferLength = 240;

  bool serialStarted = false;
  bool powered = false;
  bool atReady = false;
  SemaphoreHandle_t mutex = nullptr;
  char lastError[96] = "OK";

  class ScopedLock {
  public:
    ScopedLock(Modem &modem, TickType_t waitTicks = portMAX_DELAY) : _modem(modem) {
      _locked = _modem.lock(waitTicks);
    }

    ~ScopedLock() {
      if (_locked) _modem.unlock();
    }

    bool locked() const {
      return _locked;
    }

  private:
    Modem &_modem;
    bool _locked = false;
  };

  bool lock(TickType_t waitTicks) {
    if (mutex == nullptr) {
      mutex = xSemaphoreCreateMutex();
      if (mutex == nullptr) {
        setLastError("sin memoria para mutex");
        return false;
      }
    }

    if (xSemaphoreTake(mutex, waitTicks) != pdTRUE) {
      setLastError("modem ocupado");
      return false;
    }
    return true;
  }

  void unlock() {
    if (mutex != nullptr) {
      xSemaphoreGive(mutex);
    }
  }

  void setLastError(const char *message) {
    if (message == nullptr) message = "error desconocido";
    strncpy(lastError, message, sizeof(lastError) - 1);
    lastError[sizeof(lastError) - 1] = '\0';
  }

  void clearLastError() {
    setLastError("OK");
  }

  bool beginLocked()
  {
    if (serialStarted)
    {
      if (atReady) return true;
      atReady = waitForATLocked(2000);
      return atReady;
    }

    pinMode(powerPin, OUTPUT);
    digitalWrite(powerPin, HIGH);
    powered = true;
    vTaskDelay(pdMS_TO_TICKS(5000));

    // Los nombres MODEM_RX/MODEM_TX vienen desde la perspectiva del modem.
    // HardwareSerial espera pines desde la perspectiva del ESP32: rxPin, txPin.
    ModemIO.begin(115200, SERIAL_8N1, MODEM_TX, MODEM_RX);
    serialStarted = true;
    clearRxLocked();
    LOG_F("UART modem iniciado ESP32_RX=%d ESP32_TX=%d\n", MODEM_TX, MODEM_RX);

    if (!waitForATLocked(10000))
    {
      LOG_LN("Modem no responde a AT");
      atReady = false;
      return false;
    }

    if (!sendATLocked("ATE0", "OK", 1000))
    {
      LOG_LN("No se pudo desactivar eco del modem");
      atReady = false;
      return false;
    }

    atReady = true;
    clearLastError();
    return true;
  }

  void endLocked()
  {
    if (serialStarted)
    {
      if (atReady)
      {
        sendATLocked("AT+NETCLOSE", "OK", 1000);
      }
      ModemIO.end();
      serialStarted = false;
      atReady = false;
    }

    if (powered)
    {
      digitalWrite(powerPin, LOW);
      powered = false;
    }
    clearLastError();
  }

  bool ensureConnectedLocked(const char *apnOverride)
  {
    if (!beginLocked()) return false;
    if (!sendATLocked("AT", "OK", 2000)) return false;
    if (!sendATLocked("AT+CPIN?", "READY", 5000)) return false;

    if (isConnectedLocked()) return true;

    if (openNetworkLocked(apnOverride) && isConnectedLocked()) return true;

    LOG_LN("Modem sin IP valida; reintentando conexion PDP");
    sendATLocked("AT+NETCLOSE", "OK", 5000);
    vTaskDelay(pdMS_TO_TICKS(1000));
    return openNetworkLocked(apnOverride) && isConnectedLocked();
  }

  bool isConnectedLocked()
  {
    if (!sendATLocked("AT", "OK", 2000)) return false;

    String netState = sendATResponseLocked("AT+NETOPEN?", 5000, MaxResponseLength);
    if (netState.indexOf("+NETOPEN: 1") == -1) return false;

    String ip = sendATResponseLocked("AT+IPADDR", 5000, MaxResponseLength);
    return hasValidIp(ip);
  }

  bool openNetworkLocked(const char *apnOverride)
  {
    if (apnOverride == nullptr || apnOverride[0] == '\0') apnOverride = apn;

    char cmd[96];
    int written = snprintf(cmd, sizeof(cmd), "AT+CGDCONT=1,\"IP\",\"%s\"", apnOverride);
    if (written < 0 || written >= (int)sizeof(cmd)) {
      setLastError("APN demasiado largo");
      return false;
    }

    if (!sendATLocked(cmd, "OK", 5000)) return false;
    if (!sendATLocked("AT+CGACT=1,1", "OK", 10000)) return false;

    if (!sendATLocked("AT+NETOPEN", "0", 20000))
    {
      if (!sendATLocked("AT+NETOPEN?", "1", 5000)) return false;
    }

    String ip = sendATResponseLocked("AT+IPADDR", 5000, MaxResponseLength);
    if (!hasValidIp(ip))
    {
      setLastError("IP inválida");
      LOG_LN("Modem no obtuvo direccion IP");
      return false;
    }

    clearLastError();
    return true;
  }

  bool httpRequestLocked(uint8_t method, const char *url, const char *payload, char *respBuffer, int bufferSize)
  {
    if (url == nullptr || url[0] == '\0' || respBuffer == nullptr || bufferSize <= 1) {
      setLastError("parámetros HTTP inválidos");
      return false;
    }

    respBuffer[0] = '\0';
    bool httpStarted = false;
    bool success = false;
    int written = 0;
    int dataLen = 0;

    if (!sendATLocked("AT+HTTPINIT", "OK", 2000)) goto cleanup;
    httpStarted = true;

    char cmd[512];
    written = snprintf(cmd, sizeof(cmd), "AT+HTTPPARA=\"URL\",\"%s\"", url);
    if (written < 0 || written >= (int)sizeof(cmd)) {
      setLastError("URL demasiado larga");
      goto cleanup;
    }

    if (!sendATLocked(cmd, "OK", 2000)) goto cleanup;

    if (method == 1) {
      if (payload == nullptr) payload = "";
      if (!sendATLocked("AT+HTTPPARA=\"CONTENT\",\"application/x-www-form-urlencoded\"", "OK", 2000)) goto cleanup;

      size_t payloadLen = strlen(payload);
      written = snprintf(cmd, sizeof(cmd), "AT+HTTPDATA=%u,10000", (unsigned int)payloadLen);
      if (written < 0 || written >= (int)sizeof(cmd)) {
        setLastError("payload HTTP demasiado largo");
        goto cleanup;
      }

      if (!sendATLocked(cmd, "DOWNLOAD", 5000)) goto cleanup;
      ModemIO.print(payload);
      if (!waitForLocked("OK", 5000)) goto cleanup;
    }

    written = snprintf(cmd, sizeof(cmd), "AT+HTTPACTION=%u", method);
    if (written < 0 || written >= (int)sizeof(cmd)) goto cleanup;
    if (!sendATLocked(cmd, "OK", method == 1 ? 5000 : 10000)) goto cleanup;

    dataLen = waitForHttpActionLocked(method, 15000);
    if (dataLen < 0) goto cleanup;

    if (dataLen > 0) {
      if (!readHttpResponseLocked(dataLen, respBuffer, bufferSize, method == 1 ? 5000 : 10000)) goto cleanup;
    }

    success = true;

cleanup:
    if (httpStarted) {
      sendATLocked("AT+HTTPTERM", "OK", 1000);
    }
    return success;
  }

  int waitForHttpActionLocked(uint8_t method, unsigned long timeout)
  {
    unsigned long start = millis();
    char expectedPrefix[24];
    snprintf(expectedPrefix, sizeof(expectedPrefix), "+HTTPACTION: %u,", method);

    while (millis() - start < timeout)
    {
      String line = readLineLocked(MaxLineLength);
      if (line.length() == 0) {
        vTaskDelay(pdMS_TO_TICKS(10));
        continue;
      }

      int prefixIndex = line.indexOf(expectedPrefix);
      if (prefixIndex == -1) continue;

      int statusStart = prefixIndex + strlen(expectedPrefix);
      int comma = line.indexOf(',', statusStart);
      if (comma == -1) {
        setLastError("HTTPACTION inválido");
        return -1;
      }

      int status = line.substring(statusStart, comma).toInt();
      int dataLen = line.substring(comma + 1).toInt();
      if (status != 200) {
        setLastError("HTTP status no exitoso");
        return -1;
      }
      return dataLen >= 0 ? dataLen : -1;
    }

    setLastError("timeout HTTPACTION");
    return -1;
  }

  bool readHttpResponseLocked(int dataLen, char *respBuffer, int bufferSize, unsigned long timeout)
  {
    char cmd[32];
    int written = snprintf(cmd, sizeof(cmd), "AT+HTTPREAD=%d", dataLen);
    if (written < 0 || written >= (int)sizeof(cmd)) return false;

    clearRxLocked();
    ModemIO.println(cmd);

    unsigned long start = millis();
    bool headerSeen = false;
    bool truncated = false;
    int idx = 0;

    while (millis() - start < timeout && !headerSeen)
    {
      String line = readLineLocked(MaxLineLength);
      if (line.indexOf("+HTTPREAD") != -1) {
        headerSeen = true;
        break;
      }
      if (line.indexOf("ERROR") != -1) {
        setLastError("HTTPREAD error");
        return false;
      }
    }

    if (!headerSeen) {
      setLastError("timeout HTTPREAD");
      return false;
    }

    while (millis() - start < timeout && idx < dataLen)
    {
      if (ModemIO.available())
      {
        char c = (char)ModemIO.read();
        if (idx < bufferSize - 1) {
          respBuffer[idx++] = c;
        } else {
          truncated = true;
          idx++;
        }
      }
      else
      {
        vTaskDelay(pdMS_TO_TICKS(5));
      }
    }

    int stored = idx < bufferSize ? idx : bufferSize - 1;
    respBuffer[stored] = '\0';
    if (!waitForLocked("OK", 2000)) return false;

    if (idx < dataLen) {
      setLastError("respuesta HTTP incompleta");
      return false;
    }
    if (truncated) {
      setLastError("respuesta HTTP truncada");
      return false;
    }
    return true;
  }

  bool sendATLocked(const char *command, const char *expected, unsigned long timeout)
  {
    if (command == nullptr || expected == nullptr || command[0] == '\0') {
      setLastError("comando AT inválido");
      return false;
    }
    if (!serialStarted) {
      setLastError("UART modem no iniciada");
      return false;
    }

    clearRxLocked();
    ModemIO.println(command);
    bool ok = waitForLocked(expected, timeout);
    if (ok) clearLastError();
    return ok;
  }

  String sendATResponseLocked(const char *command, unsigned long timeout, size_t maxLen)
  {
    if (command == nullptr || command[0] == '\0' || !serialStarted) return "";
    clearRxLocked();
    ModemIO.println(command);

    String response = "";
    response.reserve(maxLen < 256 ? maxLen : 256);
    unsigned long start = millis();
    while (millis() - start < timeout)
    {
      if (ModemIO.available())
      {
        char c = (char)ModemIO.read();
        appendLimited(response, c, maxLen);
      }
      else
      {
        vTaskDelay(pdMS_TO_TICKS(10));
      }
    }
    return response;
  }

  String sendATResponseUntilFinalLocked(const char *command, unsigned long timeout, size_t maxLen)
  {
    if (command == nullptr || command[0] == '\0' || !serialStarted) return "";
    clearRxLocked();
    ModemIO.println(command);

    String response = "";
    response.reserve(maxLen < 256 ? maxLen : 256);
    unsigned long start = millis();
    while (millis() - start < timeout)
    {
      if (ModemIO.available())
      {
        char c = (char)ModemIO.read();
        appendLimited(response, c, maxLen);

        if (response.indexOf("\r\nOK\r\n") != -1 ||
            response.indexOf("\nOK\r") != -1 ||
            response.indexOf("\nOK\n") != -1 ||
            response.indexOf("ERROR") != -1)
        {
          break;
        }
      }
      else
      {
        vTaskDelay(pdMS_TO_TICKS(10));
      }
    }
    return response;
  }

  bool waitForATLocked(unsigned long timeout)
  {
    unsigned long start = millis();
    while (millis() - start < timeout)
    {
      if (sendATLocked("AT", "OK", 1000)) return true;
      vTaskDelay(pdMS_TO_TICKS(250));
    }
    setLastError("timeout esperando AT");
    return false;
  }

  bool waitForLocked(const char *expected, unsigned long timeout)
  {
    unsigned long start = millis();
    String buffer = "";
    buffer.reserve(WaitBufferLength);
    
    while (millis() - start < timeout)
    {
      if (ModemIO.available())
      {
        char c = (char)ModemIO.read();
        appendLimited(buffer, c, WaitBufferLength);

        if (buffer.indexOf(expected) != -1) return true;
        if (buffer.indexOf("ERROR") != -1) {
          setLastError("respuesta ERROR del modem");
          return false;
        }
      }
      else
      {
        vTaskDelay(pdMS_TO_TICKS(10));
      }
    }
    setLastError("timeout esperando respuesta AT");
    return false;
  }

  String readLineLocked(size_t maxLen)
  {
    String line = "";
    line.reserve(maxLen < 128 ? maxLen : 128);
    unsigned long start = millis();
    while (millis() - start < 2000)
    {
      if (ModemIO.available())
      {
        char c = (char)ModemIO.read();
        if (c == '\n') return line;
        if (c != '\r') appendLimited(line, c, maxLen);
      }
      else
      {
        vTaskDelay(pdMS_TO_TICKS(5));
      }
    }
    return line;
  }

  bool runNtpSyncLocked()
  {
    clearRxLocked();
    ModemIO.println("AT+CNTP");

    String buffer = "";
    buffer.reserve(300);
    bool sawNtpResult = false;
    unsigned long start = millis();
    while (millis() - start < 65000)
    {
      if (ModemIO.available())
      {
        char c = (char)ModemIO.read();
        appendLimited(buffer, c, 300);
        if (buffer.indexOf("+CNTP: 1,0") != -1 || buffer.indexOf("+CNTP: 1, 0") != -1)
          return true;
        if (buffer.indexOf("+CNTP:") != -1)
          sawNtpResult = true;
        if (sawNtpResult && c == '\n') {
          setLastError("CNTP falló");
          return false;
        }
      }
      else
      {
        vTaskDelay(pdMS_TO_TICKS(20));
      }
    }
    setLastError("timeout CNTP");
    return false;
  }

  void appendLimited(String &buffer, char c, size_t maxLen)
  {
    if (maxLen == 0) return;
    if (buffer.length() >= maxLen) {
      buffer.remove(0, buffer.length() - maxLen + 1);
    }
    buffer += c;
  }

  void clearRxLocked()
  {
    while (ModemIO.available()) ModemIO.read();
  }

  bool parseOctet(const String &text, int start, int end, int &value)
  {
    if (start >= end || end - start > 3) return false;
    value = 0;
    for (int i = start; i < end; ++i) {
      char c = text.charAt(i);
      if (c < '0' || c > '9') return false;
      value = value * 10 + (c - '0');
    }
    return value >= 0 && value <= 255;
  }

  bool hasValidIp(const String &response)
  {
    if (response.indexOf("ERROR") != -1) return false;

    for (int i = 0; i < (int)response.length(); ++i) {
      if (!isDigit(response.charAt(i))) continue;

      int pos = i;
      bool ok = true;
      for (int octet = 0; octet < 4; ++octet) {
        int start = pos;
        while (pos < (int)response.length() && isDigit(response.charAt(pos))) pos++;
        int value = 0;
        if (!parseOctet(response, start, pos, value)) {
          ok = false;
          break;
        }
        if (octet < 3) {
          if (pos >= (int)response.length() || response.charAt(pos) != '.') {
            ok = false;
            break;
          }
          pos++;
        }
      }

      if (ok) {
        char next = pos < (int)response.length() ? response.charAt(pos) : '\0';
        if (!isDigit(next) && next != '.') return true;
      }
    }
    return false;
  }

  bool parseClockResponse(const String &response, DateTime &utcTime)
  {
    int quoteStart = response.indexOf('"');
    int quoteEnd = response.indexOf('"', quoteStart + 1);
    if (quoteStart == -1 || quoteEnd == -1) return false;

    String value = response.substring(quoteStart + 1, quoteEnd);
    if (value.length() < 17) return false;
    if (value.charAt(2) != '/' || value.charAt(5) != '/' ||
        value.charAt(8) != ',' || value.charAt(11) != ':' ||
        value.charAt(14) != ':') {
      return false;
    }

    int year = value.substring(0, 2).toInt() + 2000;
    int month = value.substring(3, 5).toInt();
    int day = value.substring(6, 8).toInt();
    int hour = value.substring(9, 11).toInt();
    int minute = value.substring(12, 14).toInt();
    int second = value.substring(15, 17).toInt();

    if (year < 2024 || year > 2099 || month < 1 || month > 12 || day < 1 || day > 31 ||
        hour > 23 || minute > 59 || second > 59)
    {
      return false;
    }

    utcTime = DateTime(year, month, day, hour, minute, second);
    return utcTime.isValid();
  }

  void printATLocked(Print &out, const char *command)
  {
    out.printf("> %s\n", command);
    String response = sendATResponseUntilFinalLocked(command, 5000, MaxRawResponseLength);
    response.trim();
    if (response.length() == 0) response = "(sin respuesta)";
    out.println(response);
  }
};

#endif
