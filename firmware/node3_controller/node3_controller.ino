/*
 * NODE 3: PUMP CONTROLLER
 * Receives the cistern (node 1) and reservoir (node 2) levels over ESP-NOW,
 * decides the pump state, drives the relay and forwards the state to the
 * display (node 4).
 *
 * The core system is fully OFFLINE (ESP-NOW only).
 * Optional add-on: set ENABLE_BLYNK to 1 for remote monitoring/control
 * through Blynk (requires WiFi + Internet and a secrets.h file).
 */
#define ENABLE_BLYNK 0   // 0 = fully offline (default), 1 = add Blynk remote monitoring

#if ENABLE_BLYNK
  // Blynk IDs must be defined before including the Blynk library
  #include "secrets.h"   // copy secrets.example.h to secrets.h
#endif

#include <WiFi.h>
#include <esp_now.h>

#if ENABLE_BLYNK
  #include <BlynkSimpleEsp32.h>
  char ssid[] = WIFI_SSID;
  char pass[] = WIFI_PASS;
#endif

/******************** RELAY ********************/
#define RELAY_PIN 26   // active LOW: LOW = pump ON

/******************** Thresholds ********************/
const float DRY_RUN_LEVEL  = 2.5;   // cm (cistern)
const float OVERFLOW_LEVEL = 8.0;   // cm (reservoir)

/******************** MAC address of Node 4 (display) ********************/
uint8_t displayMAC[] =
{
  0xEC, 0xE3, 0x34, 0xCF, 0xB5, 0xC4
};

/******************** Received message structure ********************/
typedef struct
{
  uint8_t nodeID;
  float distance;
  float waterHeight;
  float percentage;
  bool  sensorOK;
} WaterLevelMessage;

/******************** Sent message structure ********************/
typedef struct
{
  float citerneLevel;
  float reservoirLevel;
  bool  pumpState;
  bool  dryRun;
  bool  overflow;
  bool  node1Connected;
  bool  node2Connected;
} DisplayMessage;

/******************** Variables ********************/
WaterLevelMessage citerne;
WaterLevelMessage reservoir;

DisplayMessage displayData;

unsigned long lastNode1 = 0;
unsigned long lastNode2 = 0;

bool pumpState = false;
bool manualOverride = false;   // set from V3 (Pump Control), Blynk only
bool manualPumpValue = false;

#if ENABLE_BLYNK
BlynkTimer timer;
#endif

/******************** ESP-NOW receive callback ********************/
void OnDataRecv(const esp_now_recv_info_t *recvInfo,
                 const uint8_t *incomingData,
                 int len)
{
  WaterLevelMessage temp;
  memcpy(&temp, incomingData, sizeof(temp));

  if (temp.nodeID == 1)
  {
    citerne = temp;
    lastNode1 = millis();
  }

  if (temp.nodeID == 2)
  {
    reservoir = temp;
    lastNode2 = millis();
  }
}

/******************** ESP-NOW send callback ********************/
void OnDataSent(const wifi_tx_info_t *info,
                 esp_now_send_status_t status)
{
  if (status == ESP_NOW_SEND_SUCCESS)
    Serial.println("State sent to Node 4");
  else
    Serial.println("Error sending to Node 4");
}

#if ENABLE_BLYNK
/******************** Blynk: Pump Control button (V3) ********************/
BLYNK_WRITE(V3)
{
  manualOverride  = true;
  manualPumpValue = param.asInt();
}

/******************** Periodic upload to Blynk (every 2 s) ********************/
void sendToBlynk()
{
  Blynk.virtualWrite(V0, citerne.waterHeight);
  Blynk.virtualWrite(V1, reservoir.waterHeight);
  Blynk.virtualWrite(V2, pumpState);
  Blynk.virtualWrite(V4, displayData.dryRun);
  Blynk.virtualWrite(V5, displayData.overflow);
}
#endif

/******************** Setup ********************/
void setup()
{
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);   // pump OFF at boot

  WiFi.mode(WIFI_STA);   // required before ESP-NOW

#if ENABLE_BLYNK
  // Connects to WiFi, then to the Blynk server
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  Serial.print("WiFi channel in use: ");
  Serial.println(WiFi.channel());
#endif

  if (esp_now_init() != ESP_OK)
  {
    Serial.println("ESP-NOW init error");
    while (true);
  }

  esp_now_register_recv_cb(OnDataRecv);
  esp_now_register_send_cb(OnDataSent);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, displayMAC, 6);
  peer.channel = 0;
  peer.encrypt = false;
  esp_now_add_peer(&peer);

#if ENABLE_BLYNK
  timer.setInterval(2000L, sendToBlynk);
#endif

  Serial.println();
  Serial.println("===== NODE 3: CONTROLLER =====");
}

/******************** Main loop ********************/
void loop()
{
#if ENABLE_BLYNK
  Blynk.run();
  timer.run();
#endif

  /************* Link supervision *************/
  displayData.node1Connected = (millis() - lastNode1) < 3000;
  displayData.node2Connected = (millis() - lastNode2) < 3000;

  /************* Decision *************/
  if (manualOverride)
  {
    // manual command from the Blynk app has priority
    pumpState = manualPumpValue;
  }
  else if (!displayData.node1Connected || !displayData.node2Connected)
  {
    pumpState = false;
  }
  else if (!citerne.sensorOK || !reservoir.sensorOK)
  {
    pumpState = false;
  }
  else if (citerne.waterHeight <= DRY_RUN_LEVEL)
  {
    pumpState = false;
  }
  else if (reservoir.waterHeight >= OVERFLOW_LEVEL)
  {
    pumpState = false;
  }
  else
  {
    pumpState = true;
  }

  /************* Relay command *************/
  if (pumpState)
    digitalWrite(RELAY_PIN, LOW);   // relay ON
  else
    digitalWrite(RELAY_PIN, HIGH);  // relay OFF

  /************* Display data *************/
  displayData.citerneLevel   = citerne.waterHeight;
  displayData.reservoirLevel = reservoir.waterHeight;
  displayData.pumpState      = pumpState;
  displayData.dryRun         = (citerne.waterHeight <= DRY_RUN_LEVEL);
  displayData.overflow       = (reservoir.waterHeight >= OVERFLOW_LEVEL);

  /************* Send to Node 4 *************/
  esp_now_send(displayMAC, (uint8_t*)&displayData, sizeof(displayData));

  /************* Serial output *************/
  Serial.println();
  Serial.println("==============================");

  Serial.print("Cistern: ");
  Serial.print(citerne.waterHeight);
  Serial.println(" cm");

  Serial.print("Reservoir: ");
  Serial.print(reservoir.waterHeight);
  Serial.println(" cm");

  Serial.print("Pump: ");
  Serial.println(pumpState ? "ON" : "OFF");

  Serial.print("Node 1: ");
  Serial.println(displayData.node1Connected ? "OK" : "ERROR");

  Serial.print("Node 2: ");
  Serial.println(displayData.node2Connected ? "OK" : "ERROR");

  if (displayData.dryRun)
    Serial.println("ALERT: Dry run (cistern low)");

  if (displayData.overflow)
    Serial.println("ALERT: Reservoir full");

  Serial.println("==============================");

  delay(500);
}
