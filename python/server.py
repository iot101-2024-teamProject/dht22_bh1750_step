import paho.mqtt.client as mqtt

# MQTT 브로커 설정
server = "localhost"
topic_temp = "id/jihoon/dht/temp"  # 온도 데이터 토픽
topic_humi = "id/jihoon/dht/humi"  # 습도 데이터 토픽
topic_lux = "id/jihoon/light/lux"  # 조도 데이터 토픽
topic_control = "id/jihoon/light/control"  # 제어 명령 토픽

# MQTT 연결 콜백
def on_connect(client, userdata, flags, rc):
    print("Connected to MQTT broker with RC: " + str(rc))
    # 모든 센서 데이터 토픽 구독
    client.subscribe(topic_temp)
    client.subscribe(topic_humi)
    client.subscribe(topic_lux)

# MQTT 메시지 콜백
def on_message(client, userdata, msg):
    # 토픽별로 메시지 처리
    if msg.topic == topic_temp:
        print(f"Temperature: {msg.payload.decode('utf-8')}°C")
    elif msg.topic == topic_humi:
        print(f"Humidity: {msg.payload.decode('utf-8')}%")
    elif msg.topic == topic_lux:
        try:
            lux = float(msg.payload.decode('utf-8'))  # 조도 값 변환
            if lux <= 200:
                print(f"Light Intensity {lux} lux is below threshold. Sending 'down'.")
                client.publish(topic_control, "down")  # "down" 메시지 전송
            else:
                print(f"Light Intensity {lux} lux is above threshold. Sending 'up'.")
                client.publish(topic_control, "up")  # "up" 메시지 전송
        except ValueError:
            print(f"Invalid lux value received: {msg.payload.decode('utf-8')}")
    else:
        print(f"Unknown topic {msg.topic}: {msg.payload.decode('utf-8')}")

# MQTT 클라이언트 설정
client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

client.connect(server, 1883, 60)
client.loop_forever()
