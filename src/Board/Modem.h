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
  static constexpr uint8_t PowerOnLevel = HIGH;
  static constexpr uint8_t PowerOffLevel = LOW;
  static constexpr uint32_t PowerOnSettleMs = 15000;
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

  static void configurePowerPinOff()
  {
    pinMode(MODEM_PWR_PIN, OUTPUT);
    digitalWrite(MODEM_PWR_PIN, PowerOffLevel);
  }

  static void configurePowerPinOn()
  {
    pinMode(MODEM_PWR_PIN, OUTPUT);
    digitalWrite(MODEM_PWR_PIN, PowerOnLevel);
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

  bool rawAT(const char *command, char *response, size_t responseLen, unsigned long timeout = 5000)
  {
    if (response != nullptr && responseLen > 0) response[0] = '\0';
    if (command == nullptr || command[0] == '\0') {
      setLastError("comando AT vacío");
      return false;
    }

    ScopedLock lock(*this);
    if (!lock.locked()) return false;
    if (!beginLocked()) return false;

    return sendATResponseUntilFinalLocked(command, response, responseLen, timeout);
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
    return executeHttpGet(url, respBuffer, bufferSize);
  }

  bool post(const char *url, const char *payload, char *respBuffer, int bufferSize)
  {
    return executeHttpPost(url, payload, respBuffer, bufferSize);
  }

  bool executeHttpGet(const char *url, char *respBuffer, int bufferSize)
  {
    return executeHttpRequest(0, url, nullptr, respBuffer, bufferSize);
  }

  bool executeHttpPost(const char *url, const char *payload, char *respBuffer, int bufferSize)
  {
    return executeHttpRequest(1, url, payload, respBuffer, bufferSize);
  }

  // Ping a host. elapsedMs = tiempo del primer reply (si hubo).
  bool ping(const char *host, unsigned long *elapsedMs = nullptr)
  {
    if (host == nullptr || host[0] == '\0') {
      setLastError("host ping vacío");
      return false;
    }
    ScopedLock lock(*this);
    if (!lock.locked()) return false;
    if (!beginLocked()) return false;
    return pingLocked(host, elapsedMs);
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
  int lastHttpActionStatus = 0;

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

  bool executeHttpRequest(uint8_t method, const char *url, const char *payload, char *respBuffer, int bufferSize)
  {
    ScopedLock lock(*this);
    if (!lock.locked()) return false;

    return ensureConnectedLocked(apn) &&
           waitForHttpReadyLocked(30000) &&
           httpRequestLocked(method, url, payload, respBuffer, bufferSize);
  }

  bool beginLocked()
  {
    if (serialStarted)
    {
      if (atReady) return true;
      atReady = waitForATLocked(8000);
      return atReady;
    }

    pinMode(powerPin, OUTPUT);
    digitalWrite(powerPin, PowerOnLevel);
    powered = true;
    vTaskDelay(pdMS_TO_TICKS(PowerOnSettleMs));

    // Los nombres MODEM_RX/MODEM_TX vienen desde la perspectiva del modem.
    // HardwareSerial espera pines desde la perspectiva del ESP32: rxPin, txPin.
    ModemIO.begin(115200, SERIAL_8N1, MODEM_TX, MODEM_RX);
    serialStarted = true;
    clearRxLocked();
    LOG_F("UART modem iniciado ESP32_RX=%d ESP32_TX=%d\n", MODEM_TX, MODEM_RX);

    if (!waitForATLocked(20000))
    {
      LOG_LN("Modem no responde a AT");
      atReady = false;
      return false;
    }

    if (!sendATLocked("ATE0", "OK", 10000))
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

    pinMode(powerPin, OUTPUT);
    digitalWrite(powerPin, PowerOffLevel);
    powered = false;
    clearLastError();
  }

  bool ensureConnectedLocked(const char *apnOverride)
  {
    if (!beginLocked()) return false;
    if (!sendATLocked("AT", "OK", 5000)) return false;
    if (!sendATLocked("AT+CPIN?", "READY", 5000)) return false;
    if (!waitForNetworkRegistrationLocked(90000)) return false;

    if (isConnectedLocked()) return true;

    if (openNetworkLocked(apnOverride) && isConnectedLocked()) return true;

    LOG_LN("Modem sin IP valida; reintentando conexion PDP");
    sendATLocked("AT+NETCLOSE", "OK", 5000);
    vTaskDelay(pdMS_TO_TICKS(1000));
    return openNetworkLocked(apnOverride) && isConnectedLocked();
  }

  bool isConnectedLocked()
  {
    if (!sendATLocked("AT", "OK", 5000)) return false;

    String netState = sendATResponseUntilFinalLocked("AT+NETOPEN?", 10000, MaxResponseLength);
    if (netState.indexOf("+NETOPEN: 1") == -1) return false;

    String ip = sendATResponseUntilFinalLocked("AT+IPADDR", 10000, MaxResponseLength);
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

    if (!sendATLocked(cmd, "OK", 10000)) return false;

    // CGACT puede fallar si el contexto ya está activo — tolerar y verificar
    if (!sendATLocked("AT+CGACT=1,1", "OK", 60000)) {
      String cgactState = sendATResponseUntilFinalLocked("AT+CGACT?", 10000, MaxResponseLength);
      if (cgactState.indexOf("+CGACT: 1,1") == -1) {
        setLastError("contexto PDP no activo");
        return false;
      }
      LOG_LN("CGACT ya activo, continuando...");
    }

    // Cerrar TCP stack si ya estaba abierto antes de reabrir
    sendATLocked("AT+NETCLOSE", "OK", 15000);
    vTaskDelay(pdMS_TO_TICKS(1500));

    if (!sendATLocked("AT+NETOPEN", "0", 90000))
    {
      if (!sendATLocked("AT+NETOPEN?", "1", 10000)) return false;
    }

    if (!waitForValidIpLocked(30000))
    {
      setLastError("IP inválida");
      LOG_LN("Modem no obtuvo direccion IP");
      return false;
    }

    clearLastError();
    return true;
  }

  bool waitForHttpReadyLocked(unsigned long timeout)
  {
    unsigned long start = millis();
    while (millis() - start < timeout)
    {
      if (sendATLocked("AT", "OK", 5000) && isConnectedLocked()) {
        vTaskDelay(pdMS_TO_TICKS(500));
        clearRxLocked();
        clearLastError();
        return true;
      }
      vTaskDelay(pdMS_TO_TICKS(1000));
    }

    setLastError("modem no listo para HTTP");
    return false;
  }

  bool waitForNetworkRegistrationLocked(unsigned long timeout)
  {
    unsigned long start = millis();
    while (millis() - start < timeout)
    {
      if (isRegisteredLocked()) {
        clearLastError();
        return true;
      }
      vTaskDelay(pdMS_TO_TICKS(2000));
    }

    setLastError("timeout registro red");
    return false;
  }

  bool isRegisteredLocked()
  {
    String response = sendATResponseUntilFinalLocked("AT+CREG?", 10000, MaxResponseLength);
    if (hasRegisteredStat(response)) return true;

    response = sendATResponseUntilFinalLocked("AT+CGREG?", 10000, MaxResponseLength);
    if (hasRegisteredStat(response)) return true;

    response = sendATResponseUntilFinalLocked("AT+CEREG?", 10000, MaxResponseLength);
    return hasRegisteredStat(response);
  }

  bool hasRegisteredStat(const String &response)
  {
    int idx = response.indexOf(':');
    if (idx == -1 || response.indexOf("ERROR") != -1) return false;

    String value = response.substring(idx + 1);
    value.trim();
    int firstComma = value.indexOf(',');
    int stat = value.toInt();
    if (firstComma != -1) {
      int secondComma = value.indexOf(',', firstComma + 1);
      String statField = secondComma == -1
                           ? value.substring(firstComma + 1)
                           : value.substring(firstComma + 1, secondComma);
      stat = statField.toInt();
    }
    return stat == 1 || stat == 5;
  }

  bool waitForValidIpLocked(unsigned long timeout)
  {
    unsigned long start = millis();
    while (millis() - start < timeout)
    {
      String ip = sendATResponseUntilFinalLocked("AT+IPADDR", 10000, MaxResponseLength);
      if (hasValidIp(ip)) {
        clearLastError();
        return true;
      }
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
    return false;
  }

  bool httpRequestLocked(uint8_t method, const char *url, const char *payload, char *respBuffer, int bufferSize)
  {
    if (url == nullptr || url[0] == '\0' || respBuffer == nullptr || bufferSize <= 1) {
      setLastError("parámetros HTTP inválidos");
      return false;
    }

    char normalizedUrl[512];
    char cmd[512];
    // savedError preserva el error real antes del cleanup, que puede machacarlo con "OK"
    char savedError[sizeof(lastError)];
    memcpy(savedError, lastError, sizeof(savedError));

    respBuffer[0] = '\0';
    bool httpStarted = false;
    bool success = false;
    int written = 0;
    int dataLen = 0;
    bool usingResolvedIp = false;

    if (!normalizeHttpUrl(url, normalizedUrl, sizeof(normalizedUrl))) return false;

    char requestUrl[512];
    char host[128];
    copyCString(requestUrl, sizeof(requestUrl), normalizedUrl);
    bool hasHost = extractHttpHost(normalizedUrl, host, sizeof(host));
    bool isHttps = (strncmp(normalizedUrl, "https://", 8) == 0);

retry_http:
    if (!sendATLocked("AT+HTTPINIT", "OK", 10000)) {
      sendATLocked("AT+HTTPTERM", "OK", 5000);
      vTaskDelay(pdMS_TO_TICKS(1000));
      if (!sendATLocked("AT+HTTPINIT", "OK", 10000)) goto cleanup;
    }
    httpStarted = true;

    sendATOptionalLocked("AT+CIPDNSSET=1,30000,3", "OK", 5000);
    sendATOptionalLocked("AT+HTTPPARA=\"CONNECTTO\",120", "OK", 5000);
    sendATOptionalLocked("AT+HTTPPARA=\"RECVTO\",60", "OK", 5000);
    sendATOptionalLocked("AT+HTTPPARA=\"RESPTO\",60", "OK", 5000);
    sendATOptionalLocked("AT+HTTPPARA=\"ACCEPT\",\"*/*\"", "OK", 5000);
    sendATOptionalLocked("AT+HTTPPARA=\"UA\",\"EoloXrd SIM7600\"", "OK", 5000);

    if (usingResolvedIp && hasHost) {
      written = snprintf(cmd, sizeof(cmd), "AT+HTTPPARA=\"USERDATA\",\"Host: %s\"", host);
      if (written < 0 || written >= (int)sizeof(cmd)) {
        setLastError("Host HTTP demasiado largo");
        goto cleanup;
      }
      if (!sendATLocked(cmd, "OK", 10000)) goto cleanup;
    }

    if (isHttps) {
      sendATOptionalLocked("AT+HTTPPARA=\"SSLCFG\",\"0\"", "OK", 5000);
    }

    written = snprintf(cmd, sizeof(cmd), "AT+HTTPPARA=\"URL\",\"%s\"", requestUrl);
    if (written < 0 || written >= (int)sizeof(cmd)) {
      setLastError("URL demasiado larga");
      goto cleanup;
    }

    if (!sendATLocked(cmd, "OK", 10000)) goto cleanup;

    if (method == 1) {
      if (payload == nullptr) payload = "";
      if (!sendATLocked("AT+HTTPPARA=\"CONTENT\",\"application/x-www-form-urlencoded\"", "OK", 10000)) goto cleanup;

      size_t payloadLen = strlen(payload);
      written = snprintf(cmd, sizeof(cmd), "AT+HTTPDATA=%u,30000", (unsigned int)payloadLen);
      if (written < 0 || written >= (int)sizeof(cmd)) {
        setLastError("payload HTTP demasiado largo");
        goto cleanup;
      }

      if (!sendATLocked(cmd, "DOWNLOAD", 15000)) goto cleanup;
      ModemIO.print(payload);
      if (!waitForLocked("OK", 30000)) goto cleanup;
    }

    written = snprintf(cmd, sizeof(cmd), "AT+HTTPACTION=%u", method);
    if (written < 0 || written >= (int)sizeof(cmd)) {
      setLastError("comando HTTPACTION inválido");
      goto cleanup;
    }
    if (!sendATLocked(cmd, "OK", 30000)) goto cleanup;
    lastHttpActionStatus = 0;

    {
      unsigned long actionTimeout = isHttps ? 120000 : 90000;
      unsigned long readTimeout   = isHttps ? 60000 : 45000;
      dataLen = waitForHttpActionLocked(method, actionTimeout);
      if (dataLen < 0) {
        if (method == 0 && !isHttps && !usingResolvedIp && lastHttpActionStatus == 713 && hasHost) {
          char resolvedIp[48];
          char actionError[sizeof(lastError)];
          copyCString(actionError, sizeof(actionError), lastError);

          if (httpStarted) {
            sendATLocked("AT+HTTPTERM", "OK", 10000);
            httpStarted = false;
          }

          if (resolveHostIPv4Locked(host, resolvedIp, sizeof(resolvedIp)) &&
              buildHttpUrlWithHostIp(normalizedUrl, resolvedIp, requestUrl, sizeof(requestUrl))) {
            usingResolvedIp = true;
            LOG_F("HTTP DNS fallo para %s; reintentando por IP %s con Host header\n", host, resolvedIp);
            goto retry_http;
          }

          setLastError(actionError);
        }
        goto cleanup;
      }
      if (dataLen > 0) {
        if (!readHttpResponseLocked(dataLen, respBuffer, bufferSize, readTimeout)) goto cleanup;
      }
    }

    success = true;

cleanup:
    // Preservar el error real antes de que HTTPTERM lo limpie
    memcpy(savedError, lastError, sizeof(savedError));
    savedError[sizeof(savedError) - 1] = '\0';
    if (httpStarted) {
      sendATLocked("AT+HTTPTERM", "OK", 10000);
    }
    if (!success) setLastError(savedError);
    return success;
  }

  int waitForHttpActionLocked(uint8_t method, unsigned long timeout)
  {
    unsigned long start = millis();
    char expectedPrefix[24];
    snprintf(expectedPrefix, sizeof(expectedPrefix), "+HTTPACTION: %u,", method);
    size_t prefixLen = strlen(expectedPrefix);

    String line = "";
    line.reserve(64);

    while (millis() - start < timeout)
    {
      if (!ModemIO.available())
      {
        vTaskDelay(pdMS_TO_TICKS(10));
        continue;
      }

      char c = (char)ModemIO.read();
      if (c == '\n')
      {
        int prefixIndex = line.indexOf(expectedPrefix);
        if (prefixIndex != -1)
        {
          int statusStart = prefixIndex + (int)prefixLen;
          int comma = line.indexOf(',', statusStart);
          if (comma == -1) {
            setLastError("HTTPACTION inválido");
            return -1;
          }
          int status = line.substring(statusStart, comma).toInt();
          lastHttpActionStatus = status;
          int dataLen = line.substring(comma + 1).toInt();
          if (status < 200 || status >= 300) {
            char errBuf[64];
            snprintf(errBuf, sizeof(errBuf), "HTTPACTION %d %s", status, httpActionStatusText(status));
            setLastError(errBuf);
            return -1;
          }
          return dataLen >= 0 ? dataLen : -1;
        }
        line = "";
      }
      else if (c != '\r')
      {
        appendLimited(line, c, MaxLineLength);
      }
    }

    setLastError("timeout HTTPACTION");
    return -1;
  }

  bool readHttpResponseLocked(int dataLen, char *respBuffer, int bufferSize, unsigned long timeout)
  {
    char cmd[32];
    int written = snprintf(cmd, sizeof(cmd), "AT+HTTPREAD=%d", dataLen);
    if (written < 0 || written >= (int)sizeof(cmd)) {
      setLastError("comando HTTPREAD inválido");
      return false;
    }

    clearRxLocked();
    ModemIO.println(cmd);

    unsigned long start = millis();
    bool headerSeen = false;
    bool truncated = false;
    int idx = 0;

    // Buscar "+HTTPREAD" con streaming, sin quemar 2s por línea
    {
      String hdrLine = "";
      hdrLine.reserve(32);
      while (millis() - start < timeout && !headerSeen)
      {
        if (!ModemIO.available()) { vTaskDelay(pdMS_TO_TICKS(5)); continue; }
        char c = (char)ModemIO.read();
        if (c == '\n')
        {
          if (hdrLine.indexOf("+HTTPREAD") != -1) { headerSeen = true; break; }
          if (hdrLine.indexOf("ERROR") != -1) { setLastError("HTTPREAD error"); return false; }
          hdrLine = "";
        }
        else if (c != '\r')
        {
          appendLimited(hdrLine, c, 64);
        }
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
    if (!waitForHttpReadEndLocked(10000)) return false;

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

  bool waitForHttpReadEndLocked(unsigned long timeout)
  {
    unsigned long start = millis();
    String buffer = "";
    buffer.reserve(96);

    while (millis() - start < timeout)
    {
      if (ModemIO.available())
      {
        char c = (char)ModemIO.read();
        appendLimited(buffer, c, 96);
        if (buffer.indexOf("OK") != -1 || buffer.indexOf("+HTTPREAD: 0") != -1) {
          clearLastError();
          return true;
        }
        if (buffer.indexOf("ERROR") != -1) {
          setLastError("HTTPREAD error");
          return false;
        }
      }
      else
      {
        vTaskDelay(pdMS_TO_TICKS(5));
      }
    }

    setLastError("timeout fin HTTPREAD");
    return false;
  }

  bool normalizeHttpUrl(const char *input, char *output, size_t outputSize)
  {
    if (input == nullptr || output == nullptr || outputSize == 0) {
      setLastError("URL HTTP inválida");
      return false;
    }

    const char *hostStart = nullptr;
    if (strncmp(input, "http://", 7) == 0) {
      hostStart = input + 7;
    } else if (strncmp(input, "https://", 8) == 0) {
      hostStart = input + 8;
    } else {
      setLastError("URL debe iniciar con http:// o https://");
      return false;
    }

    if (hostStart[0] == '\0' || hostStart[0] == '/') {
      setLastError("URL sin host");
      return false;
    }

    const char *pathStart = strchr(hostStart, '/');
    if (pathStart != nullptr) {
      int written = snprintf(output, outputSize, "%s", input);
      if (written < 0 || written >= (int)outputSize) {
        setLastError("URL demasiado larga");
        return false;
      }
      return true;
    }

    int written = snprintf(output, outputSize, "%s/", input);
    if (written < 0 || written >= (int)outputSize) {
      setLastError("URL demasiado larga");
      return false;
    }
    return true;
  }

  bool extractHttpHost(const char *url, char *host, size_t hostSize)
  {
    if (url == nullptr || host == nullptr || hostSize == 0) return false;

    const char *hostStart = nullptr;
    if (strncmp(url, "http://", 7) == 0) {
      hostStart = url + 7;
    } else if (strncmp(url, "https://", 8) == 0) {
      hostStart = url + 8;
    } else {
      return false;
    }

    const char *hostEnd = hostStart;
    while (*hostEnd != '\0' && *hostEnd != '/' && *hostEnd != ':') hostEnd++;
    size_t len = hostEnd - hostStart;
    if (len == 0 || len >= hostSize) return false;

    memcpy(host, hostStart, len);
    host[len] = '\0';
    return true;
  }

  bool buildHttpUrlWithHostIp(const char *url, const char *ip, char *output, size_t outputSize)
  {
    if (url == nullptr || ip == nullptr || output == nullptr || outputSize == 0) return false;
    if (strncmp(url, "http://", 7) != 0) return false;

    const char *hostStart = url + 7;
    const char *pathStart = strchr(hostStart, '/');
    if (pathStart == nullptr) pathStart = "/";

    int written = snprintf(output, outputSize, "http://%s%s", ip, pathStart);
    if (written < 0 || written >= (int)outputSize) {
      setLastError("URL IP demasiado larga");
      return false;
    }
    return true;
  }

  bool resolveHostIPv4Locked(const char *host, char *ip, size_t ipSize)
  {
    if (host == nullptr || host[0] == '\0' || ip == nullptr || ipSize == 0) {
      setLastError("host DNS inválido");
      return false;
    }

    char cmd[192];
    int written = snprintf(cmd, sizeof(cmd), "AT+CDNSGIP=\"%s\"", host);
    if (written < 0 || written >= (int)sizeof(cmd)) {
      setLastError("host DNS demasiado largo");
      return false;
    }

    String response = sendATResponseUntilFinalLocked(cmd, 45000, MaxScanResponseLength);
    if (response.indexOf("+CDNSGIP: 1") == -1 || response.indexOf("ERROR") != -1) {
      setLastError("CDNSGIP sin IPv4");
      return false;
    }

    int searchFrom = 0;
    while (searchFrom < (int)response.length())
    {
      int quoteStart = response.indexOf('"', searchFrom);
      if (quoteStart == -1) break;
      int quoteEnd = response.indexOf('"', quoteStart + 1);
      if (quoteEnd == -1) break;

      String candidate = response.substring(quoteStart + 1, quoteEnd);
      if (isValidIPv4(candidate)) {
        if (candidate.length() >= ipSize) {
          setLastError("IPv4 demasiado larga");
          return false;
        }
        copyCString(ip, ipSize, candidate.c_str());
        clearLastError();
        return true;
      }

      searchFrom = quoteEnd + 1;
    }

    setLastError("CDNSGIP sin IPv4 válida");
    return false;
  }

  bool isValidIPv4(const String &candidate)
  {
    int start = 0;
    for (int octet = 0; octet < 4; ++octet)
    {
      int end = octet == 3 ? candidate.length() : candidate.indexOf('.', start);
      if (end == -1) return false;
      int value = 0;
      if (!parseOctet(candidate, start, end, value)) return false;
      start = end + 1;
    }
    return start == (int)candidate.length() + 1;
  }

  const char *httpActionStatusText(int status)
  {
    switch (status)
    {
    case 600: return "Not HTTP PDU";
    case 601: return "Network Error";
    case 602: return "No memory";
    case 603: return "DNS Error";
    case 604: return "Stack Busy";
    case 701: return "Alert state";
    case 702: return "Unknown error";
    case 703: return "Busy";
    case 704: return "Connection closed";
    case 705: return "Timeout";
    case 706: return "Socket send/recv failed";
    case 707: return "File/memory error";
    case 708: return "Invalid parameter";
    case 709: return "Network error";
    case 710: return "SSL session failed";
    case 711: return "Wrong state";
    case 712: return "Create socket failed";
    case 713: return "Get DNS failed";
    case 714: return "Connect socket failed";
    case 715: return "Handshake failed";
    case 716: return "Close socket failed";
    case 717: return "No network";
    default: return "";
    }
  }

  bool pingLocked(const char *host, unsigned long *elapsedMs)
  {
    char cmd[96];
    int written = snprintf(cmd, sizeof(cmd), "AT+CPING=\"%s\",1,32,5000", host);
    if (written < 0 || written >= (int)sizeof(cmd)) {
      setLastError("host ping demasiado largo");
      return false;
    }

    // CPING devuelve OK de inmediato, luego URCs asincrónicos
    if (!sendATLocked(cmd, "OK", 3000)) return false;

    unsigned long start = millis();
    String line = "";
    line.reserve(64);

    while (millis() - start < 8000)
    {
      if (!ModemIO.available()) { vTaskDelay(pdMS_TO_TICKS(10)); continue; }
      char c = (char)ModemIO.read();
      if (c == '\n')
      {
        // Éxito: "+CPING: REPLY,..." o variante numérica del SIM7600
        if (line.indexOf("+CPING:") != -1)
        {
          if (line.indexOf("TIMEOUT") != -1 || line.indexOf("ERROR") != -1 || line.indexOf(": 2") != -1) {
            setLastError("ping timeout/error");
            return false;
          }
          // Intentar parsear el tiempo de la respuesta
          if (elapsedMs != nullptr) {
            // Formato SIM7600: "+CPING: ip,type,bytes,time,ttl" o "+CPING: REPLY,ip,bytes,time,ttl"
            // Buscar el cuarto campo numérico separado por coma
            int commas = 0;
            int timeStart = -1;
            for (int i = 0; i < (int)line.length(); i++) {
              if (line.charAt(i) == ',') {
                commas++;
                if (commas == 3) { timeStart = i + 1; break; }
              }
            }
            if (timeStart != -1) {
              *elapsedMs = (unsigned long)line.substring(timeStart).toInt();
            } else {
              *elapsedMs = millis() - start;
            }
          }
          return true;
        }
        line = "";
      }
      else if (c != '\r')
      {
        appendLimited(line, c, 128);
      }
    }

    setLastError("timeout ping sin respuesta");
    return false;
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

  bool sendATOptionalLocked(const char *command, const char *expected, unsigned long timeout)
  {
    char savedError[sizeof(lastError)];
    memcpy(savedError, lastError, sizeof(savedError));
    savedError[sizeof(savedError) - 1] = '\0';

    bool ok = sendATLocked(command, expected, timeout);
    if (!ok) setLastError(savedError);
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

  bool sendATResponseUntilFinalLocked(const char *command, char *out, size_t outLen, unsigned long timeout)
  {
    if (out == nullptr || outLen == 0) return false;
    out[0] = '\0';
    if (command == nullptr || command[0] == '\0' || !serialStarted) return false;
    clearRxLocked();
    ModemIO.println(command);

    size_t n = 0;
    bool truncated = false;
    unsigned long start = millis();
    while (millis() - start < timeout)
    {
      if (ModemIO.available())
      {
        char c = (char)ModemIO.read();
        if (n + 1 < outLen) {
          out[n++] = c;
          out[n] = '\0';
        } else {
          truncated = true;
        }

        if (strstr(out, "\r\nOK\r\n") != nullptr ||
            strstr(out, "\nOK\r") != nullptr ||
            strstr(out, "\nOK\n") != nullptr ||
            strstr(out, "ERROR") != nullptr)
        {
          break;
        }
      }
      else
      {
        vTaskDelay(pdMS_TO_TICKS(10));
      }
    }

    if (truncated) {
      setLastError("respuesta AT truncada");
      return false;
    }
    return out[0] != '\0' && strstr(out, "ERROR") == nullptr;
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

  void appendLimited(String &buffer, char c, size_t maxLen)
  {
    if (maxLen == 0) return;
    if (buffer.length() >= maxLen) {
      buffer.remove(0, buffer.length() - maxLen + 1);
    }
    buffer += c;
  }

  void copyCString(char *dest, size_t size, const char *src)
  {
    if (dest == nullptr || size == 0) return;
    if (src == nullptr) src = "";
    strncpy(dest, src, size - 1);
    dest[size - 1] = '\0';
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
