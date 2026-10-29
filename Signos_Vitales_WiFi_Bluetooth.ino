#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <MAX30105.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// Completa estos datos con tu red Wi-Fi.
// Si compartiste una contraseña real, cámbiala por seguridad.
const char* ssid = "TU_NOMBRE_WIFI";
const char* password = "TU_CONTRASENA_WIFI";

WebServer server(80);

// Bluetooth BLE: estos UUID deben coincidir con los de la aplicación web.
#define SERVICE_UUID        "12345678-1234-1234-1234-1234567890ab"
#define CHARACTERISTIC_UUID "12345678-1234-1234-1234-1234567890ac"
BLECharacteristic* bleCharacteristic = nullptr;
bool bleConectado = false;
unsigned long ultimaNotificacionBLE = 0;

class ServidorCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* server) override { bleConectado = true; }
  void onDisconnect(BLEServer* server) override {
    bleConectado = false;
    BLEDevice::startAdvertising();
  }
};

// Sensores
Adafruit_MLX90614 mlx;
MAX30105 particleSensor;

const int pinHW827 = 36;

// Datos de los sensores
float tempObjeto = 0.0;
float tempAmbiente = 0.0;
long irValue = 0;
long redValue = 0;
float spO2 = 0.0;
String estadoDedo = "Pon el dedo";
int hw827Raw = 0;
float hw827Volt = 0.0;

bool mlxDisponible = false;
bool maxDisponible = false;

// Lee los datos recientes del MAX3010x.
// Se llama continuamente desde loop() para mantener actualizado el FIFO.
void leerMax30102() {
  if (!maxDisponible) return;

  particleSensor.check();

  while (particleSensor.available()) {
    irValue = particleSensor.getFIFOIR();
    redValue = particleSensor.getFIFORed();
    particleSensor.nextSample();
  }

  if (irValue < 50000) {
    estadoDedo = "Pon el dedo";
    spO2 = 0;
  } else {
    estadoDedo = "Dedo detectado";

    // Estimación muy aproximada, solo para demostración educativa.
    // No usar como medición médica.
    if (irValue > 0) {
      float ratio = (float)redValue / (float)irValue;
      spO2 = 110.0 - (25.0 * ratio);
      if (spO2 > 100.0) spO2 = 100.0;
      if (spO2 < 0.0) spO2 = 0.0;
    }
  }
}

// Página web: se envía una sola vez; los valores se actualizan con fetch().
void handleRoot() {
  String html = R"HTML(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Centro de Monitoreo ESP32</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: 'Segoe UI', Roboto, Helvetica, Arial, sans-serif;
      background: #0f172a; color: #f8fafc; min-height: 100vh;
      display: flex; flex-direction: column; align-items: center; padding: 20px;
    }
    .header { text-align: center; margin-bottom: 40px; }
    .header h1 {
      font-size: clamp(1.7rem, 4vw, 2.5rem);
      background: linear-gradient(135deg, #3b82f6, #10b981);
      -webkit-background-clip: text; -webkit-text-fill-color: transparent;
      margin-bottom: 10px;
    }
    .header p { color: #94a3b8; font-size: 1.1em; }
    .grid {
      display: grid; grid-template-columns: repeat(auto-fit, minmax(260px, 1fr));
      gap: 25px; max-width: 1200px; width: 100%;
    }
    .card {
      background: #1e293b; border: 1px solid #334155; border-radius: 20px;
      padding: 30px; box-shadow: 0 10px 25px rgba(0,0,0,.2);
      display: flex; flex-direction: column; align-items: center;
    }
    .card h2 {
      color: #94a3b8; font-size: 1.1em; margin-bottom: 20px;
      text-transform: uppercase; letter-spacing: 1px; text-align: center;
    }
    .metric { text-align: center; margin: 15px 0; width: 100%; }
    .label { font-size: 1em; color: #94a3b8; margin-bottom: 5px; display: block; }
    .value { font-size: clamp(2.2rem, 5vw, 3rem); font-weight: bold; display: block; }
    .value.sm { font-size: 1.8rem; }
    .red { color: #ef4444; } .blue { color: #3b82f6; }
    .green { color: #22c55e; } .purple { color: #a855f7; }
    .status-badge {
      display: inline-block; padding: 10px 20px; border-radius: 50px;
      font-weight: bold; margin-top: 15px; background: #334155; text-align: center;
    }
    .status-active { background: #22c55e; color: #052e16; }
    .footer { color: #94a3b8; margin-top: 24px; text-align: center; font-size: .9rem; }
  </style>
</head>
<body>
  <div class="header">
    <h1>Centro de Innovación Educativa</h1>
    <p>Monitoreo de sensores en tiempo real</p>
  </div>

  <div class="grid">
    <div class="card">
      <h2>Termómetro IR (GY-906)</h2>
      <div class="metric">
        <span class="label">Temperatura objeto</span>
        <span id="tempObjeto" class="value red">-- °C</span>
      </div>
      <div class="metric">
        <span class="label">Temperatura ambiente</span>
        <span id="tempAmbiente" class="value blue">-- °C</span>
      </div>
    </div>

    <div class="card">
      <h2>Pulsioxímetro (MAX3010x)</h2>
      <div class="metric">
        <span class="label">Estimación de SpO₂</span>
        <span id="spO2" class="value green">-- %</span>
      </div>
      <div id="estadoDedo" class="status-badge">Esperando sensor...</div>
    </div>

    <div class="card">
      <h2>Sensor HW-827</h2>
      <div class="metric">
        <span class="label">Lectura analógica (GPIO36)</span>
        <span id="hw827Raw" class="value purple">--</span>
      </div>
      <div class="metric">
        <span class="label">Voltaje equivalente</span>
        <span id="hw827Volt" class="value sm purple">-- V</span>
      </div>
    </div>
  </div>

  <div class="footer" id="estadoConexion">Conectando con el ESP32...</div>

  <script>
    async function actualizarDatos() {
      try {
        const respuesta = await fetch('/datos', { cache: 'no-store' });
        if (!respuesta.ok) throw new Error('Respuesta HTTP ' + respuesta.status);
        const d = await respuesta.json();

        document.getElementById('tempObjeto').textContent =
          Number(d.tempObjeto).toFixed(1) + ' °C';
        document.getElementById('tempAmbiente').textContent =
          Number(d.tempAmbiente).toFixed(1) + ' °C';
        document.getElementById('spO2').textContent =
          Number(d.spO2).toFixed(0) + ' %';
        document.getElementById('hw827Raw').textContent = d.hw827Raw;
        document.getElementById('hw827Volt').textContent =
          Number(d.hw827Volt).toFixed(2) + ' V';

        const estado = document.getElementById('estadoDedo');
        estado.textContent = d.estadoDedo;
        estado.classList.toggle('status-active', d.estadoDedo === 'Dedo detectado');

        document.getElementById('estadoConexion').textContent =
          'Actualizado automáticamente • ' + new Date().toLocaleTimeString();
      } catch (e) {
        document.getElementById('estadoConexion').textContent =
          'Sin comunicación con el ESP32. Reintentando...';
      }
    }

    actualizarDatos();
    setInterval(actualizarDatos, 1000);
  </script>
</body>
</html>
)HTML";

  server.send(200, "text/html; charset=utf-8", html);
}

// Actualiza sensores para que Wi-Fi y Bluetooth compartan las mismas lecturas.
void actualizarLecturas() {
  if (mlxDisponible) {
    tempAmbiente = mlx.readAmbientTempC();
    tempObjeto = mlx.readObjectTempC();
  }
  hw827Raw = analogRead(pinHW827);
  hw827Volt = hw827Raw * (3.3f / 4095.0f);
}

String crearJsonDatos() {
  String json = "{";
  json += "\"tempObjeto\":" + String(tempObjeto, 1) + ",";
  json += "\"tempAmbiente\":" + String(tempAmbiente, 1) + ",";
  json += "\"spO2\":" + String(spO2, 1) + ",";
  json += "\"irValue\":" + String(irValue) + ",";
  json += "\"estadoDedo\":\"" + estadoDedo + "\",";
  json += "\"hw827Raw\":" + String(hw827Raw) + ",";
  json += "\"hw827Volt\":" + String(hw827Volt, 2);
  json += "}";
  return json;
}

// Endpoint Wi-Fi que entrega las lecturas en formato JSON.
void handleDatos() {
  actualizarLecturas();
  server.send(200, "application/json; charset=utf-8", crearJsonDatos());
}

// Envía las mismas lecturas por Bluetooth BLE cada medio segundo.
void enviarDatosBLE() {
  if (!bleConectado || bleCharacteristic == nullptr) return;
  if (millis() - ultimaNotificacionBLE < 500) return;
  ultimaNotificacionBLE = millis();
  actualizarLecturas();
  String json = crearJsonDatos();
  // BLE suele permitir solo 20 bytes por notificación con MTU predeterminado.
  // Enviamos fragmentos cortos y terminamos cada JSON con salto de línea.
  for (size_t i = 0; i < json.length(); i += 18) {
    String fragmento = json.substring(i, i + 18);
    if (i + 18 >= json.length()) fragmento += "\n";
    bleCharacteristic->setValue(fragmento.c_str());
    bleCharacteristic->notify();
    delay(8);
  }
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  pinMode(pinHW827, INPUT);

  // GY-906 en SDA 21 / SCL 22
  Wire.begin(21, 22);
  mlxDisponible = mlx.begin();
  if (!mlxDisponible) {
    Serial.println(">>> ERROR: No se encontró el GY-906.");
  } else {
    Serial.println("GY-906 detectado correctamente.");
  }

  // MAX3010x en SDA 16 / SCL 17
  Wire1.begin(16, 17);
  maxDisponible = particleSensor.begin(Wire1, I2C_SPEED_FAST);
  if (!maxDisponible) {
    Serial.println(">>> ERROR: No se encontró el MAX3010x.");
  } else {
    Serial.println("MAX3010x detectado correctamente.");
    byte ledBrightness = 60;
    byte sampleAverage = 4;
    byte ledMode = 2;
    int sampleRate = 100;
    int pulseWidth = 411;
    int adcRange = 4096;
    particleSensor.setup(ledBrightness, sampleAverage, ledMode,
                         sampleRate, pulseWidth, adcRange);
  }

  // Bluetooth BLE: permite conectar desde la página HTTPS de GitHub Pages.
  BLEDevice::init("SignosVitalesESP32");
  BLEServer* bleServer = BLEDevice::createServer();
  bleServer->setCallbacks(new ServidorCallbacks());
  BLEService* bleService = bleServer->createService(SERVICE_UUID);
  bleCharacteristic = bleService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  bleCharacteristic->addDescriptor(new BLE2902());
  bleCharacteristic->setValue("Esperando lecturas");
  bleService->start();
  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->setScanResponse(true);
  BLEDevice::startAdvertising();
  Serial.println("Bluetooth BLE listo: SignosVitalesESP32");

  // Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi...");
  unsigned long inicioWiFi = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicioWiFi < 20000) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi conectado!");
    Serial.print("Abre en tu navegador: http://");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("No se pudo conectar al Wi-Fi. Revisa el nombre y la contraseña.");
  }

  server.on("/", handleRoot);
  server.on("/datos", handleDatos);
  server.begin();
  Serial.println("Servidor web iniciado.");
}

void loop() {
  server.handleClient();

  // Mantiene el FIFO del MAX3010x actualizado sin bloquear mucho tiempo.
  leerMax30102();
  enviarDatosBLE();
  delay(1);
}
