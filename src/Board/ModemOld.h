// TODO - convertir a clase Modem, usar sensores y dispositivo EOMP-000/001

void sendData(){
  unsigned long modemInstanceStartTime = millis();
  Serial.println();
  Serial.println("> Send Data");
  turnOnVRM();
  muxYellowLed();
  checkSD();                                              // Verificamos tarjeta SD
  checkFile();                                            // Verificamos que existan los CSV

  //pinMode(17, INPUT);  
  //pinMode(16, INPUT);  

  myModem.begin(115200, SERIAL_8N1,17,16);
  Serial.println("Esperando Modem");
  modemReady = false;
  bool modemConnected = true;

  unsigned long startWaitTime = millis();
  while(!myModem.available()){
    // Espera que el modem entregue su estado inicial     
    if(millis() - startWaitTime > 30000){
      Serial.println("El modem no contesta");
      modemConnected = false;
      break;
    }
  }

  if(modemConnected){
    muxGreenLed();
    while(myModem.available()){               // Recibe el estado inicial
      Serial.print((char)myModem.read());
    }
    //comandoAT("AT+COPS=0", "\r\nOK\r\n", 1000);

    for(int i=0; i<30; i++){                  // Verifica conexión
      if(connectGSM()){
        modemReady = true;
        break;
      }
      delay(1000);
    }

    signalValue = signalQuality();
    
    if(modemReady){
      connectGPRS();
      sendCache();
      closeGPRS();
    }
    else{
      Serial.println("No hay señal. No borramos Caché.");
      muxRedLed();
      delay(1000);
      muxOffLed();

    }
    modemInstance = false;
  }
  myModem.end();
  digitalWrite(17, LOW);
  digitalWrite(16, LOW);
  for(int i = 0; i < 5; i++)
    blinkGreenLed();
}

//////////////////////////////////////////////////////////////////////////

int signalQuality(){
  myModem.println("AT+CSQ");
  delay(1000);
  if (myModem.available()) {
    char response[100];
    readResponse(response, sizeof(response));
    //Serial.print("Respuesta: ");
    //Serial.println(response);

    Serial.print("Calidad Señal: ");
    // Extraer el valor de la calidad de la señal
    int signalQuality = extractSignalQuality(response);
    if (signalQuality != -1) {
      Serial.println(signalQuality);
    }
    else {
      Serial.println("Error");
    }
    return signalQuality;
  }
  else{
    return -1;
  }
}

// Función para leer la respuesta completa del módulo
void readResponse(char* buffer, int maxLength) {
  int index = 0;
  unsigned long startTime = millis();
  while (millis() - startTime < 1000) {
    if (myModem.available() && index < maxLength - 1) {
      char c = myModem.read();
      if (c == '\r' || c == '\n') continue; // Ignorar los caracteres de nueva línea
      buffer[index++] = c;
    }
  }
  buffer[index] = '\0'; // Terminar la cadena correctamente
}

// Función para extraer la calidad de la señal de la respuesta
int extractSignalQuality(char* response) {
  char* p = strstr(response, "+CSQ: ");
  if (p != NULL) {
    int rssi;
    if (sscanf(p, "+CSQ: %d,", &rssi) == 1) {
      return rssi;
    }
  }
  return -1; // Indica error
}


bool sendCache() {
  Serial.println("> sendCache()");
  File file = SD.open("/cache.csv");
  if (!file) { Serial.println("No se pudo abrir cache.csv"); return false; }

  char linea[256];
  while (leerLinea(file, linea, sizeof(linea))) {
    Serial.print("Linea: ");
    Serial.println(linea);

    // PARSEAR CSV 
    char *tok   = strtok(linea, ",");
    char *stamp = tok;                          // timestamp

    uint8_t col = 0;
    while (col < totalVariables - 1 && (tok = strtok(nullptr, ","))) {
      dataToSend[col++] = atof(tok);
    }
    if (col != totalVariables - 1) {                 // línea incompleta
      Serial.println("Fila malformada; se descarta");
      continue;
    }

    // CONSTRUIR URL
    char enlace[256];
    char *p = enlace;

    p += sprintf(p, "http://api-sensores.cmasccp.cl/insertarMedicion?" "times=%s", stamp);

    // idsSensores 
    p += sprintf(p, "&idsSensores=%d", idsSensores[0]);
    for (uint8_t i = 1; i < totalVariables; i++)
      p += sprintf(p, ",%d", idsSensores[i]);

    // idsVariables 
    p += sprintf(p, "&idsVariables=%d", idsVariables[0]);
    for (uint8_t i = 1; i < totalVariables; i++)
      p += sprintf(p, ",%d", idsVariables[i]);

    // valores  (0..N-1 = CSV,  N = señal) 
    p += sprintf(p, "&valores=%.2f", dataToSend[0]);
    for (uint8_t i = 1; i < totalVariables - 1; i++)      // Todas las variables, menos la Señal telefónica
      p += sprintf(p, ",%.2f", dataToSend[i]);

    p += sprintf(p, ",%d", signalValue);        // último valor = señal

    // ENVIAR  
    char at[300];
    sprintf(at, "AT+HTTPPARA=\"URL\",\"%s\"", enlace);
    Serial.print("Comando: ");
    Serial.println(at);
    comandoAT(at, "OK", 1000);
    delay(1000);
    if (comandoAT("AT+HTTPACTION=0", "\r\nOK\r\n\r\n+HTTPACTION: 0,201,66\r\n", 10000))
      Serial.println("HTTPACTION OK");
    else
      Serial.println("HTTPACTION ERROR");
    delay(1000);
    comandoAT("AT+HTTPREAD=66", "any", 1000);
    delay(000);
  }

  file.close();
  clearFile(SD, "/cache.csv");
  return true;
}

bool connectGSM(){
  if(!comandoAT("AT", "\r\nOK\r\n", 1000))
    return false;
  if(!comandoAT("AT+CREG?", "\r\n+CREG: 0,5\r\n\r\nOK\r\n", 1000))
    return false;
  if(!comandoAT("AT+CGREG?", "\r\n+CGREG: 0,5\r\n\r\nOK\r\n", 1000))
    return false;
  return true;
}

bool connectGPRS(){
  char atCommand[255];
  sprintf(atCommand, "AT+CGDCONT=1,\"IP\",\"%s\"", apn);
  if(comandoAT(atCommand, "\r\nOK\r\n", 1000))
    Serial.println("APN CHECK");
  
  if(comandoAT("AT+NETOPEN", "\r\nOK\r\n\r\n+NETOPEN: 0\r\n", 5000))
    Serial.println("AT+NETOPEN CHECK");

  if(comandoAT("AT+HTTPINIT", "\r\nOK\r\n", 5000))
    Serial.println("AT+HTTPINIT CHECK");

  return true;
}

bool closeGPRS(){
  comandoAT("AT+HTTPTERM", "OK", 1000);
  comandoAT("AT+NETCLOSE", "OK", 1000);
  return true;
}

bool comandoAT(const char* ATcommand, const char* resp_correcta, unsigned int tiempo) {
  int x = 0;
  bool correcto = false;
  char respuesta[100];
  unsigned long anterior;

  if (strcmp(resp_correcta, "any") == 0) {
    while (myModem.available() > 0)
      myModem.read();
    myModem.println(ATcommand);
    delay(100);
    while (myModem.available() > 0)
      Serial.print((char)myModem.read());
    correcto = true;
  } 
  else {
    memset(respuesta, '\0', 100);
    delay(100);
    while (myModem.available() > 0)
      myModem.read();
    myModem.println(ATcommand);
    x = 0;
    anterior = millis();
    do {
      if (myModem.available() != 0) {
        char c = myModem.read();
        if (x < sizeof(respuesta) - 1) {
          respuesta[x] = c;
          x++;
        }
        if (strstr(respuesta, resp_correcta) != NULL) {
          correcto = true;
        }
      }
    } while (!correcto && ((millis() - anterior) < tiempo));
  }

  respuesta[x] = '\0'; // Asegurarse de que la cadena esté terminada
  Serial.println(respuesta);
  return correcto;
}