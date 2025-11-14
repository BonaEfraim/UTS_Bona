#include <WiFi.h>
#include <PubSubClient.h>

// ====== CONFIG ======
const char* WIFI_SSID   = "331_2.4G";
const char* WIFI_PASS   = "horas123";
const char* MQTT_SERVER = "broker.emqx.io";
const char* STUDENT_ID  = "2702335244";

#define LED_PIN 4
// =====================
void mqttTask(void* pvParameters);
void ledTask(void* pvParameters);
void mqttCallback(char* topic, byte* payload, unsigned int length);
String topicControl();
// Handles
TaskHandle_t ledTaskHandle = NULL;

WiFiClient espClient;
PubSubClient mqtt(espClient);

// Create topic: "studentID/control/LED"
String topicControl() {
  return String(STUDENT_ID) + "/control/LED";
}

// ================= SETUP ==================
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);

  // Create Tasks
  xTaskCreatePinnedToCore(mqttTask, "MQTT Task", 4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(ledTask,  "LED Task",  2048, NULL, 1, &ledTaskHandle, 0);
}

void loop() {
  // not used
}


// ================= MQTT CALLBACK ==================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Convert to string
  String msg = "";
  for (int i = 0; i < length; i++) msg += (char)payload[i];

  msg.trim();
  Serial.print("[MQTT] Received: ");
  Serial.println(msg);

  // If message == ALERT → notify LED task
  if (msg == "ALERT") {
    Serial.println("[MQTT] ALERT detected → notifying LED task");
    xTaskNotify(ledTaskHandle, 1, eSetValueWithoutOverwrite);
  }
}


// ================= MQTT TASK ==================
void mqttTask(void* pvParameters) {
  mqtt.setServer(MQTT_SERVER, 1883);
  mqtt.setCallback(mqttCallback);

  // Connect WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connecting WiFi...");
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
  Serial.println("WiFi connected!");

  // Main MQTT loop
  for (;;) {
    if (!mqtt.connected()) {
      Serial.println("Connecting MQTT...");

      String clientId = "ESP32-" + String((uint32_t)esp_random(), HEX);
      if (mqtt.connect(clientId.c_str())) {
        Serial.println("MQTT connected!");
        mqtt.subscribe(topicControl().c_str());
        Serial.print("Subscribed to: ");
        Serial.println(topicControl());
      } else {
        Serial.println("MQTT connect failed");
      }
    }

    mqtt.loop();
    vTaskDelay(20 / portTICK_PERIOD_MS);
  }
}


// ================= LED TASK ==================
void ledTask(void* pvParameters) {
  for (;;) {
    uint32_t notif = 0;

    // Check for ALERT notification
    if (xTaskNotifyWait(0, 0xFFFFFFFF, &notif, 0) == pdTRUE) {

      Serial.println("[LED] ALERT received → ON for 10 seconds");

      // steady ON
      digitalWrite(LED_PIN, HIGH);
      vTaskDelay(10000 / portTICK_PERIOD_MS);

      Serial.println("[LED] ALERT done → resume blinking");
    }

    // normal blinking
    digitalWrite(LED_PIN, HIGH);
    vTaskDelay(300 / portTICK_PERIOD_MS);
    digitalWrite(LED_PIN, LOW);
    vTaskDelay(300 / portTICK_PERIOD_MS);
  }
}
