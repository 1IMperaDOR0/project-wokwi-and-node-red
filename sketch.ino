#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

#define DHTPIN 2
#define trigger 16
#define echo 4
#define led 17

DHT dht(DHTPIN, DHT22);

long getDistancia() {
  digitalWrite(trigger, LOW);
  delayMicroseconds(2);
  digitalWrite(trigger, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigger, LOW);

  long duracao = pulseIn(echo, HIGH);
  long distancia = duracao * 0.034 / 2;

  return distancia;
}


long primeiraDistancia, segundaDistancia;
unsigned long primeiroTempo, segundoTempo;
float v;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

void setup() {
  Serial.begin(9600);

  pinMode(trigger, OUTPUT);
  pinMode(echo, INPUT);
  pinMode(led, OUTPUT);

  dht.begin();

  WiFi.begin("Wokwi-GUEST", "");
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }
  Serial.println("\nWiFi Conectado!");

  mqttClient.setServer("test.mosquitto.org", 1883);
  mqttClient.setCallback([](char* topic, byte* payload, unsigned int length) {
    if (payload[0] == '1') {
      digitalWrite(led, HIGH);
      Serial.println("\nSISTEMA LIGADO");
    }
    if (payload[0] == '0') {
      digitalWrite(led, LOW);
      Serial.println("\nSISTEMA DESLIGADO");
    }
  });

  while (!mqttClient.connect("esp32-rafaelts")) {
    Serial.println("Conectando-se ao MQTT...");
    delay(1000);
  }

  Serial.println("MQTT conectado!");
  mqttClient.subscribe("led/control/rafaelts");

  Serial.println("Monitor de Velocidade Iniciado");
}

void loop() {
  if (!mqttClient.connected()) {
    mqttClient.connect("esp32-rafaelts");
  }

  mqttClient.loop();

  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (isnan(t) || isnan(h)) {
    Serial.println("Erro ao ler o sensor DHT!");
    return;
  }

  if (t > 40) {
    Serial.println("Temperatura inadequada para jogo");
  }

  if (h < 20) {
    Serial.println("Umidade inadequada para jogo");
  }

  primeiraDistancia = getDistancia();
  primeiroTempo = millis();

  delay(250);

  segundaDistancia = getDistancia();
  segundoTempo = millis();

  float deltaDistancia = abs(primeiraDistancia - segundaDistancia);
  float deltaTempo = (segundoTempo - primeiroTempo) / 1000.0;

  if (deltaTempo > 0) {
    v = deltaDistancia / deltaTempo;
  } else {
    v = 0;
  }

  Serial.println("--------------------");
  Serial.print("Distancia1: ");
  Serial.print(primeiraDistancia);
  Serial.println(" cm");

  Serial.print("Distancia2: ");
  Serial.print(segundaDistancia);
  Serial.println(" cm");

  Serial.print("Tempo: ");
  Serial.print(deltaTempo, 2);
  Serial.println(" s");

  Serial.print("Velocidade: ");
  Serial.print(v, 1);
  Serial.println(" cm/s");

  // Publicar no MQTT
  StaticJsonDocument<200> json;
  json["Temperatura"] = t;
  json["Umidade"] = h;
  json["Velocidade"] = v;
  json["Distancia1"] = primeiraDistancia;
  json["Distancia2"] = segundaDistancia;
  json["Tempo"] = deltaTempo;

  char jsonBuffer[200];
  serializeJson(json, jsonBuffer);

  mqttClient.publish("sensor/dht/rafaelts", jsonBuffer);

  Serial.println("JSON publicado no MQTT!!!");
}