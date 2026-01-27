#ifndef MODEM_H
#define MODEM_H

#include <Arduino.h>
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

  void begin()
  {
    pinMode(powerPin, OUTPUT);
    digitalWrite(powerPin, HIGH);
    vTaskDelay(pdMS_TO_TICKS(2500));

    ModemIO.begin(115200, SERIAL_8N1, MODEM_TX, MODEM_RX);
    
    unsigned long start = millis();
    while (!ModemIO.available() && millis() - start < 5000)
    {
      vTaskDelay(pdMS_TO_TICKS(100));
    }

    sendAT("ATE0", "OK", 1000);
    connect();
  }

  void end()
  {
    sendAT("AT+NETCLOSE", "OK", 1000);
    ModemIO.end();
    digitalWrite(powerPin, LOW);
  }

  bool connect()
  {
    if (!sendAT("AT", "OK", 2000)) return false;
    if (!sendAT("AT+CPIN?", "READY", 5000)) return false;

    sendAT("AT+CGDCONT=1,\"IP\",\"gigsky-02\"", "OK", 5000);
    sendAT("AT+CGACT=1,1", "OK", 10000);

    if (!sendAT("AT+NETOPEN", "0", 10000))
    {
      if (!sendAT("AT+NETOPEN?", "1", 5000)) return false;
    }

    String ip = sendATResponse("AT+IPADDR", 5000);
    return ip.length() > 5;
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
    while (ModemIO.available()) ModemIO.read();
    ModemIO.println(command);
    return waitFor(expected, timeout);
  }

  String sendATResponse(const char *command, unsigned long timeout)
  {
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

private:
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
};

#endif