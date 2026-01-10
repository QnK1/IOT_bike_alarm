import paho.mqtt.client as mqtt
import time
import random

BROKER = "127.0.0.1"
PORT = 1883
TOPIC_TEMPLATE = "system_iot/user_001/esp32_sim/{sensor}"

client = mqtt.Client(client_id="pc_publisher")
client.username_pw_set("myuser", "1234")
client.connect(BROKER, PORT, 60)

sensors = ["gps", "battery", "acc"]

while True:
    for sensor in sensors:
        if sensor == "gps":
            payload = f"{round(50 + random.random(), 6)},{round(19 + random.random(), 6)}"
        elif sensor == "battery":
            payload = f"{round(3.7 + random.random() * 0.3, 2)}V"
        elif sensor == "acc":
            payload = f"x:{round(random.uniform(-1,1),2)},y:{round(random.uniform(-1,1),2)},z:{round(random.uniform(-1,1),2)}"
        
        topic = TOPIC_TEMPLATE.format(sensor=sensor)
        client.publish(topic, payload)
        print(f"Published: {topic} -> {payload}")
        time.sleep(1)
