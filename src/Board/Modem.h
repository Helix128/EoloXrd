#ifndef MODEM_H
#define MODEM_H

#define ModemIO Serial1

#define KB(x) x * 1024
#define MB(x) x* 1024 * 1024

#pragma message("REVISAR MODEM! No está listo.")

#include "ESPJob.h"
#include "../Config.h"

class Modem
{
public:
  // pines
  const int powerPin = MODEM_PWR_PIN;
  const int MODEM_RX = 16;
  const int MODEM_TX = 17;

  // conexión
  const char *apn = "gigsky-02";

  // datos
  const char *deviceName = "EOMP-000";
  const int sensorIds[7] = {0, 0, 0, 0, 0, 0, 0};

  Modem() {}

  bool findNetwork()
  {
    Serial.println("Escaneando redes disponibles...");

    const int RESP_SIZE = 4096;
    char *response = (char *)malloc(RESP_SIZE);

    if (response == NULL)
    {
      Serial.println("Error de asignación de memoria.");
      return false;
    }

    if (sendAT("AT+COPS=?", 120000, response, RESP_SIZE) != 1)
    {
      Serial.println("Fallo al escanear redes.");
      free(response);
      return false;
    }

    Serial.println("Analizando candidatos...");

    char *cursor = response;
    char cmdBuffer[64];
    char smallBuffer[64];

    while ((cursor = strstr(cursor, "(1,")) != NULL)
    {
      char *groupEnd = strchr(cursor, ')');
      if (!groupEnd)
        break;

      bool isLTE = (*(groupEnd - 2) == ',' && (*(groupEnd - 1) == '7' || *(groupEnd - 1) == '9'));

      if (isLTE)
      {
        char *q2 = NULL;
        char *q1 = NULL;

        char *scanner = groupEnd;
        while (scanner > cursor)
        {
          if (*scanner == '"')
          {
            if (!q2)
              q2 = scanner;
            else
            {
              q1 = scanner;
              break;
            }
          }
          scanner--;
        }

        if (q1 && q2)
        {
          char targetID[16];
          int len = q2 - (q1 + 1);
          if (len > 15)
            len = 15;
          strncpy(targetID, q1 + 1, len);
          targetID[len] = '\0';

          Serial.print("Intentando conectar a: ");
          Serial.println(targetID);

          snprintf(cmdBuffer, sizeof(cmdBuffer), "AT+COPS=1,2,\"%s\",7", targetID);

          if (sendAT(cmdBuffer, 30000, smallBuffer, sizeof(smallBuffer)) == 1)
          {
            Serial.println("Esperando confirmación de red...");
            delay(5000);

            sendAT("AT+CEREG?", 3000, smallBuffer, sizeof(smallBuffer));

            if (strstr(smallBuffer, ",1") || strstr(smallBuffer, ",5"))
            {
              Serial.println("Conexión manual establecida.");
              free(response);
              return true;
            }
            else
            {
              Serial.println("La red rechazó el registro.");
            }
          }
          else
          {
            Serial.println("El modem rechazó el comando COPS.");
          }
        }
      }
      cursor = groupEnd + 1;
    }

    free(response);
    return false;
  }

  void begin()
  {
    Serial.println("Iniciando modem...");

    pinMode(powerPin, OUTPUT);
    pinMode(4, OUTPUT);

    digitalWrite(powerPin, HIGH);
    digitalWrite(4, HIGH);

    Serial.println("Esperando modem...");

    // es el único delay explícitamente necesario
    // sin este delay el modem no funciona (el serial se habilita pero nunca responde)
    delay(2500);
    ModemIO.begin(115200, SERIAL_8N1, MODEM_TX, MODEM_RX);

    while (!ModemIO.available())
    {
      delay(500);
    }
    Serial.println();
    Serial.print("Modem listo.");

    bool connected = connect();

    const int MAX_RETRIES = 3;
    int attempts = 0;
    while (!connected && attempts < MAX_RETRIES)
    {
      Serial.println("Reintentando conexión...");
      delay(2000);
      connected = connect();
      attempts++;
    }

    String IPADDR = sendAT("AT+IPADDR", 5000);
    Serial.print("Dirección IP asignada: ");
    Serial.println(IPADDR);
  }

  void end()
  {
    disconnect();
    Serial.println("Fin.");
    ModemIO.end();
    digitalWrite(powerPin, LOW);
  }

  // requests
  bool get(const char *url, char *respBuffer, int bufferSize, int timeout = 15000)
  {
    
    sendAT("AT+HTTPINIT", "OK", 2000);
    sendAT("AT+HTTPPARA=\"CID\",1", "OK", 2000);

    char cmdBuffer[256];
    snprintf(cmdBuffer, sizeof(cmdBuffer), "AT+HTTPPARA=\"URL\",\"%s\"", url);
    if (!sendAT(cmdBuffer, "OK", 2000))
    {
      Serial.println("Fallo URL");
      return false;
    }
    
    while (ModemIO.available())
      ModemIO.read(); 

    if (!sendAT("AT+HTTPACTION=0", "OK", 5000))
    {
      Serial.println("Fallo HTTPACTION request");
      return false;
    }

    unsigned long start = millis();
    int httpCode = 0;
    int dataLen = 0;
    bool success = false;

    char lineBuf[64];
    int lineIdx = 0;

    while (millis() - start < timeout)
    {
      if (ModemIO.available())
      {
        char c = ModemIO.read();

        if (c < 32 && c != '\n' && c != '\r')
          continue;

        if (c == '\n')
        {
          lineBuf[lineIdx] = 0; 

          char *p = strstr(lineBuf, "+HTTPACTION:");
          if (p != NULL)
          {
            Serial.print("URC Recibido: ");
            Serial.println(lineBuf); 

            int method;
            if (sscanf(p, "+HTTPACTION: %d,%d,%d", &method, &httpCode, &dataLen) >= 2)
            {
              if (httpCode == 200)
              {
                success = true;
              }
              else
              {
                Serial.print("HTTP Fail Code: ");
                Serial.println(httpCode);
              }
            }
            break; 
          }
          lineIdx = 0; 
        }
        else if (lineIdx < 63 && c != '\r')
        {
          lineBuf[lineIdx++] = c;
        }
      }
    }

    if (!success || dataLen <= 0)
    {
      Serial.printf("Error o sin datos. Codigo: %d\n", httpCode);
      sendAT("AT+HTTPTERM", "OK", 1000);
      return false;
    }

    snprintf(cmdBuffer, sizeof(cmdBuffer), "AT+HTTPREAD=%d", dataLen);

    while (ModemIO.available())
      ModemIO.read();
    ModemIO.println(cmdBuffer); 

    unsigned long readStart = millis();
    int bytesRead = 0;
    int state = 0;
    memset(respBuffer, 0, bufferSize);

    lineIdx = 0;

    while (millis() - readStart < 8000)
    {
      if (ModemIO.available())
      {
        char c = ModemIO.read();
        readStart = millis(); 

        if (state == 0)  
        {
          if (c == '\n')
          {
            lineBuf[lineIdx] = 0;
            if (strstr(lineBuf, "+HTTPREAD:"))
            {
              state = 1;  
            }
            lineIdx = 0;
          }
          else if (lineIdx < 63 && c != '\r')
          {
            lineBuf[lineIdx++] = c;
          }
        }
        else if (state == 1)  
        {
          if (bytesRead < bufferSize - 1 && bytesRead < dataLen)
          {
            respBuffer[bytesRead++] = c;
          }
          if (bytesRead >= dataLen)
            break; 
        }
      }
    }

    respBuffer[bytesRead] = 0;
    sendAT("AT+HTTPTERM", "OK", 1000);
    return (bytesRead > 0);
  }

  bool connect()
  {
    bool AT = sendAT("AT", "OK", 2000);
    if (!AT)
      return false;

    bool CPIN = sendAT("AT+CPIN?", "READY", 5000);
    if (!CPIN)
      return false;
    int signalQuality = getSignalQuality();
    Serial.print("Calidad de señal (RSSI): ");
    Serial.println(signalQuality);

    bool signalOk = true;
    if (signalQuality < 10 || signalQuality == 99)
    {
      Serial.println("Señal insuficiente.");
      signalOk = false;
    }

    if (!signalOk)
    {
      const int MAX_RETRIES_CREG = 12;
      bool registered = findNetwork();

      if (!registered)
      {
        Serial.println("No registrado en red celular.");
        return false;
      }
      Serial.println("Registrado en red celular.");
    }
    bool CGDCONT = sendAT("AT+CGDCONT?", "IP", 5000);
    if (!CGDCONT)
      return false;

    bool CGDCONT_SET = sendAT("AT+CGDCONT=1,\"IP\",\"gigsky-02\"", "OK", 5000);
    if (!CGDCONT_SET)
      return false;
    bool CGACT = sendAT("AT+CGACT=1,1", "OK", 10000);
    if (!CGACT)
      return false;

    bool NETOPEN = sendAT("AT+NETOPEN", "0", 10000);
    if (!NETOPEN)
    {
      NETOPEN = sendAT("AT+NETOPEN?", "1", 5000);
      if (!NETOPEN)
        return false;
    }
    return true;
  }

  int getSignalQuality()
  {
    char *buffer = (char *)malloc(64);
    sendAT("AT+CSQ", 2000, buffer, 64);
    int quality = extractSignalQuality(buffer);
    free(buffer);
    return quality;
  }

  void readResponse(char *buffer, int maxLength)
  {
    int index = 0;
    unsigned long startTime = millis();
    while (millis() - startTime < 1000)
    {
      if (ModemIO.available() && index < maxLength - 1)
      {
        char c = ModemIO.read();
        if (c == '\r' || c == '\n')
          continue;
        buffer[index++] = c;
      }
    }
    buffer[index] = '\0';
  }

  int extractSignalQuality(char *response)
  {
    char *p = strstr(response, "+CSQ: ");
    if (p != NULL)
    {
      int rssi;
      if (sscanf(p, "+CSQ: %d,", &rssi) == 1)
        return rssi;
    }
    return -1;
  }

  bool disconnect()
  {
    sendAT("AT+NETCLOSE", "OK", 1000);
    return true;
  }

  bool sendAT(const char *ATcommand, const char *resp_correcta, unsigned int tiempo)
  {
    int x = 0;
    bool correcto = false;
    char respuesta[100];
    unsigned long anterior;

    memset(respuesta, '\0', 100);

    while (ModemIO.available() > 0)
      ModemIO.read();

    ModemIO.println(ATcommand);

    x = 0;
    anterior = millis();

    do
    {
      if (ModemIO.available() != 0)
      {
        char c = ModemIO.read();
        if (x < sizeof(respuesta) - 1)
        {
          respuesta[x] = c;
          x++;
          respuesta[x] = '\0';
        }

        if (strstr(respuesta, resp_correcta) != NULL)
        {
          correcto = true;
        }

        else if (strstr(respuesta, "ERROR") != NULL)
        {
          correcto = false;
          break;
        }
      }
    } while (!correcto && ((millis() - anterior) < tiempo));

    Serial.println(respuesta);
    return correcto;
  }

  // TODO: dejar de usar String y usar solo char* para manejo de memoria
  String sendAT(const char *ATcommand, unsigned int timeout)
  {
    String respuesta = "";

    while (ModemIO.available() > 0)
      ModemIO.read();

    ModemIO.println(ATcommand);

    unsigned long anterior = millis();

    while ((millis() - anterior) < timeout)
    {
      if (ModemIO.available() != 0)
      {
        char c = ModemIO.read();
        respuesta += c;
        if (respuesta.endsWith("OK\r\n") || respuesta.endsWith("ERROR\r\n"))
        {
          break;
        }
      }
    }

    return respuesta;
  }

  // 1 = OK, -1 = ERROR, 0 = TIMEOUT o buffer sin espacio
  char sendAT(const char *ATcommand, uint32_t timeout, char *buffer, uint16_t bufferSize)
  {
    while (ModemIO.available() > 0)
      ModemIO.read();

    memset(buffer, 0, bufferSize);

    ModemIO.println(ATcommand);

    unsigned long anterior = millis();
    unsigned short index = 0;

    while ((millis() - anterior) < timeout)
    {
      if (ModemIO.available() > 0)
      {
        char c = ModemIO.read();
        if (index < bufferSize - 1)
        {
          buffer[index] = c;
          index++;
          buffer[index] = '\0';
        }
        else
        {
          return 0;
        }

        if (strstr(buffer, "OK\r\n") != NULL)
        {
          return 1;
        }
        if (strstr(buffer, "ERROR\r\n") != NULL)
        {
          return -1;
        }
      }
    }
    return 0;
  }
};
#endif