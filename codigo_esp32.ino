#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// pinos dos atuadores
const int pinoLED = 12;
const int pinoBuzzer = 19;

const char* ssid = "Wokwi-GUEST";
const char* password = "";

const char* mqtt_server = "broker.hivemq.com";
const char* topico_alerta = "mackenzie/auracare/emergencia_seu_nome";

WiFiClient espClient;
PubSubClient client(espClient);
Adafruit_MPU6050 mpu;

void setup_wifi() {
  delay(10);
  Serial.println("\nConectando a rede Wi-Fi...");

  WiFi.disconnect(true);
  delay(1000);
  WiFi.mode(WIFI_STA);

  WiFi.begin("Wokwi-GUEST", "", 6);

  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    tentativas++;
    if(tentativas > 20) {
       Serial.println("\nFalha ao conectar. Tentando conectar novamente...");
       WiFi.disconnect(true);
       delay(1000);
       WiFi.begin("Wokwi-GUEST", "", 6);
       tentativas = 0;
    }
  }

  Serial.println("\nWiFi conectado com sucesso!");
  Serial.print("IP recebido: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Tentando conexao com Broker MQTT HiveMQ...");
    String clientId = "AuraCare-ESP32-";
    clientId += String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str())) {
      Serial.println("Conectado ao HiveMQ!");
    } else {
      Serial.print("Falhou, codigo de erro = ");
      Serial.print(client.state());
      Serial.println(" Tentando novamente em 5 segundos...");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  
  // configuração dos pinos
  pinMode(pinoLED, OUTPUT);
  pinMode(pinoBuzzer, OUTPUT);
  digitalWrite(pinoLED, LOW);
  digitalWrite(pinoBuzzer, LOW);

  setup_wifi();
  client.setServer(mqtt_server, 1883);

  if (!mpu.begin()) {
    Serial.println("Falha ao encontrar o sensor MPU6050!");
    while (1) {
      delay(10);
    }
  }
  Serial.println("Sensor MPU6050 inicializado.");
  
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // receber os dados do sensor
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // calcula a magnitude total da aceleração (o normal em repouso é ~9.8 m/s²)
  float aceleracaoTotal = sqrt(pow(a.acceleration.x, 2) + pow(a.acceleration.y, 2) + pow(a.acceleration.z, 2));

  // lógica de detecção de queda (impacto brusco > 15 m/s² + Giro brusco > 2 rad/s)
  if (aceleracaoTotal > 15.0 && (abs(g.gyro.x) > 2.0 || abs(g.gyro.y) > 2.0 || abs(g.gyro.z) > 2.0)) {
    Serial.println("\n*** ALERTA: QUEDA DETECTADA! ***");
    
    // aciona os atuadores locais
    digitalWrite(pinoLED, HIGH);
    tone(pinoBuzzer, 1000); // emite som no buzzer a 1000Hz
    
    // envia mensagem MQTT para a rede de socorro
    String payload = "{\"status\": \"emergencia\", \"mensagem\": \"Queda detectada! Necessario socorro imediato!\"}";
    client.publish(topico_alerta, payload.c_str());
    Serial.println("Mensagem MQTT publicada: " + payload);
    
    // o alarme toca por 4 segundos
    delay(4000); 
    
    // desliga o alarme para aguardar novo evento
    digitalWrite(pinoLED, LOW);
    noTone(pinoBuzzer);
    Serial.println("Sistema normalizado. Monitorando...");
  }

  delay(100);
}
