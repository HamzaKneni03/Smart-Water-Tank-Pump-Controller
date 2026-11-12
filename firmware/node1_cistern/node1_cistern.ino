#include <WiFi.h>
#include <esp_now.h>

/******************** HC-SR04 ********************/
#define TRIG_PIN 5
#define ECHO_PIN 18

/******************** Parameters ********************/
const float SENSOR_HEIGHT = 9;      // cm
const float TANK_HEIGHT   = 8.5;      // cm

// MAC address of Node 3 (controller)
uint8_t receiverMAC[] =
{
  0xB0,0xCB,0xD8,0x04,0x77,0x04
};

/******************** Message structure ********************/
typedef struct
{
  uint8_t nodeID;

  float distance;

  float waterHeight;

  float percentage;

  bool sensorOK;

} WaterLevelMessage;

WaterLevelMessage data;

/******************** HC-SR04 reading ********************/
float readDistance()
{
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(3);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);

  if(duration==0)
    return -1;

  return duration*0.0343/2.0;
}

/******************** Averaging filter ********************/
float filteredDistance()
{
  float sum=0;
  int valid=0;

  for(int i=0;i<5;i++)
  {
    float d=readDistance();

    if(d>0)
    {
      sum+=d;
      valid++;
    }

    delay(20);
  }

  if(valid==0)
    return -1;

  return sum/valid;
}

/******************** Send callback ********************/
void OnDataSent(const wifi_tx_info_t *info,
                esp_now_send_status_t status)
{
  Serial.print("ESP-NOW: ");

  if(status==ESP_NOW_SEND_SUCCESS)
      Serial.println("OK");

  else
      Serial.println("Error");
}

/******************** Setup ********************/
void setup()
{
  Serial.begin(115200);

  pinMode(TRIG_PIN,OUTPUT);
  pinMode(ECHO_PIN,INPUT);

  WiFi.mode(WIFI_STA);

  if(esp_now_init()!=ESP_OK)
  {
    Serial.println("ESP-NOW init error");

    while(true);
  }

  esp_now_register_send_cb(OnDataSent);

  esp_now_peer_info_t peer={};

  memcpy(peer.peer_addr,receiverMAC,6);

  peer.channel=0;

  peer.encrypt=false;

  esp_now_add_peer(&peer);

  data.nodeID=1;

  Serial.println("===== NODE 1: CISTERN =====");
}

/******************** Loop ********************/
void loop()
{
  float distance=filteredDistance();

  if(distance<0)
  {
    data.sensorOK=false;

    Serial.println("Sensor not detected");

    esp_now_send(receiverMAC,
                 (uint8_t*)&data,
                 sizeof(data));

    delay(500);

    return;
  }

  data.sensorOK=true;

  data.distance=distance;

  data.waterHeight=SENSOR_HEIGHT-distance;

  if(data.waterHeight<0)
      data.waterHeight=0;

  if(data.waterHeight>TANK_HEIGHT)
      data.waterHeight=TANK_HEIGHT;

  data.percentage=
      (data.waterHeight/TANK_HEIGHT)*100.0;

  Serial.println("---------------------------");

  Serial.print("Distance: ");
  Serial.print(data.distance);
  Serial.println(" cm");

  Serial.print("Water height: ");
  Serial.print(data.waterHeight);
  Serial.println(" cm");

  Serial.print("Fill level: ");
  Serial.print(data.percentage);
  Serial.println(" %");

  esp_now_send(receiverMAC,
               (uint8_t*)&data,
               sizeof(data));

  delay(500);
}
