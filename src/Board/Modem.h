#ifndef MODEM_H
#define MODEM_H

#define ModemIO Serial1

#pragma message("REVISAR MODEM! No está listo.");

// TODO 
// implementar usando async para evitar bloqueos 
// mejorar gestión de tiempos de espera
// manejo de encendido y apagado del modem
// code cleanup

class Modem
{
public:

  // pines
  const int powerPin = 13;
  const int MODEM_RX = 16;
  const int MODEM_TX = 17;

  // conexión
  const char *apn = "gigsky-02"; // a definir 

  // datos (TODO PENDIENTE Y A DEFINIR COMO GUARDARLOS BIEN)
  const char* deviceName = "EOMP-000";
  const int sensorIds[7]; // a definir 
  // cómo voy a recibir los valores?
  // sensor registry? valores directos? pendiente!!!

  Modem();

  void sendData()
  {
    Serial.println("Iniciando modem...");

    // encender modem ??
    ModemIO.begin(115200, SERIAL_8N1, MODEM_TX, MODEM_RX);
    bool isConnected = true;
    unsigned long waitStart = millis();

    while (ModemIO.available())
    {
      if (millis() - waitStart > 30000)
      {
        Serial.println("El modem no contesta");
        isConnected = false;
        break;
      }
    }

    if (isConnected)
    {
      while (ModemIO.available())
      {
        Serial.print((char)ModemIO.read()); // estado inicial
      }

      const int GSMRetries = 30;
      bool gsmReady = false;
      for (int i = 0; i < GSMRetries; i++)
      {
        if (connectGSM())
        {
          Serial.println("Modem conectado a la red");
          gsmReady = true;
          break;
        }
        delay(1000);
      }

      if (gsmReady)
      {
        int signalQuality = getSignalQuality();
        Serial.print("Calidad de señal: ");
        Serial.println(signalQuality);
        connectGPRS();
        sendCache();
        disconnect();
      }
      else
      {
        Serial.println("No hay señal");
      }
    }
    ModemIO.end();
    // apagar modem (?)
  }

  int getSignalQuality()
  {
    ModemIO.println("AT+CSQ");
    delay(1000); // reemplazar por espera más "safe" o rápida
    // async? while wait? TODO
    if (ModemIO.available())
    {
      char response[100];
      readResponse(response, sizeof(response));
      Serial.print("Respuesta: ");
      Serial.println(response);

      Serial.print("Calidad Señal: ");
      // Extraer el valor de la calidad de la señal
      int signalQuality = extractSignalQuality(response);
      if (signalQuality != -1)
      {
        Serial.println(signalQuality);
      }
      else
      {
        Serial.println("Error");
      }
      return signalQuality;
    }
    else
    {
      return -1;
    }
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
          continue; // Ignorar los caracteres de nueva línea
        buffer[index++] = c;
      }
    }
    buffer[index] = '\0'; // Terminar la cadena correctamente
  }

  int extractSignalQuality(char *response)
  {
    char *p = strstr(response, "+CSQ: ");
    if (p != NULL)
    {
      int rssi;
      if (sscanf(p, "+CSQ: %d,", &rssi) == 1)
      {
        return rssi;
      }
    }
    return -1; // Indica error
  }

  bool sendCache()
  {
    // TODO
  }

  bool connect()
  {
    Serial.println("Iniciando conexión...");
    if (!sendAT("AT", "OK", 1000))
    {
      Serial.println("No responde AT");
      return false;
    }

    if (!connectGSM())
    {
      Serial.println("Fallo en registro GSM");
      return false;
    }

    if (!connectGPRS())
    {
      Serial.println("Fallo en apertura GPRS");
      return false;
    }

    return true;
  }

  bool connectGSM(unsigned long timeout = 30000)
  {
    unsigned long start = millis();

    while (millis() - start < timeout)
    {
      if (sendAT("AT+CREG?", "+CREG: 0,1", 1000) || sendAT("AT+CREG?", "+CREG: 0,5", 1000))
      {
        Serial.println("CREG registrado");
        break;
      }
      delay(1000);
    }
    if (millis() - start >= timeout)
    {
      Serial.println("Timeout CREG");
      return false;
    }

    start = millis();
    while (millis() - start < timeout)
    {
      if (sendAT("AT+CGREG?", "+CGREG: 0,1", 1000) || sendAT("AT+CGREG?", "+CGREG: 0,5", 1000))
      {
        Serial.println("CGREG registrado");
        return true;
      }
      delay(1000);
    }
    Serial.println("Timeout CGREG");
    return false;
  }

  bool connectGPRS()
  {
    char atCommand[255];
    snprintf(atCommand, sizeof(atCommand), "AT+CGDCONT=1,\"IP\",\"%s\"", apn);
    if (sendAT(atCommand, "OK", 1000))
    {
      Serial.println("APN configurado");
    }
    else
    {
      Serial.println("Fallo AT+CGDCONT");
      return false;
    }

    if (!sendAT("AT+NETOPEN", "+NETOPEN: 0", 5000))
    {
      if (!sendAT("AT+NETOPEN", "OK", 5000))
      {
        Serial.println("Fallo AT+NETOPEN");
        return false;
      }
    }
    Serial.println("NETOPEN OK");

    if (!sendAT("AT+HTTPINIT", "OK", 1000))
    {
      Serial.println("Fallo AT+HTTPINIT");
      return false;
    }
    Serial.println("HTTPINIT OK");

    return true;
  }

  bool disconnect()
  {
    bool http = sendAT("AT+HTTPTERM", "OK", 1000);
    bool net = sendAT("AT+NETCLOSE", "OK", 1000);
    return http && net;
  }

  bool sendAT(const char *ATcommand, const char *resp_correcta, unsigned int tiempo)
  {
    int x = 0;
    bool correcto = false;
    char respuesta[100];
    unsigned long anterior;

    if (strcmp(resp_correcta, "any") == 0)
    {
      while (ModemIO.available() > 0)
        ModemIO.read();
      ModemIO.println(ATcommand);
      delay(100);
      while (ModemIO.available() > 0)
        Serial.print((char)ModemIO.read());
      correcto = true;
    }
    else
    {
      memset(respuesta, '\0', 100);
      delay(100);
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
          }
          if (strstr(respuesta, resp_correcta) != NULL)
          {
            correcto = true;
          }
        }
      } while (!correcto && ((millis() - anterior) < tiempo));
    }

    respuesta[x] = '\0'; // Asegurarse de que la cadena esté terminada
    Serial.println(respuesta);
    return correcto;
  }
};
#endif