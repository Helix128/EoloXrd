class Modem{
public:
  Modem();
  void sendData();
private:
  int signalQuality();
  void readResponse(char* buffer, int maxLength);
  int extractSignalQuality(char* response);
  bool sendCache();
  bool connectGSM();
  bool connectGPRS();
  bool closeGPRS();
  bool comandoAT(const char* ATcommand, const char* resp_correcta, unsigned int tiempo);
};