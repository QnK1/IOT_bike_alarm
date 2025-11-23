import paho.mqtt.client as mqtt
import datetime

BROKER = "10.85.58.210"
PORT = 1883

TOPIC_SUBSCRIBE = "system_iot/+/+/+"

def on_connect(client, userdata, flags, rc):
    print(f"[Monitor PC] Connected to broker. Code: {rc}")
    client.subscribe(TOPIC_SUBSCRIBE)
    print(f"[Monitor PC] Listening to: {TOPIC_SUBSCRIBE}")

def on_message(client, userdata, msg):
    payload = msg.payload.decode("utf-8")
    topic = msg.topic

    time = datetime.datetime.now().strftime("%H:%M:%S")

    parts = topic.split("/")
    if len(parts) == 4:
        user = parts[1]
        device = parts[2]
        sensor_type = parts[3]
        print(f"[{time}] User: {user} | Device: {device} | {sensor_type.upper()}: {payload}")
    else:
        print(f"[{time}] {topic}: {payload}")

client = mqtt.Client(client_id="pc_monitor_client")
client.on_connect = on_connect
client.on_message = on_message

client.username_pw_set("myuser", "1234")

print(f"Connecting to {BROKER}...")
client.connect(BROKER, PORT, 60)

client.loop_forever()