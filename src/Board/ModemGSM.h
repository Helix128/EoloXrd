#ifndef MODEM_GSM_H
#define MODEM_GSM_H

#include <Arduino.h>
#include "../Config.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TINY_GSM_MODEM_SIM7600
#include <TinyGsmClient.h>

#define SerialAT Serial1

class ModemGSM
{
public:
  const int powerPin = MODEM_PWR_PIN;
  const int MODEM_RX = 16;
  const int MODEM_TX = 17;
  const char *apn = "gigsky-02";

  TinyGsm modem;
  TinyGsmClient client;
#ifndef MODEM_H
#define MODEM_H

#include <Arduino.h>
#include "../Config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define ModemIO Serial1

class ModemManual
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
  Modem() : modem(SerialAT), client(modem) {}

  void begin()
  {
    pinMode(powerPin, OUTPUT);
    digitalWrite(powerPin, HIGH);
    vTaskDelay(pdMS_TO_TICKS(500)); 

    SerialAT.begin(115200, SERIAL_8N1, MODEM_TX, MODEM_RX, false);
    vTaskDelay(pdMS_TO_TICKS(3000));

    modem.restart();
  }

  void end()
  {
    modem.gprsDisconnect();
    digitalWrite(powerPin, LOW);
  }

  bool connect()
  {
    if (!modem.waitForNetwork(60000L)) return false;
    if (!modem.gprsConnect(apn)) return false;
    return modem.isNetworkConnected();
  }

  bool get(const char *url, char *respBuffer, int bufferSize)
  {
    char host[64];
    char path[128];
    int port = 80;

    if (!parseUrl(url, host, path, port)) return false;

    if (!client.connect(host, port)) return false;

    client.print(String("GET ") + path + " HTTP/1.1\r\n");
    client.print(String("Host: ") + host + "\r\n");
    client.print("Connection: close\r\n\r\n");

    return readResponse(respBuffer, bufferSize);
  }
  
  bool post(const char *url, const char *payload, char *respBuffer, int bufferSize)
  {
    char host[64];
    char path[128];
    int port = 80;

    if (!parseUrl(url, host, path, port)) return false;

    if (!client.connect(host, port)) return false;

    client.print(String("POST ") + path + " HTTP/1.1\r\n");
    client.print(String("Host: ") + host + "\r\n");
    client.print("Content-Type: application/x-www-form-urlencoded\r\n");
    client.print(String("Content-Length: ") + strlen(payload) + "\r\n");
    client.print("Connection: close\r\n\r\n");
    client.print(payload);

    return readResponse(respBuffer, bufferSize);
  }

private:
  bool parseUrl(const char *url, char *host, char *path, int &port)
  {
    String urlStr = String(url);
    int index = urlStr.indexOf("://");
    if (index != -1) urlStr = urlStr.substring(index + 3);

    index = urlStr.indexOf('/');
    String hostStr = (index == -1) ? urlStr : urlStr.substring(0, index);
    String pathStr = (index == -1) ? "/" : urlStr.substring(index);

    int portIndex = hostStr.indexOf(':');
    if (portIndex != -1)
    {
      port = hostStr.substring(portIndex + 1).toInt();
      hostStr = hostStr.substring(0, portIndex);
    }
    else
    {
      port = 80;
    }

    if (hostStr.length() >= 63 || pathStr.length() >= 127) return false;
    
    strcpy(host, hostStr.c_str());
    strcpy(path, pathStr.c_str());
    return true;
  }

  bool readResponse(char *respBuffer, int bufferSize)
  {
    unsigned long timeout = millis();
    while (client.connected() && !client.available())
    {
      if (millis() - timeout > 10000L) 
      {
        client.stop();
        return false;
      }
      vTaskDelay(pdMS_TO_TICKS(10));
    }

    bool headerEnded = false;
    int idx = 0;
    String line;
    
    timeout = millis();
    while (client.connected() || client.available())
    {
      if (millis() - timeout > 10000L) break;

      if (client.available())
      {
        if (!headerEnded)
        {
           char c = client.read();
           if (c == '\n')
           {
             if (line.length() == 0 || line == "\r") headerEnded = true;
             line = "";
           }
           else if (c != '\r')
           {
             line += c;
           }
        }
        else
        {
          if (idx < bufferSize - 1)
          {
            respBuffer[idx++] = client.read();
          }
          else
          {
            client.read(); 
          }
        }
      }
      else
      {
        vTaskDelay(pdMS_TO_TICKS(10));
      }
    }
    
    respBuffer[idx] = '\0';
    client.stop();
    return (idx > 0);
  }
};

#endif