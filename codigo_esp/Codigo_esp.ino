#include <Wire.h>
#include <MadgwickAHRS.h>
#include <Preferences.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <DNSServer.h>
struct MPUData {
  int16_t AcX, AcY, AcZ, GyX, GyY, GyZ;
};
Preferences parametros;
DNSServer dnsServer;
WebServer webServer(80);
Madgwick filterCER;
Madgwick filterLOM;
TwoWire I2C_1 = TwoWire(0);  // I2C0
TwoWire I2C_2 = TwoWire(1);  // I2C0
#define buttonCal 4
#define led 13
#define buttonReset 2
#define vibracallCER 5
#define vibracallLOM 12
#define vibracallTOR 14
bool sentado = false;
bool calibrado = false;
// Endereço do MPU6050
const int MPUCER = 0x68;
const int MPULOM = 0x69;
// URL do endpoint para onde os dados JSON serão enviados (SUBSTITUA PELA SUA URL)
const char* serverUrl = "https://teste-maiq.onrender.com/";
// Estrutura para armazenar os dados brutos do MPU
MPUData dataCER, dataLOM;
// Variáveis convertidas
float accXCER, accYCER, accZCER;
float gyroXCER, gyroYCER, gyroZCER;
float accXLOM, accYLOM, accZLOM;
float gyroXLOM, gyroYLOM, gyroZLOM;
//---------------------------------------------------------------------------------- Avaliação da postura---------------------------------------------------------------------------------
unsigned long initErrCER = 0;
unsigned long initErrLOM = 0;
unsigned long timePosErrCER = 0;
unsigned long timePosErrLOM = 0;
unsigned long timeOfErrCER = 0;
unsigned long timeOfErrLOM = 0;
//blob
float rollCER, minVal;
float cervicalmin, cervicalmin_sit, cervicalmax, lombarmin, lombarmax;
float neutroLOM, neutroCER;
// Função para ler dados do MPU
void lerMPU(TwoWire& i2c_bus, int mpu_address, MPUData& data) {
  i2c_bus.beginTransmission(mpu_address);
  i2c_bus.write(0x3B);  // começa no registrador ACCEL_XOUT_H
  i2c_bus.endTransmission(false);
  i2c_bus.requestFrom(mpu_address, 14, true);
  data.AcX = i2c_bus.read() << 8 | i2c_bus.read();
  data.AcY = i2c_bus.read() << 8 | i2c_bus.read();
  data.AcZ = i2c_bus.read() << 8 | i2c_bus.read();
  i2c_bus.read();
  i2c_bus.read();  // temperatura (descartada)
  data.GyX = i2c_bus.read() << 8 | i2c_bus.read();
  data.GyY = i2c_bus.read() << 8 | i2c_bus.read();
  data.GyZ = i2c_bus.read() << 8 | i2c_bus.read();
}
// Função para converter dados brutos e atualizar o filtro Madgwick
void processMPUData(MPUData& data, float& accX, float& accY, float& accZ, float& gyroX, float& gyroY, float& gyroZ, Madgwick& filter) {
  accX = data.AcX / 16384.0;
  accY = data.AcY / 16384.0;
  accZ = data.AcZ / 16384.0;
  gyroX = data.GyX / 131.0;
  gyroY = data.GyY / 131.0;
  gyroZ = data.GyZ / 131.0;
  filter.updateIMU(gyroX * DEG_TO_RAD, gyroY * DEG_TO_RAD, gyroZ * DEG_TO_RAD, accX, accY, accZ);
}
// Função para inicializar o MPU6050
void inicializarMPU(TwoWire& i2c_bus, int mpu_address) {
  i2c_bus.beginTransmission(mpu_address);
  i2c_bus.write(0x6B);
  i2c_bus.write(0);
  i2c_bus.endTransmission(true);
  // Configurações adicionais para otimização
  // Configurar Sample Rate Divider para 0 (1kHz sample rate)
  i2c_bus.beginTransmission(mpu_address);
  i2c_bus.write(0x19);  // SMPLRT_DIV register
  i2c_bus.write(0x00);  // Set to 0 for 1kHz sample rate
  i2c_bus.endTransmission(true);
  // Configurar DLPF para a menor latência (maior largura de banda)
  i2c_bus.beginTransmission(mpu_address);
  i2c_bus.write(0x1A);  // CONFIG register
  i2c_bus.write(0x01);  // DLPF_CFG = 1 (accel/gyro bandwidth 188Hz/188Hz)
  i2c_bus.endTransmission(true);
}
// Função para carregar parâmetros
void carregarParametros(const char* keyMin, const char* keyMax, const char* neutroPAR, float& valMin, float& valMax, float& neutroESP) {
  valMin = parametros.getFloat(keyMin);
  valMax = parametros.getFloat(keyMax);
  neutroESP = parametros.getFloat(neutroPAR);
}
// Função para salvar parâmetros
void salvarParametrosLOM(const char* keyMin, const char* keyMax, const char* neutroPAR, float roll, float& valMin, float& valMax, float& neutroESP) {
  parametros.putFloat(keyMin, roll - 15);
  valMin = parametros.getFloat(keyMin);
  parametros.putFloat(keyMax, roll + 50);
  valMax = parametros.getFloat(keyMax);
  parametros.putFloat(neutroPAR, roll);
  neutroESP = parametros.getFloat(neutroPAR);
}
void salvarParametrosCER() {
  parametros.putFloat("cervicalmin", rollCER - 15);
  parametros.putFloat("cervicalmin_sit", rollCER - 20);
  cervicalmin = parametros.getFloat("cervicalmin");
  cervicalmin_sit = parametros.getFloat("cervicalmin");
  parametros.putFloat("cervicalmax", rollCER + 50);
  cervicalmax = parametros.getFloat("cervicalmax");
  parametros.putFloat("neutroCER", rollCER);
  neutroCER = parametros.getFloat("neutroCER");
}
unsigned long totalTimeErrCER = 0;
unsigned long totalTimeErrLOM = 0;
// Variáveis para controlar o tempo do último aviso
unsigned long lastWarningMillisCER = 0;
unsigned long lastWarningMillisLOM = 0;
// Função para avaliar a postura
void avaliarPostura(int vibracall, float roll, float minVal, float maxVal, unsigned long& initErr, unsigned long& timePosErr, unsigned long& totalTimeErr, unsigned long& lastWarningMillis, const char* message) {
  unsigned long currentMillis = millis();
  if (roll < minVal || roll > maxVal) {
    if (initErr == 0) {
      initErr = currentMillis;            // Marca o início do período de erro
      lastWarningMillis = currentMillis;  // Inicializa o tempo do último aviso
    }
    // Acumula o tempo decorrido desde a última verificação da postura errada
    totalTimeErr += (currentMillis - (initErr + timePosErr));
    timePosErr = currentMillis - initErr;  // Atualiza a duração do erro contínuo
    // Lógica de avisos
    if (timePosErr >= 20000) {  // Se a postura errada persistir por 5 minutos (300000 ms)
      if (lastWarningMillis == initErr) {
        digitalWrite(led, HIGH);
        digitalWrite(vibracall, HIGH);
        Serial.println(message);
        delay(2000);
        digitalWrite(vibracall, LOW);
        digitalWrite(led, LOW);
        lastWarningMillis = currentMillis;
      }
    }
  }
}
void avaliarPosturaCER() {
  unsigned long currentMillis = millis();
  if (sentado) {
    minVal = cervicalmin_sit;
  } else {
    minVal = cervicalmin;
  }
  if (rollCER < minVal || rollCER > cervicalmax) {
    if (initErrCER == 0) {
      initErrCER = currentMillis;            // Marca o início do período de erro
      lastWarningMillisCER = currentMillis;  // Inicializa o tempo do último aviso
    }
    // Acumula o tempo decorrido desde a última verificação da postura errada
    totalTimeErrCER += (currentMillis - (initErrCER + timePosErrCER));
    timePosErrCER = currentMillis - initErrCER;  // Atualiza a duração do erro contínuo
    // Lógica de avisos
    if (timePosErrCER >= 20000) {  // Se a postura errada persistir por 5 minutos (300000 ms)
      if (lastWarningMillisCER == initErrCER) {
        digitalWrite(led, HIGH);
        digitalWrite(vibracallCER, HIGH);
        Serial.println("pare de inclinar o pescoço");
        delay(2000);
        digitalWrite(vibracallCER, LOW);
        digitalWrite(led, LOW);
        lastWarningMillisCER = currentMillis;
      }
      if (currentMillis - lastWarningMillisCER >= 10000) {  // Aviso a cada 2 minutos ou o primeiro aviso após 5 minutos
        digitalWrite(led, HIGH);
        digitalWrite(vibracallLOM, HIGH);
        digitalWrite(vibracallCER, HIGH);
        digitalWrite(vibracallTOR, HIGH);
        Serial.println("pare de inclinar o pescoço");
        delay(2000);
        digitalWrite(led, LOW);
        digitalWrite(vibracallLOM, LOW);
        digitalWrite(vibracallTOR, LOW);
        digitalWrite(vibracallCER, LOW);
        lastWarningMillisCER = currentMillis;  // Atualiza o tempo do último aviso
      }
    }
  } else {
    // Se a postura foi corrigida, zera os contadores de erro e aviso para esta sessão
    initErrCER = 0;
    timePosErrCER = 0;
    lastWarningMillisCER = 0;
  }
}
//----------------------------------------------------------------------------------Calibração--------------------------------------------------------------------------------------
unsigned long timetoInit = 0;  // Usar unsigned long para tempo
unsigned long timePress = 0;
unsigned long timetoCal = 0;
unsigned long timeafterInit = 0;
//------------------------------------------------------------------------Reset--------------------------------------------------------------------------
unsigned long initReset = 0;
unsigned long timePressReset = 0;
const int timeToReset = 5000;
const char* pref_namespace = "wifi_conf";
const char* pref_ssid_key = "ssid";
const char* pref_pass_key = "pass";
const char* apSSID = "StraightBack";
const char* apPASS = "projete2k25";
IPAddress apIP(192, 168, 4, 1);
const byte DNS_PORT = 53;
const unsigned long TRY_INTERVAL_MS = 10UL * 60UL * 1000UL;
unsigned long lastTryMillis = 0;
bool apStarted = false;
String storedSSID = "";
String storedPASS = "";
String gerarPortalHTML() {
  int n = WiFi.scanNetworks();
  String options = "";
  if (n == 0) {
    options = "<option>Nenhuma rede encontrada</option>";
  } else {
    for (int i = 0; i < n; i++) {
      String ssid = WiFi.SSID(i);
      options += "<option value='" + ssid + "'>" + ssid + "</option>";
    }
  }
  String html = R"rawliteral(
  <!DOCTYPE html>
  <html lang="pt-BR">
  <head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Configuração Wi-Fi ESP32</title>
  <style>
    body { font-family: Arial, sans-serif; display: flex; flex-direction: column; justify-content: center; align-items: center; min-height: 100vh; background: linear-gradient(to right, #FEF9E7, #FEF9E7); color: #333; margin: 0; }
    .container { background: #FFF; padding: 25px 40px; border-radius: 12px; box-shadow: 0 6px 18px rgba(0,0,0,0.2); max-width: 400px; width: 90%; }
    h2 { text-align: center; margin-bottom: 20px; }
    label { display: block; margin-top: 12px; font-weight: bold; }
    select, input[type="password"] { width: 100%; padding: 10px; margin-top: 6px; border-radius: 6px; border: 1px solid #ccc; box-sizing: border-box; }
    input[type="submit"] { width: 100%; padding: 12px; margin-top: 18px; background: #4CAF50; color: #fff; border: none; border-radius: 6px; font-size: 16px; cursor: pointer; }
    input[type="submit"]:hover { background: #45a049; }
  </style>
  </head>
  <body>
    <div class="container">
      <h2>Configurar WiFi</h2>
      <form action="/save" method="POST">
        <label for="ssid">Rede Wi-Fi:</label>
        <select name="ssid" id="ssid">)rawliteral";
  html += options;
  html += R"rawliteral(</select>
        <label for="pass">Senha:</label>
        <input type="password" name="pass" id="pass">
        <input type="submit" value="Salvar e Conectar">
      </form>
      <p>Depois de salvar, o dispositivo tentará conectar automaticamente.</p>
    </div>
  </body>
  </html>)rawliteral";
  return html;
}
void startCaptivePortal() {
  if (apStarted) return;
  // configurar AP
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_AP_STA);  // permite AP e STA ao mesmo tempo
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  bool ok = WiFi.softAP(apSSID, apPASS);
  // iniciar DNS - redireciona tudo para apIP
  dnsServer.start(DNS_PORT, "*", apIP);
  // rotas do webserver
  webServer.on("/", HTTP_GET, []() {
    String page = gerarPortalHTML();
    webServer.send(200, "text/html", page);
  });
  webServer.on("/save", HTTP_POST, []() {
    String ssid = webServer.arg("ssid");
    String pass = webServer.arg("pass");
    if (ssid.length() > 0) {
      // salvar credenciais
      parametros.putString(pref_ssid_key, ssid);
      parametros.putString(pref_pass_key, pass);
      String resp = "<html><body><h3>Salvo! Tentando conectar a: " + ssid + "</h3><p>Feche esta página.</p></body></html>";
      webServer.send(200, "text/html", resp);
      // atualizar variáveis e tentar conectar (sem bloquear muito)
      storedSSID = ssid;
      storedPASS = pass;
      // tentativa de conexão imediata (pode levar alguns instantes)
      WiFi.begin(storedSSID.c_str(), storedPASS.c_str());
    } else {
      webServer.send(400, "text/plain", "SSID inválido");
    }
  });
  webServer.onNotFound([]() {
    // redirecionar todas requisições para root do portal
    webServer.sendHeader("Location", String("http://") + apIP.toString(), true);
    webServer.send(302, "text/plain", "");
  });
  webServer.begin();
  apStarted = true;
  lastTryMillis = millis();  // sincroniza timer
  Serial.println("Captive portal iniciado.");
  Serial.print("AP SSID: ");
  Serial.println(apSSID);
}
void stopCaptivePortal() {
  if (!apStarted) return;
  webServer.stop();
  dnsServer.stop();
  WiFi.softAPdisconnect(true);
  apStarted = false;
  Serial.println("Captive portal parado (AP desligada).");
}
void tryConnectToSavedWiFi(unsigned long timeoutMs = 10000) {
  if (storedSSID.length() == 0) return;
  Serial.print("Tentando conectar a SSID: ");
  Serial.println(storedSSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(storedSSID.c_str(), storedPASS.c_str());
  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("Conectado! IP: ");
      Serial.println(WiFi.localIP());
      return;
    }
  }
  Serial.println("Timeout de conexão (não conectado).");
}
void loadStoredCredentials() {
  storedSSID = parametros.getString(pref_ssid_key, "");
  storedPASS = parametros.getString(pref_pass_key, "");
  if (storedSSID.length()) {
    Serial.print("Credencial encontrada: ");
    Serial.println(storedSSID);
  } else {
    Serial.println("Nenhuma credencial salva.");
  }
}
void setup() {
  pinMode(vibracallLOM, OUTPUT);
  pinMode(vibracallCER, OUTPUT);
  pinMode(vibracallTOR, OUTPUT);
  pinMode(led, OUTPUT);
  pinMode(buttonCal, INPUT);
  pinMode(buttonReset, INPUT);
  Serial.begin(115200);
  Serial.println("Comecou");
  // Inicializa os canais I2C
  I2C_1.begin(21, 22, 400000);  // SDA, SCL, frequência (Hz)
  I2C_2.begin(18, 19, 400000);
  // Inicializar MPU6050
  inicializarMPU(I2C_2, MPUCER);
  inicializarMPU(I2C_1, MPULOM);
  filterCER.begin(100);
  filterLOM.begin(100);
  parametros.begin("Parametros", false);
  carregarParametros("cervicalmin", "cervicalmax", "neutroCER", cervicalmin, cervicalmax, neutroCER);
  carregarParametros("lombarmin", "lombarmax", "neutroLOM", lombarmin, lombarmax, neutroLOM);
  // Carregar totalTimeErr do NVS
  totalTimeErrCER = parametros.getULong("totalErrCER", 0);
  totalTimeErrLOM = parametros.getULong("totalErrLOM", 0);
  Serial.begin(115200);
  // Conectar ao Wi-Fi
  loadStoredCredentials();
  lastTryMillis = millis();
  if (storedSSID.length()) {
    // tenta conectar rapidamente ao ligar
    tryConnectToSavedWiFi(10000);  // timeout 10s
  }
  if (WiFi.status() == WL_CONNECTED) {
    // já conectado -> garante que AP não está ativa
    Serial.println("Conectado no boot com credenciais salvas.");
    stopCaptivePortal();
  } else {
    // não conectado -> inicia AP e captive portal
    Serial.println("Nao conectado — iniciando AP para configuração.");
    startCaptivePortal();
  }
}
void loop() {
  if (apStarted) {
    dnsServer.processNextRequest();
    webServer.handleClient();
  }
  // Checa conexão WiFi
  if (WiFi.status() == WL_CONNECTED) {
    // se estiver conectado e AP ativo, desligar AP
    if (apStarted) {
      Serial.println("Conectou ao WiFi - desligando AP.");
      stopCaptivePortal();
    }
    // aqui você pode colocar comportamento normal quando conectado
  } else {
    // se não conectado: a cada TRY_INTERVAL_MS tentar reconectar
    unsigned long now = millis();
    if ((now - lastTryMillis) >= TRY_INTERVAL_MS) {
      Serial.println("Timer expirou: tentando reconectar...");
      // recarrega credenciais (no caso de terem sido alteradas)
      loadStoredCredentials();
      if (storedSSID.length()) {
        // tenta conectar
        tryConnectToSavedWiFi(15000);  // tenta por 15s
      } else {
        Serial.println("Nenhuma credencial para tentar reconectar.");
      }
      lastTryMillis = now;
      // se ainda não conectado, garante que AP está ativo
      if (WiFi.status() != WL_CONNECTED) {
        if (!apStarted) {
          Serial.println("Ainda desconectado: ligando AP para configurar.");
          startCaptivePortal();
        }
      }
    }
  }
  unsigned long currentMillis = millis();  // Declarando currentMillis aqui
  // Carregar parâmetros (já feito no setup, mas mantido aqui para consistência com o original, se necessário)// cervicalmin = parametros.getFloat("cervicalmin");
  // cervicalmax = parametros.getFloat("cervicalmax");
  // lombarmin = parametros.getFloat("lombarmin");
  // lombarmax = parametros.getFloat("lombarmax");
  lerMPU(I2C_2, MPUCER, dataCER);
  lerMPU(I2C_1, MPULOM, dataLOM);
  processMPUData(dataCER, accXCER, accYCER, accZCER, gyroXCER, gyroYCER, gyroZCER, filterCER);
  rollCER = filterCER.getRoll();
  processMPUData(dataLOM, accXLOM, accYLOM, accZLOM, gyroXLOM, gyroYLOM, gyroZLOM, filterLOM);
  float rollLOM = filterLOM.getRoll();
  if (rollLOM <= neutroLOM - 5) {
    sentado = true;
  } else {
    sentado = false;
  }
  if (digitalRead(buttonCal) == HIGH) {
    if (timePress == 0) {
      timePress = millis();
    }
    timetoInit = millis() - timePress;
  } else if (timetoInit < 3000) {
    timePress = 0;  // Resetar timePress se o botão for solto
    timetoInit = 0;
  }
  if (timetoInit >= 3000) {
    Serial.println("------------------------------------------------Iniciou a calibração-------------------------------------");
    digitalWrite(led, HIGH);
    if (timeafterInit == 0) {
      timeafterInit = millis();
    }
    timetoCal = millis() - timeafterInit;
  } else {
    timeafterInit = 0;  // Resetar timeafterInit se o botão não for pressionado por tempo suficiente
    timetoCal = 0;
  }
  if (timetoCal > 12000) {
    digitalWrite(led, LOW);
    digitalWrite(vibracallCER, LOW);
    timetoInit = 0;
    timePress = 0;
    timetoCal = 0;
    timeafterInit = 0;
    salvarParametrosCER();
    salvarParametrosLOM("lombarmin", "lombarmax", "neutroLOM", rollLOM, lombarmin, lombarmax, neutroLOM);
    Serial.println("Calibrou-se, meu nobre.");
  }
  // timeOfErrCER = initErrCER - timePosErrCER; // Esta linha parece incorreta, initErrCER é um timestamp e timePosErrCER é uma duração
  Serial.print("Roll Cervical: ");
  Serial.println(rollCER);
  Serial.print("Cervicalmin: ");
  Serial.println(cervicalmin);
  Serial.print("Cervicalmax: ");
  Serial.println(cervicalmax);
  Serial.print("Roll Lombar: ");
  Serial.println(rollLOM);
  Serial.print("Lombarmin: ");
  Serial.println(lombarmin);
  Serial.print("Lombarmax: ");
  Serial.println(lombarmax);
  Serial.println(sentado);
  if (parametros.isKey("cervicalmin")) {
    avaliarPosturaCER();
    avaliarPostura(vibracallLOM, rollLOM, lombarmin, lombarmax, initErrLOM, timePosErrLOM, totalTimeErrLOM, lastWarningMillisLOM, "Pare de se curvar.");  // Assumindo a mesma mensagem para lombar
  } else if (!parametros.isKey("cervicalmin")) {
    Serial.println("calibre os parâmetros");
  }
  if (digitalRead(buttonReset) == HIGH) {
    if (initReset == 0) {
      initReset = millis();
    }
    timePressReset = millis() - initReset;
  } else {
    initReset = 0;  // Resetar timePress se o botão for solto
    timePressReset = 0;
  }
  if (timePressReset >= timeToReset) {
    digitalWrite(led, HIGH);
    parametros.clear();
    initReset = 0;
    timePressReset = 0;
    Serial.println("Resetou");
  }
  // Salvar totalTimeErr no NVS periodicamente ou antes de desligar
  // Para este exemplo, salvaremos a cada 10 segundos se houver mudança
  static unsigned long lastSaveMillis = 0;
  const unsigned long saveInterval = 10000;  // 10 segundos
  if (currentMillis - lastSaveMillis >= saveInterval) {
    parametros.putULong("totalErrCER", totalTimeErrCER);
    parametros.putULong("totalErrLOM", totalTimeErrLOM);
    lastSaveMillis = currentMillis;
  }
  if (parametros.isKey("cervicalmin")) {
    calibrado = true;
    Serial.println("Está calibrado");
  } else {
    calibrado = false;
    Serial.println("Não está calibrado");
    digitalWrite(led, HIGH);
    digitalWrite(vibracallCER, HIGH);
  }
  Serial.println(">>>");
  static unsigned long lastPostMillis = 0;
  const unsigned long postInterval = 60000;  // 1 min
  if (WiFi.status() == WL_CONNECTED) {
    if (currentMillis - lastPostMillis >= postInterval) {
      Serial.println("Transmitindo dados...");
      HTTPClient http;
      http.begin(serverUrl);
      http.addHeader("Content-Type", "application/json");
      StaticJsonDocument<200> doc;
      doc["cervical"] = totalTimeErrCER;
      doc["cervicalmax"] = cervicalmax;
      doc["cervicalmin"] = cervicalmin;
      doc["lombar"] = totalTimeErrLOM;
      doc["lombarmax"] = lombarmax;
      doc["lombarmin"] = lombarmin;
      doc["sentado"] = sentado;
      doc["calibrado"] = calibrado;
      String requestBody;
      serializeJson(doc, requestBody);
      int httpResponseCode = http.POST(requestBody);
      if (httpResponseCode > 0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        String response = http.getString();
        Serial.println(response);
      } else {
        Serial.print("Erro no HTTP: ");
        Serial.println(httpResponseCode);
      }
      http.end();
      lastPostMillis = currentMillis;  // atualiza o timer
    }
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi conectado.");
  } else {
    Serial.println("WiFi não conectado.");
  }
}