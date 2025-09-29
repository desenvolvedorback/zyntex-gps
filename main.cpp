/* RASTREADOR - Arduino Mega 2560
   - NEO-M8N on Serial1 (RX1/TX1)
   - SIM800L on Serial2 (RX2/TX2)
   - BMP388 via I2C (SDA/A4, SCL/A5)
   - Envia a cada 1s se fix GNSS
   Requer libs:
     TinyGPSPlus
     AESLib (https://github.com/DavyLandman/AESLib)  (ou TinyAES substitute)
     Base64 (ArduinoBase64)
     Adafruit_BMP3XX (ou Adafruit_BMP388)
*/

#include <TinyGPSPlus.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP3XX.h>
#include <AESLib.h>

arduino-cli lib install "ArduinoBase64"


#define DEVICE_SERIAL "ABC123456789"  // muda para o serial do teu dispositivo
#define APN "your.apn.here"           // muda para o APN da operadora
#define APN_USER ""                   // se necessário
#define APN_PASS ""                   // se necessário

// AES key (16 bytes). Troque por chave segura. Exemplo simples em ASCII (16 chars).
const char *AES_KEY = "16chaveaesexemplo"; // 16 chars

// Buffers
TinyGPSPlus gps;
Adafruit_BMP3XX bmp;

AESLib aesLib;

unsigned long lastSendMs = 0;
const unsigned long SEND_INTERVAL_MS = 1000; // enviar a cada 1 segundo

// Serial mapping:
// Serial1 <-> GNSS (pins 19(RX1)/18(TX1))
// Serial2 <-> SIM800L (pins 17(RX2)/16(TX2))

// Helpers para SIM800L
Stream *simSerial = &Serial2;

void simSendCmd(const char* cmd, unsigned long waitMs = 1000) {
  simSerial->println(cmd);
  delay(waitMs);
}

String simReadResponse(unsigned long timeoutMs = 2000) {
  String res = "";
  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    while (simSerial->available()) {
      char c = simSerial->read();
      res += c;
    }
  }
  return res;
}

bool initSIM800() {
  // Assumes SIM powered and with SIM inserted.
  simSerial->begin(9600);
  delay(100);
  // Basic AT check
  simSerial->println("AT");
  delay(500);
  if (simSerial->available()) {
    String r = simReadResponse(1000);
    if (r.indexOf("OK") >= 0) {
      // set echo off
      simSendCmd("ATE0", 200);
      return true;
    }
  }
  return false;
}

bool setupGPRS() {
  // set bearer profile for GPRS
  simSendCmd("AT+SAPBR=3,1,\"Contype\",\"GPRS\"", 500);
  String cmdApn = String("AT+SAPBR=3,1,\"APN\",\"") + APN + "\"";
  simSendCmd(cmdApn.c_str(), 800);
  if (String(APN_USER).length()) {
    String cmdUser = String("AT+SAPBR=3,1,\"USER\",\"") + APN_USER + "\"";
    simSendCmd(cmdUser.c_str(), 500);
    String cmdPass = String("AT+SAPBR=3,1,\"PWD\",\"") + APN_PASS + "\"";
    simSendCmd(cmdPass.c_str(), 500);
  }
  // open bearer
  simSendCmd("AT+SAPBR=1,1", 2000);
  // query
  simSendCmd("AT+SAPBR=2,1", 1000);
  String r = simReadResponse(2000);
  if (r.indexOf("SAPBR") >= 0) return true;
  return false;
}

bool httpPost(const char* url, const String &body, String &responseOut) {
  // Use SIM800L HTTP commands
  simSendCmd("AT+HTTPTERM", 200);
  simSendCmd("AT+HTTPINIT", 500);

  // set CID 1
  simSendCmd("AT+HTTPPARA=\"CID\",1", 200);
  // set URL
  String urlCmd = String("AT+HTTPPARA=\"URL\",\"") + url + "\"";
  simSendCmd(urlCmd.c_str(), 200);
  // set content type
  simSendCmd("AT+HTTPPARA=\"CONTENT\",\"application/json\"", 200);

  // send data
  String lenCmd = String("AT+HTTPDATA=") + String(body.length()) + ",10000";
  simSendCmd(lenCmd.c_str(), 200);
  delay(200);
  simSerial->print(body);
  delay(1000);

  // execute POST
  simSendCmd("AT+HTTPACTION=1", 1000);
  // wait and read response
  delay(2000);
  String r = simReadResponse(3000);
  responseOut = r;

  // read the result body (optional)
  simSendCmd("AT+HTTPREAD", 500);
  String readr = simReadResponse(2000);

  // cleanup
  simSendCmd("AT+HTTPTERM", 200);
  return (r.indexOf("+HTTPACTION: 1,200") >= 0) || (r.indexOf(",200,") >= 0);
}

// AES helpers (AES-128-CBC)
void aesEncrypt(const uint8_t *in, int inLen, uint8_t *out, byte iv[16]) {
  // aesLib.encryptBufferCBC requires key and iv as bytes
  byte key[16];
  memcpy(key, AES_KEY, 16);
  aesLib.gen_iv(iv); // gera IV aleatório
  aesLib.encryptCBC(in, inLen, out, key, 16, iv);
}

// Convert binary to base64 (Arduino Base64 lib)
String toBase64(uint8_t *buf, int len) {
  int encodedLen = base64_enc_len(len);
  char encoded[encodedLen + 1];
  base64_encode(encoded, (char*)buf, len);
  encoded[encodedLen] = 0;
  return String(encoded);
}

void setup() {
  Serial.begin(115200); // debug
  Serial1.begin(9600);  // GNSS
  Serial2.begin(9600);  // SIM800L
  Wire.begin();
  delay(500);

  Serial.println("Iniciando BMP...");
  if (!bmp.begin_I2C()) {
    Serial.println("BMP não encontrado. Verifique fiação.");
  } else {
    bmp.setTemperatureOversampling(BMP3_OVERSAMPLING_8X);
    bmp.setPressureOversampling(BMP3_OVERSAMPLING_4X);
    bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3);
  }

  // init SIM
  Serial.println("Inicializando SIM800L...");
  if (!initSIM800()) {
    Serial.println("Falha init SIM800L. Verifique energia e SIM.");
  } else {
    Serial.println("SIM inicializado.");
  }
  Serial.println("Configurar GPRS...");
  if (!setupGPRS()) {
    Serial.println("Falha GPRS. Checar APN.");
  } else {
    Serial.println("GPRS pronto.");
  }

  lastSendMs = millis();
}

void loop() {
  // Ler dados GNSS continuamente
  while (Serial1.available()) {
    char c = Serial1.read();
    gps.encode(c);
  }

  // leitura do BMP
  float temperature = NAN;
  float pressure = NAN;
  if (bmp.performReading()) {
    temperature = bmp.temperature;
    pressure = bmp.pressure;
  }

  unsigned long now = millis();
  if (now - lastSendMs >= SEND_INTERVAL_MS) {
    lastSendMs = now;

    if (gps.location.isValid()) {
      double lat = gps.location.lat();
      double lng = gps.location.lng();
      double alt = gps.altitude.meters();
      double hdop = gps.hdop.hdop();
      String utc = String(gps.time.hour()) + ":" + String(gps.time.minute()) + ":" + String(gps.time.second());
      // montar JSON
      String json = "{";
      json += "\"device\":\"" + String(DEVICE_SERIAL) + "\",";
      json += "\"lat\":" + String(lat, 8) + ",";
      json += "\"lon\":" + String(lng, 8) + ",";
      json += "\"alt\":" + String(alt, 2) + ",";
      json += "\"hdop\":" + String(hdop, 2) + ",";
      json += "\"time\":\"" + utc + "\",";
      if (!isnan(pressure)) {
        json += "\"pressure\":" + String(pressure, 2) + ",";
      } else {
        json += "\"pressure\":null,";
      }
      if (!isnan(temperature)) {
        json += "\"temp\":" + String(temperature, 2) + ",";
      } else {
        json += "\"temp\":null,";
      }
      json += "\"battery\":null"; // opcional: se tiver medição de bateria, colocar
      json += "}";

      Serial.print("JSON claro: ");
      Serial.println(json);

      // encriptar JSON com AES-CBC
      // Preparar blocos (padding PKCS7)
      int blockSize = 16;
      int inLen = json.length();
      int paddedLen = ((inLen / blockSize) + 1) * blockSize;
      uint8_t plain[paddedLen];
      memset(plain, 0, paddedLen);
      memcpy(plain, json.c_str(), inLen);
      // PKCS7 padding
      uint8_t pad = paddedLen - inLen;
      for (int i = inLen; i < paddedLen; i++) plain[i] = pad;

      uint8_t cipher[paddedLen];
      byte iv[16];
      aesEncrypt(plain, paddedLen, cipher, iv);

      // base64
      String b64_cipher = toBase64(cipher, paddedLen);

      // iv em base64
      String b64_iv = toBase64(iv, 16);

      // montar corpo final (JSON)
      String outJson = "{\"payload\":\"" + b64_cipher + "\",\"iv\":\"" + b64_iv + "\"}";

      // Enviar via SIM800L HTTP POST para o servidor
      // Muda URL para o do teu servidor (IP público ou domínio)
      const char* serverUrl = "http://SEU_SERVIDOR/public/receive"; // ALTERE AQUI
      String resp;
      bool ok = httpPost(serverUrl, outJson, resp);
      if (ok) {
        Serial.println("POST enviado com sucesso.");
      } else {
        Serial.println("Falha ao enviar POST:");
        Serial.println(resp);
      }
    } else {
      Serial.println("Sem fix GNSS ainda...");
    }
  }
}
