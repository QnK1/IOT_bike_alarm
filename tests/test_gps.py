import paho.mqtt.client as mqtt

BROKER = "127.0.0.1"
PORT = 1883
TOPIC_TEMPLATE = "system_iot/user_001/esp32_sim/{sensor}"

def publish_fake_gps():
    client = mqtt.Client()
    client = mqtt.Client(client_id="pc_publisher")
    client.username_pw_set("myuser", "1234")
    client.connect(BROKER, PORT, 60)
    
    while True:
        payload = '{"lat": 52.229675, "lon": 21.012230, "sats": 8}'
        client.publish("system_iot/user_001/esp32/gps", payload)
        print("Sent:", payload)
        import time
        time.sleep(5)

publish_fake_gps()
