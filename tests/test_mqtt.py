import paho.mqtt.client as mqtt
import datetime

# Adres IP brokera MQTT
BROKER = "127.0.0.1"
# Standardowy port dla nieszyfrowanej komunikacji MQTT
PORT = 1883

# Temat subskrypcji z użyciem znaków wieloznacznych (wildcards):
# '+' oznacza pojedynczy poziom w hierarchii tematu (system_iot/user_id/device_id/sensor)
TOPIC_SUBSCRIBE = "system_iot/+/+/+"

# Funkcja wywoływana po nawiązaniu połączenia z brokerem
    # client: Instancja klienta
    # userdata: Dane użytkownika (domyślnie None)
    # flags: Słownik flag odpowiedzi brokera
    # rc: Kod powrotu, 0 oznacza sukces
def on_connect(client, userdata, flags, rc):
    print(f"[Monitor PC] Connected to broker. Code: {rc}")
    # Subskrybowanie określonego tematu
    client.subscribe(TOPIC_SUBSCRIBE)
    print(f"[Monitor PC] Listening to: {TOPIC_SUBSCRIBE}")

# Funkcja wywoływana po otrzymaniu wiadomości od brokera
    # client: Instancja klienta
    # userdata: Dane użytkownika
    # msg: Obiekt MQTTMessage zawierający topic, payload, qos itd.
def on_message(client, userdata, msg):
    # Dekodowanie treści wiadomości (payload) z bajtów na string (UTF-8)
    payload = msg.payload.decode("utf-8")
    # Pobranie pełnego tematu wiadomości
    topic = msg.topic

    # Pobranie aktualnego czasu i sformatowanie go do H:M:S
    time = datetime.datetime.now().strftime("%H:%M:%S")

    # Podział tematu wiadomości na części, używając '/' jako separatora
    parts = topic.split("/")
    # Sprawdzenie, czy temat ma oczekiwaną strukturę (4 części: system_iot/user/device/sensor_type)
    if len(parts) == 4:
        # Przypisanie poszczególnych części tematu do zmiennych
        user = parts[1]        # np. 'user_001'
        device = parts[2]      # np. 'esp32_real'
        sensor_type = parts[3] # np. 'acc'
        print(f"[{time}] User: {user} | Device: {device} | {sensor_type.upper()}: {payload}")
    else:
        # Wyświetlenie wiadomości, jeśli temat nie pasuje do oczekiwanego formatu
        print(f"[{time}] {topic}: {payload}")

# Tworzenie instancji klienta MQTT z unikalnym ID
client = mqtt.Client(client_id="pc_monitor_client")

client.on_connect = on_connect
client.on_message = on_message

# Ustawienie nazwy użytkownika i hasła do uwierzytelniania na brokerze
client.username_pw_set("myuser", "1234")

# Wyświetlenie informacji o próbie połączenia
print(f"Connecting to {BROKER}...")
# Próba połączenia z brokerem, na podanym porcie, z limitem czasu 60 sekund
client.connect(BROKER, PORT, 60)

# Uruchomienie pętli sieciowej klienta. Blokuje ona bieżący wątek
# i obsługuje ciągłe nasłuchiwanie i wysyłanie pakietów MQTT.
client.loop_forever()