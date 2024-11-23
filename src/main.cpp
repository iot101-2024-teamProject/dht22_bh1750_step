#include <WiFi.h>
#include <BH1750.h>
#include <ConfigPortal32.h>
#include <PubSubClient.h>
#include <DHTesp.h>
#include <Wire.h>

#define SDA_PIN 21 // Define your custom SDA pin
#define SCL_PIN 22 // Define your custom SCL pin

// 사용자 정의 Captive Portal HTML 추가
char* ssid_pfix = (char*)"teamproject";
String user_config_html = ""
    "<p><input type='text' name='broker' placeholder='MQTT Server'>";

// MQTT 브로커 및 WiFi 설정
char mqttServer[100];
const int mqttPort = 1883;

// DHT 설정
#define DHT_PIN 15 // DHT22 센서 핀 번호
DHTesp dht;
BH1750 lightMeter;

// MQTT 클라이언트 설정
WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
unsigned long interval = 5000; // 데이터 전송 주기
char msg[50];

// 제어 명령 토픽 설정
const char* topic_control = "id/jihoon/light/control"; // 서버에서 제어 명령 받을 토픽

// MQTT 연결 함수
void reconnect() {
    while (!client.connected()) {
        Serial.print("Attempting MQTT connection...");
        if (client.connect("ESP32DHTClient")) {
            Serial.println("connected");
            client.subscribe(topic_control); // 제어 명령 토픽 구독
            Serial.printf("Subscribed to topic: %s\n", topic_control);
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            delay(5000);
        }
    }
}

// MQTT 메시지 콜백
void callback(char* topic, byte* payload, unsigned int length) {
    String message;
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }

    Serial.printf("Message received on topic %s: %s\n", topic, message.c_str());

    // "up" 또는 "down" 메시지 처리
    if (String(topic) == topic_control) {
        if (message == "up") {
            Serial.println("Received 'up' command. Taking action...");
            // TODO: "up" 명령에 대한 동작 추가
        } else if (message == "down") {
            Serial.println("Received 'down' command. Taking action...");
            // TODO: "down" 명령에 대한 동작 추가
        } else {
            Serial.printf("Unknown command received: %s\n", message.c_str());
        }
    }
}

// Captive Portal 설정 함수
void setup_wifi() {
    Serial.println("Starting Captive Portal...");
    loadConfig();
    if (!cfg.containsKey("config") || strcmp((const char*)cfg["config"], "done")) {
        configDevice();
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin((const char*)cfg["ssid"], (const char*)cfg["w_pw"]);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}

// MQTT 설정 함수
void setup_mqtt() {
    if (cfg.containsKey("broker")) {
        sprintf(mqttServer, (const char*)cfg["broker"]);
    } else {
        Serial.println("No MQTT broker configured. Check Captive Portal.");
        return;
    }

    client.setServer(mqttServer, mqttPort);
    client.setCallback(callback); // 콜백 함수 설정
}

// 초기화 함수
void setup() {
    Serial.begin(115200);

    // DHT22 초기화
    dht.setup(DHT_PIN, DHTesp::DHT22);

    // Captive Portal 및 WiFi 설정
    setup_wifi();
    setup_mqtt();

    // I2C 초기화 (SDA, SCL 핀 사용자 정의)
    Wire.begin(SDA_PIN, SCL_PIN);

    // BH1750 초기화 (명시적으로 모드와 주소 지정)
    if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x23)) { // 주소 0x23은 기본값
        Serial.println("[BH1750] Sensor initialized successfully.");
    } else {
        Serial.println("[BH1750] Sensor initialization failed. Check wiring or I2C address.");
    }
}

// 메인 루프
void loop() {
    if (!client.connected()) {
        reconnect();
    }
    client.loop();

    unsigned long now = millis();
    if (now - lastMsg > interval) {
        lastMsg = now;

        // DHT22 데이터 읽기
        float temperature = dht.getTemperature();
        float humidity = dht.getHumidity();

        if (!isnan(temperature) && !isnan(humidity)) {
            snprintf(msg, sizeof(msg), "%.2f", temperature);
            client.publish("id/jihoon/dht/temp", msg);

            snprintf(msg, sizeof(msg), "%.2f", humidity);
            client.publish("id/jihoon/dht/humi", msg);

            Serial.printf("Published -> Temperature: %.2f°C, Humidity: %.2f%%\n", temperature, humidity);
        } else {
            Serial.println("Failed to read from DHT sensor!");
        }

        // BH1750 데이터 읽기
        float lux = lightMeter.readLightLevel();
        if (!isnan(lux)) {
            snprintf(msg, sizeof(msg), "%.2f", lux);
            client.publish("id/jihoon/light/lux", msg);

            Serial.printf("Published -> Light Intensity: %.2f lux\n", lux);
        } else {
            Serial.println("Failed to read from BH1750 sensor!");
        }
    }
}
