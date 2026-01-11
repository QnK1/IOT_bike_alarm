import paho.mqtt.client as mqtt
import time

def on_publish(client, userdata, mid):
    print("Message Published!")

BROKER = "10.85.218.163"
PORT = 1883

client = mqtt.Client(client_id="pc_publisher")
client.username_pw_set("myuser", "1234")
client.on_publish = on_publish # Assign the callback

client.connect(BROKER, PORT, 60)
client.loop_start() # Starts a background thread for network traffic

msg_info = client.publish("system_iot/user_001/esp32/cmd", "DISARM", qos=1)

# Wait specifically for this message to be published
msg_info.wait_for_publish()

client.loop_stop()
client.disconnect()