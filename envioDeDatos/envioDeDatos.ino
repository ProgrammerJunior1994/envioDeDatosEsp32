#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <DHT.h>
#include <time.h>

// ========================================
// CONFIGURACION WIFI
// ========================================

const char* ssid = "INFINITUM11CE";
const char* password = "M@rc3l1n@1";

// ========================================
// CONFIGURACION AWS API GATEWAY
// ========================================

const char* serverURL =
  "https://f7t3ta0j2h.execute-api.us-east-1.amazonaws.com/etapa1/recepcionDeDatos";

// ========================================
// CONFIGURACION SENSOR DHT11
// ========================================

#define DHTPIN 4
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

// ========================================
// IDENTIFICACION DEL DISPOSITIVO
// ========================================

const char* device_id = "AGT-ESP32-001";
const char* shipment_id = "ENV-2026-001";

// ========================================
// DATOS SIMULADOS DEL GPS
// ========================================

float latitude = 19.2365;
float longitude = -104.7032;
float speed_kmh = 45.2;

// ========================================
// CONTROL DEL ENVIO
// ========================================

unsigned long previousMillis = 0;

const unsigned long interval = 10000;

// ========================================
// FUNCION: CONECTAR WIFI
// ========================================

bool conectarWiFi() {

  Serial.println();
  Serial.println("Conectando al WiFi...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  unsigned long inicio = millis();

  while (WiFi.status() != WL_CONNECTED) {

    Serial.print(".");

    delay(500);

    // Tiempo maximo de espera: 20 segundos
    if (millis() - inicio >= 20000) {

      Serial.println();
      Serial.println("Error: No se pudo conectar al WiFi.");

      return false;
    }
  }

  Serial.println();
  Serial.println("WiFi conectado");

  Serial.print("IP del ESP32: ");
  Serial.println(WiFi.localIP());

  return true;
}

// ========================================
// FUNCION: SINCRONIZAR HORA
// ========================================

bool sincronizarHora() {

  Serial.println();
  Serial.println("Sincronizando hora...");

  // Configuracion UTC
  configTime(0, 0, "pool.ntp.org", "time.google.com");

  unsigned long inicio = millis();

  while (time(nullptr) < 1700000000) {

    delay(500);

    if (millis() - inicio >= 20000) {

      Serial.println("Error: No se pudo sincronizar la hora.");

      return false;
    }
  }

  Serial.println("Hora sincronizada correctamente");

  return true;
}

// ========================================
// FUNCION: OBTENER TIMESTAMP
// ========================================

String obtenerTimestamp() {

  time_t ahora = time(nullptr);

  struct tm tiempoUTC;

  if (gmtime_r(&ahora, &tiempoUTC) == nullptr) {

    Serial.println("Error al obtener la fecha.");

    return "";
  }

  char buffer[25];

  strftime(
    buffer,
    sizeof(buffer),
    "%Y-%m-%dT%H:%M:%SZ",
    &tiempoUTC
  );

  return String(buffer);
}

// ========================================
// FUNCION: ENVIAR DATOS A AWS
// ========================================

void enviarDatos() {

  Serial.println();
  Serial.println("================================");
  Serial.println("INICIANDO ENVIO DE DATOS");
  Serial.println("================================");

  // ------------------------------------
  // 1. VERIFICAR WIFI
  // ------------------------------------

  if (WiFi.status() != WL_CONNECTED) {

    Serial.println("WiFi desconectado.");

    if (!conectarWiFi()) {
      return;
    }
  }

  Serial.println("[1] WiFi disponible.");

  // ------------------------------------
  // 2. LEER SENSOR DHT11
  // ------------------------------------

  Serial.println("[2] Leyendo temperatura...");

  float temperatura = dht.readTemperature();

  delay(100);

  Serial.println("[3] Leyendo humedad...");

  float humedad = dht.readHumidity();

  if (isnan(temperatura) || isnan(humedad)) {

    Serial.println("ERROR: No se pudo leer el DHT11.");

    Serial.println("Verifica las conexiones del sensor.");

    return;
  }

  Serial.println("Sensor DHT11 leido correctamente.");

  Serial.print("Temperatura: ");
  Serial.print(temperatura);
  Serial.println(" °C");

  Serial.print("Humedad: ");
  Serial.print(humedad);
  Serial.println(" %");

  // ------------------------------------
  // 3. OBTENER FECHA Y HORA
  // ------------------------------------

  Serial.println("[4] Obteniendo timestamp...");

  String timestamp = obtenerTimestamp();

  if (timestamp == "") {

    Serial.println("ERROR: No hay una hora valida.");

    return;
  }

  Serial.print("Timestamp: ");
  Serial.println(timestamp);

  // ------------------------------------
  // 4. CONSTRUIR JSON
  // ------------------------------------

  Serial.println("[5] Construyendo JSON...");

  String json = "{";

  json += "\"device_id\":\"";
  json += device_id;
  json += "\",";

  json += "\"shipment_id\":\"";
  json += shipment_id;
  json += "\",";

  json += "\"timestamp\":\"";
  json += timestamp;
  json += "\",";

  json += "\"temperature_c\":";
  json += String(temperatura, 1);
  json += ",";

  json += "\"humidity_pct\":";
  json += String(humedad, 1);
  json += ",";

  json += "\"latitude\":";
  json += String(latitude, 6);
  json += ",";

  json += "\"longitude\":";
  json += String(longitude, 6);
  json += ",";

  json += "\"speed_kmh\":";
  json += String(speed_kmh, 1);

  json += "}";

  Serial.println("JSON construido:");

  Serial.println(json);

  // ------------------------------------
  // 5. PREPARAR CONEXION HTTPS
  // ------------------------------------

  Serial.println("[6] Preparando conexion HTTPS...");

  WiFiClientSecure client;

  // Solo para pruebas.
  // No verifica el certificado del servidor.
  client.setInsecure();

  HTTPClient http;

  http.setConnectTimeout(10000);

  http.setTimeout(15000);

  Serial.println("[7] Iniciando conexion con API Gateway...");

  if (!http.begin(client, serverURL)) {

    Serial.println("ERROR: No se pudo iniciar HTTP.");

    return;
  }

  http.addHeader("Content-Type", "application/json");

  // ------------------------------------
  // 6. ENVIAR PETICION POST
  // ------------------------------------

  Serial.println("[8] Enviando peticion POST...");

  int httpResponseCode = http.POST(json);

  // ------------------------------------
  // 7. PROCESAR RESPUESTA
  // ------------------------------------

  if (httpResponseCode > 0) {

    Serial.println();
    Serial.println("RESPUESTA DE AWS");

    Serial.print("Codigo HTTP: ");

    Serial.println(httpResponseCode);

    String respuesta = http.getString();

    Serial.println("Respuesta del servidor:");

    Serial.println(respuesta);

  } else {

    Serial.println();

    Serial.println("ERROR AL ENVIAR LOS DATOS");

    Serial.print("Codigo de error: ");

    Serial.println(httpResponseCode);

    Serial.print("Descripcion: ");

    Serial.println(HTTPClient::errorToString(httpResponseCode));
  }

  // ------------------------------------
  // 8. CERRAR CONEXION
  // ------------------------------------

  http.end();

  Serial.println();
  Serial.println("Finalizo el proceso de envio.");

  Serial.println("================================");
}

// ========================================
// SETUP
// ========================================

void setup() {

  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println("================================");
  Serial.println("SISTEMA DE MONITOREO DE ENVIOS");
  Serial.println("ESP32 + DHT11 + AWS");
  Serial.println("================================");

  // Inicializar sensor
  Serial.println("Inicializando DHT11...");

  dht.begin();

  delay(2000);

  // Conectar WiFi
  if (!conectarWiFi()) {

    Serial.println("No se puede continuar sin WiFi.");

    return;
  }

  // Sincronizar hora
  if (!sincronizarHora()) {

    Serial.println("No se puede continuar sin hora.");

    return;
  }

  Serial.println();
  Serial.println("Sistema inicializado correctamente.");

  Serial.println("Preparado para enviar informacion.");

  // Permitir que el primer envio sea inmediato
  previousMillis = millis() - interval;
}

// ========================================
// LOOP
// ========================================

void loop() {

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {

    previousMillis = currentMillis;

    enviarDatos();
  }

  // Permitir que el ESP32 atienda otras tareas
  delay(10);
}