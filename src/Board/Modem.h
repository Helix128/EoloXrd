#ifndef MODEM_H
#define MODEM_H

#include <Arduino.h>
#include <RTClib.h>
#include "../Config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define ModemIO Serial1

class Modem
{
public:
  const int powerPin = MODEM_PWR_PIN;
  const int MODEM_RX = 16;
  const int MODEM_TX = 17;
  const char *apn = "gigsky-02";

  Modem() {}

  bool begin()
  {
    if (serialStarted)
    {
      return atReady || waitForAT(2000);
    }

    pinMode(powerPin, OUTPUT);
    digitalWrite(powerPin, HIGH);
    powered = true;
    vTaskDelay(pdMS_TO_TICKS(5000));

    ModemIO.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
    serialStarted = true;
    LOG_F("UART modem iniciado RX=%d TX=%d\n", MODEM_RX, MODEM_TX);

    if (!waitForAT(10000))
    {
      LOG_LN("Modem no responde a AT");
      return false;
    }

    if (!sendAT("ATE0", "OK", 1000))
    {
      LOG_LN("No se pudo desactivar eco del modem");
      return false;
    }

    atReady = true;
    return true;
  }

  void end()
  {
    if (serialStarted)
    {
      if (atReady)
      {
        sendAT("AT+NETCLOSE", "OK", 1000);
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
    if (!sendAT("AT", "OK", 2000)) return false;
    if (!sendAT("AT+CPIN?", "READY", 5000)) return false;

    if (isConnected()) return true;

    if (openNetwork(apnOverride) && isConnected()) return true;

    LOG_LN("Modem sin IP valida; reintentando conexion PDP");
    sendAT("AT+NETCLOSE", "OK", 5000);
    vTaskDelay(pdMS_TO_TICKS(1000));
    return openNetwork(apnOverride) && isConnected();
  }

  bool isConnected()
  {
    if (!sendAT("AT", "OK", 2000)) return false;

    String netState = sendATResponse("AT+NETOPEN?", 5000);
    if (netState.indexOf("+NETOPEN: 1") == -1) return false;

    String ip = sendATResponse("AT+IPADDR", 5000);
    return hasValidIp(ip);
  }

  bool openNetwork()
  {
    return openNetwork(apn);
  }

  bool openNetwork(const char *apnOverride)
  {
    if (apnOverride == nullptr || apnOverride[0] == '\0') apnOverride = apn;

    char cmd[64];
    snprintf(cmd, sizeof(cmd), "AT+CGDCONT=1,\"IP\",\"%s\"", apnOverride);
    if (!sendAT(cmd, "OK", 5000)) return false;
    if (!sendAT("AT+CGACT=1,1", "OK", 10000)) return false;

    if (!sendAT("AT+NETOPEN", "0", 20000))
    {
      if (!sendAT("AT+NETOPEN?", "1", 5000)) return false;
    }

    String ip = sendATResponse("AT+IPADDR", 5000);
    if (!hasValidIp(ip))
    {
      LOG_LN("Modem no obtuvo direccion IP");
      return false;
    }

    return true;
  }

  bool isSerialStarted() const
  {
    return serialStarted;
  }

  bool isPowered() const
  {
    return powered;
  }

  bool rawAT(const char *command, String &response, unsigned long timeout = 5000)
  {
    if (command == nullptr || command[0] == '\0') return false;
    response = sendATResponseUntilFinal(command, timeout);
    return response.length() > 0 && response.indexOf("ERROR") == -1;
  }

  bool scanOperators(String &response, unsigned long timeout = 180000)
  {
    response = "";
    if (!sendAT("AT", "OK", 2000)) return false;
    response = sendATResponseUntilFinal("AT+COPS=?", timeout);
    return response.indexOf("+COPS:") != -1 && response.indexOf("ERROR") == -1;
  }

  bool selectOperatorAuto()
  {
    return sendAT("AT+COPS=0", "OK", 120000);
  }

  bool selectOperatorNumeric(const char *numericCode)
  {
    if (numericCode == nullptr || numericCode[0] == '\0') return false;

    char cmd[40];
    snprintf(cmd, sizeof(cmd), "AT+COPS=1,2,\"%s\"", numericCode);
    return sendAT(cmd, "OK", 120000);
  }

  void printDiagnostics(Print &out)
  {
    out.printf("power=%s serial=%s at=%s\n",
               powered ? "on" : "off",
               serialStarted ? "on" : "off",
               atReady ? "ready" : "unknown");

    if (!serialStarted)
    {
      out.println("Modem apagado/no inicializado. Usa: m begin");
      return;
    }

    printAT(out, "AT");
    printAT(out, "ATI");
    printAT(out, "AT+CPIN?");
    printAT(out, "AT+CSQ");
    printAT(out, "AT+COPS?");
    printAT(out, "AT+CREG?");
    printAT(out, "AT+CGREG?");
    printAT(out, "AT+CEREG?");
    printAT(out, "AT+CGACT?");
    printAT(out, "AT+NETOPEN?");
    printAT(out, "AT+IPADDR");
  }

  bool get(const char *url, char *respBuffer, int bufferSize)
  {
    sendAT("AT+HTTPINIT", "OK", 2000);

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "AT+HTTPPARA=\"URL\",\"%s\"", url);

    if (!sendAT(cmd, "OK", 2000))
    {
      sendAT("AT+HTTPTERM", "OK", 1000);
      return false;
    }

    sendAT("AT+HTTPACTION=0", "OK", 10000);

    bool success = false;
    unsigned long start = millis();
    int dataLen = 0;

    while (millis() - start < 15000)
    {
      String line = readLine();
      if (line.indexOf("+HTTPACTION: 0,200,") != -1)
      {
        int firstComma = line.indexOf(',');
        int secondComma = line.indexOf(',', firstComma + 1);
        dataLen = line.substring(secondComma + 1).toInt();
        success = true;
        break;
      }
      if (line.indexOf("+HTTPACTION: 0,") != -1 && line.indexOf(",200,") == -1)
      {
        break;
      }
      vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (success && dataLen > 0)
    {
      snprintf(cmd, sizeof(cmd), "AT+HTTPREAD=%d", dataLen);
      ModemIO.println(cmd);

      unsigned long readStart = millis();
      int idx = 0;
      
      while (millis() - readStart < 10000 && idx < bufferSize - 1)
      {
        if (ModemIO.available())
        {
          char c = ModemIO.read();
          respBuffer[idx++] = c;
          if (idx >= dataLen) break;
        }
        else
        {
          vTaskDelay(pdMS_TO_TICKS(5));
        }
      }
      respBuffer[idx] = '\0';
    }

    sendAT("AT+HTTPTERM", "OK", 1000);
    return success;
  }
  
  bool post(const char *url, const char *payload, char *respBuffer, int bufferSize)
  {
    sendAT("AT+HTTPINIT", "OK", 2000);

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "AT+HTTPPARA=\"URL\",\"%s\"", url);
    if (!sendAT(cmd, "OK", 2000))
    {
      sendAT("AT+HTTPTERM", "OK", 1000);
      return false;
    }

    sendAT("AT+HTTPPARA=\"CONTENT\",\"application/x-www-form-urlencoded\"", "OK", 2000);

    int payloadLen = strlen(payload);
    snprintf(cmd, sizeof(cmd), "AT+HTTPDATA=%d,10000", payloadLen);

    if (!sendAT(cmd, "DOWNLOAD", 5000))
    {
      sendAT("AT+HTTPTERM", "OK", 1000);
      return false;
    }

    ModemIO.print(payload);
    if (!waitFor("OK", 5000))
    {
      sendAT("AT+HTTPTERM", "OK", 1000);
      return false;
    }

    sendAT("AT+HTTPACTION=1", "OK", 5000);

    bool success = false;
    unsigned long start = millis();
    int dataLen = 0;

    while (millis() - start < 15000)
    {
      String line = readLine();
      if (line.indexOf("+HTTPACTION: 1,200,") != -1)
      {
        int firstComma = line.indexOf(',');
        int secondComma = line.indexOf(',', firstComma + 1);
        dataLen = line.substring(secondComma + 1).toInt();
        success = true;
        break;
      }
      if (line.indexOf("+HTTPACTION: 1,") != -1 && line.indexOf(",200,") == -1) break;
      vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (success && dataLen > 0)
    {
      snprintf(cmd, sizeof(cmd), "AT+HTTPREAD=%d", dataLen);
      ModemIO.println(cmd);

      unsigned long readStart = millis();
      int idx = 0;
      
      while (millis() - readStart < 5000 && idx < bufferSize - 1)
      {
        if (ModemIO.available())
        {
          char c = ModemIO.read();
          respBuffer[idx++] = c;
          if (idx >= dataLen) break;
        } 
        else 
        {
          vTaskDelay(pdMS_TO_TICKS(5));
        }
      }
      respBuffer[idx] = '\0';
    }

    sendAT("AT+HTTPTERM", "OK", 1000);
    return success;
  }

  bool sendAT(const char *command, const char *expected, unsigned long timeout)
  {
    if (!serialStarted) return false;
    while (ModemIO.available()) ModemIO.read();
    ModemIO.println(command);
    return waitFor(expected, timeout);
  }

  String sendATResponse(const char *command, unsigned long timeout)
  {
    if (!serialStarted) return "";
    while (ModemIO.available()) ModemIO.read();
    ModemIO.println(command);

    String response = "";
    unsigned long start = millis();
    while (millis() - start < timeout)
    {
      if (ModemIO.available())
      {
        char c = ModemIO.read();
        response += c;
      }
      else
      {
        vTaskDelay(pdMS_TO_TICKS(10));
      }
    }
    return response;
  }

  String sendATResponseUntilFinal(const char *command, unsigned long timeout)
  {
    if (!serialStarted) return "";
    while (ModemIO.available()) ModemIO.read();
    ModemIO.println(command);

    String response = "";
    unsigned long start = millis();
    while (millis() - start < timeout)
    {
      if (ModemIO.available())
      {
        char c = ModemIO.read();
        response += c;

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

  bool getNetworkTimeUTC(const char *serverHost, DateTime &utcTime)
  {
    if (!sendAT("AT", "OK", 2000)) return false;

    char cmd[96];
    snprintf(cmd, sizeof(cmd), "AT+CNTP=\"%s\",0", serverHost);
    if (!sendAT(cmd, "OK", 5000)) return false;

    if (!runNtpSync())
    {
      LOG_LN("Servidor NTP no respondio correctamente");
      return false;
    }

    String clockResponse = sendATResponse("AT+CCLK?", 5000);
    return parseClockResponse(clockResponse, utcTime);
  }

private:
  bool serialStarted = false;
  bool powered = false;
  bool atReady = false;

  bool waitForAT(unsigned long timeout)
  {
    unsigned long start = millis();
    while (millis() - start < timeout)
    {
      if (sendAT("AT", "OK", 1000)) return true;
      vTaskDelay(pdMS_TO_TICKS(250));
    }
    return false;
  }

  bool waitFor(const char *expected, unsigned long timeout)
  {
    unsigned long start = millis();
    String buffer = "";
    
    while (millis() - start < timeout)
    {
      if (ModemIO.available())
      {
        char c = ModemIO.read();
        buffer += c;
        if (buffer.length() > 200) buffer = buffer.substring(50);

        if (buffer.indexOf(expected) != -1) return true;
        if (buffer.indexOf("ERROR") != -1) return false;
      }
      else
      {
        vTaskDelay(pdMS_TO_TICKS(10));
      }
    }
    return false;
  }

  String readLine()
  {
    String line = "";
    unsigned long start = millis();
    while (millis() - start < 2000)
    {
      if (ModemIO.available())
      {
        char c = ModemIO.read();
        if (c == '\n') return line;
        if (c != '\r') line += c;
      }
      else
      {
        vTaskDelay(pdMS_TO_TICKS(5));
      }
    }
    return line;
  }

  bool runNtpSync()
  {
    while (ModemIO.available()) ModemIO.read();
    ModemIO.println("AT+CNTP");

    String buffer = "";
    bool sawNtpResult = false;
    unsigned long start = millis();
    while (millis() - start < 65000)
    {
      if (ModemIO.available())
      {
        char c = ModemIO.read();
        buffer += c;
        if (buffer.length() > 300) buffer = buffer.substring(80);
        if (buffer.indexOf("+CNTP: 1,0") != -1 || buffer.indexOf("+CNTP: 1, 0") != -1)
          return true;
        if (buffer.indexOf("+CNTP:") != -1)
          sawNtpResult = true;
        if (sawNtpResult && c == '\n')
          return false;
      }
      else
      {
        vTaskDelay(pdMS_TO_TICKS(20));
      }
    }
    return false;
  }

  bool hasValidIp(const String &response)
  {
    if (response.indexOf("ERROR") != -1) return false;

    int dots = 0;
    int digits = 0;
    bool inAddress = false;

    for (size_t i = 0; i < response.length(); i++)
    {
      char c = response.charAt(i);
      if (c >= '0' && c <= '9')
      {
        digits++;
        inAddress = true;
      }
      else if (c == '.' && inAddress)
      {
        dots++;
      }
      else if (inAddress)
      {
        if (dots == 3 && digits >= 4) return true;
        dots = 0;
        digits = 0;
        inAddress = false;
      }
    }

    return dots == 3 && digits >= 4;
  }

  bool parseClockResponse(const String &response, DateTime &utcTime)
  {
    int quoteStart = response.indexOf('"');
    int quoteEnd = response.indexOf('"', quoteStart + 1);
    if (quoteStart == -1 || quoteEnd == -1) return false;

    String value = response.substring(quoteStart + 1, quoteEnd);
    if (value.length() < 17) return false;

    int year = value.substring(0, 2).toInt() + 2000;
    int month = value.substring(3, 5).toInt();
    int day = value.substring(6, 8).toInt();
    int hour = value.substring(9, 11).toInt();
    int minute = value.substring(12, 14).toInt();
    int second = value.substring(15, 17).toInt();

    if (year < 2024 || month < 1 || month > 12 || day < 1 || day > 31 ||
        hour > 23 || minute > 59 || second > 59)
    {
      return false;
    }

    utcTime = DateTime(year, month, day, hour, minute, second);
    return true;
  }

  void printAT(Print &out, const char *command)
  {
    out.printf("> %s\n", command);
    String response = sendATResponseUntilFinal(command, 5000);
    response.trim();
    if (response.length() == 0) response = "(sin respuesta)";
    out.println(response);
  }
};

#endif
