#include <WiFi.h>
#include <BH1750.h>
#include <ConfigPortal32.h>
#include <PubSubClient.h>
#include <DHTesp.h>
#include <Wire.h>
#include <AccelStepper.h>

char* ssid_pfix = (char*)"teamproject";
String user_config_html = ""
    "<p><input type='text' name='broker' placeholder='MQTT Server'>";

char mqttServer[100];
const int mqttPort = 1883;

// 핀 설정
#define SDA_PIN 21 // I2C SDA 핀
#define SCL_PIN 22 // I2C SCL 핀
#define DHT_PIN 15 // DHT22 센서 핀 번호
#define IN1 16
#define IN2 17
#define IN3 5
#define IN4 18

// 스텝모터 설정
AccelStepper MyStepper(AccelStepper::FULL4WIRE, IN1, IN2, IN3, IN4);
volatile bool stepperEnabled = false; // 모터 동작 상태
volatile int targetSteps = 0;         // 목표 스텝 수

// 센서 및 MQTT 설정
DHTesp dht;
BH1750 lightMeter;
WiFiClient wifiClient;
PubSubClient client(wifiClient);
void msgCB(char* topic, byte* payload, unsigned int length); // message Callback
void pubStatus(float lux, float temperature, float humidity);
unsigned long interval = 5000; // 데이터 전송 주기
unsigned long lastPublished = -interval;

// MQTT 토픽
const char* topic_temp = "id/semicon/livingroom/dht22/temperature";         // 온도 데이터 토픽
const char* topic_humi = "id/semicon/livingroom/dht22/humidity";         // 습도 데이터 토픽
const char* topic_lux = "id/semicon/livingroom/bh1750/lux";         // 조도 데이터 토픽
const char* topic_control = "id/semicon/livingroom/step/control"; // 스텝모터 제어 토픽

void setup() { 
    delay(3000);
    Serial.begin(115200);
    Wire.begin(SDA_PIN, SCL_PIN); // I2C 초기화


    // DHT22 초기화
    
    
    dht.setup(DHT_PIN, DHTesp::DHT22);

    // BH1750 초기화
    if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x23)) {
        Serial.println("[BH1750] Sensor initialized successfully.");
    } else {
        Serial.println("[BH1750] Sensor initialization failed.");
    }
    
    // 스텝모터 설정
    pinMode(IN1, OUTPUT);
    pinMode(IN2, OUTPUT);
    pinMode(IN3, OUTPUT);
    pinMode(IN4, OUTPUT);
    MyStepper.setMaxSpeed(200); // 최대 속도 설정
    MyStepper.setAcceleration(500); // 가속도 설정

    // Captive Portal 및 WiFi 설정
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
    Serial.printf("\nWiFi connected! IP: %s\n", WiFi.localIP().toString().c_str());

    // MQTT 설정
    if (cfg.containsKey("broker")) {
    sprintf(mqttServer, (const char*)cfg["broker"]);
    Serial.printf("Broker Address: %s\n", mqttServer); // 로드된 브로커 주소 확인
    }
    client.setServer(mqttServer, mqttPort);
    client.setCallback(msgCB);

    // MQTT 연결
    while (!client.connected()) {
        Serial.println("Connecting to MQTT...");
        if (client.connect("ESP32DHTClient")) {
            Serial.println("Connected to MQTT broker");
            client.subscribe(topic_control);
        } else {
            Serial.printf("Failed to connect to MQTT broker, state: %d\n", client.state());
            delay(2000);
        }
    }
}

void loop() {
    client.loop();

    // 스텝 모터 실행
    if (stepperEnabled && targetSteps != 0) {
        if (MyStepper.distanceToGo() == 0) {
            stepperEnabled = false; // 목표 도달 시 모터 비활성화
        } else {
            MyStepper.run(); //모터 동작
        }
        }

    unsigned long now = millis();
    if (now - lastPublished >= interval) {
        lastPublished = now;

        // DHT22 데이터 읽기 및 전송
        float temperature = dht.getTemperature();
        float humidity = dht.getHumidity();

        if (!isnan(temperature) && !isnan(humidity)) {
            char msg[10];
            snprintf(msg, sizeof(msg), "%.2f", temperature);
            client.publish(topic_temp, msg);
            snprintf(msg, sizeof(msg), "%.2f", humidity);
            client.publish(topic_humi, msg);
            Serial.printf("Temperature: %.2f°C, Humidity: %.2f%%\n", temperature, humidity);
        } else {
            Serial.println("Failed to read DHT sensor!");
        }

        // BH1750 데이터 읽기 및 전송
        float lux = lightMeter.readLightLevel();
        if (!isnan(lux)) {
            char msg[10];
            snprintf(msg, sizeof(msg), "%.2f", lux);
            client.publish(topic_lux, msg);
            Serial.printf("Light Intensity: %.2f lux\n", lux);
        } else {
            Serial.println("Failed to read BH1750 sensor!");
        }

        
    }
}

void pubStatus(float lux, float temperature, float humidity) {
    char luxStr[10];       // 조도 데이터를 문자열로 저장
    char tempStr[10];      // 온도 데이터를 문자열로 저장
    char humiStr[10];      // 습도 데이터를 문자열로 저장

    // 조도 값 처리
    if (!isnan(lux)) {
        sprintf(luxStr, "%.2f", lux);
    }

    // 온도 값 처리
    if (!isnan(temperature)) {
        sprintf(tempStr, "%.2f", temperature); // 온도 값을 문자열로 변환
    } else {
        sprintf(tempStr, "NaN"); // NaN 처리
    }

    // 습도 값 처리
    if (!isnan(humidity)) {
        sprintf(humiStr, "%.2f", humidity); // 습도 값을 문자열로 변환
    } else {
        sprintf(humiStr, "NaN"); // NaN 처리
    }

    // MQTT 전송
    client.publish(topic_temp, tempStr);      // 온도 값 전송
    client.publish(topic_humi, humiStr);      // 습도 값 전송
    client.publish(topic_lux, luxStr);      // 조도 값 전송

    // 디버그 메시지
    Serial.printf("Published -> Lux: %s, Temperature: %s, Humidity: %s\n", luxStr, tempStr, humiStr);
}
int stepSequence[4][4] = {
  {1, 0, 0, 0},
  {0, 1, 0, 0},
  {0, 0, 1, 0},
  {0, 0, 0, 1}
};
int currentStep = 0;

void stepMotorRotate_right(bool clockwise, int steps) {
  for (int i = 0; i < steps; i++) {
    if (clockwise) {
      currentStep = (currentStep + 1) % 4;
    } else {
      currentStep = (currentStep - 1 + 4) % 4;
    }
    
    digitalWrite(IN1, stepSequence[currentStep][0]);
    digitalWrite(IN2, stepSequence[currentStep][1]);
    digitalWrite(IN3, stepSequence[currentStep][2]);
    digitalWrite(IN4, stepSequence[currentStep][3]);
    
    delay(3); // 각 스텝 사이 딜레이
    client.loop(); // MQTT 통신 유지
  }
}

void stepMotorRotate_left(bool clockwise, int steps) {
  for (int i = 0; i < steps; i++) {
    if (clockwise) {
      currentStep = (currentStep + 1) % 4;
    } else {
      currentStep = (currentStep - 1 + 4) % 4;
    }
    
    digitalWrite(IN1, stepSequence[currentStep][3]);
    digitalWrite(IN2, stepSequence[currentStep][2]);
    digitalWrite(IN3, stepSequence[currentStep][1]);
    digitalWrite(IN4, stepSequence[currentStep][0]);
    
    delay(5); // 각 스텝 사이 딜레이
    client.loop(); // MQTT 통신 유지
  }
}
void msgCB(char* topic, byte* payload, unsigned int length) {
    char msgBuffer[20];
    strncpy(msgBuffer, (char*)payload, length);
    msgBuffer[length] = '\0';

    Serial.printf("Message received -> %s: %s\n", topic, msgBuffer);

    // Lux 기반 스텝모터 제어 처리
    if (strcmp(topic, "id/semicon/livingroom/step/control") == 0) {
        if (strcmp(msgBuffer, "up") == 0) {
            Serial.println("Lux Command: Moving stepper forward 5 revolutions (lux >= threshold)");
            stepMotorRotate_right(true, 10240);  // 시계방향 5바퀴
        
        } else if (strcmp(msgBuffer, "down") == 0) {
            Serial.println("Lux Command: Moving stepper backward 5 revolutions (lux < threshold)");
            stepMotorRotate_left(true, 10240);  // 반시계방향 5바퀴
        }
    }
}

