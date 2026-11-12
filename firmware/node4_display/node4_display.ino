#include <WiFi.h>
#include <esp_now.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

/******************** OLED ********************/
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_SDA 21
#define OLED_SCL 22

Adafruit_SSD1306 display(SCREEN_WIDTH,
                         SCREEN_HEIGHT,
                         &Wire,
                         -1);


/******************** Received message structure ********************/
typedef struct
{
  float citerneLevel;

  float reservoirLevel;

  bool pumpState;

  bool dryRun;

  bool overflow;

  bool node1Connected;

  bool node2Connected;

} DisplayMessage;


DisplayMessage data;


/******************** Variables ********************/
bool dataReceived = false;


/******************** ESP-NOW receive callback ********************/
void OnDataRecv(const esp_now_recv_info_t *recvInfo,
                const uint8_t *incomingData,
                int len)
{

  memcpy(&data,
         incomingData,
         sizeof(data));

  dataReceived = true;

}



/******************** Setup ********************/
void setup()
{

  Serial.begin(115200);


  /******** OLED ********/

  Wire.begin(OLED_SDA, OLED_SCL);


  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println("OLED init error");

    while(true);
  }


  display.clearDisplay();

  display.setTextSize(1);

  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0,0);

  display.println("NODE 4");

  display.println("DISPLAY");

  display.display();


  delay(1000);



  /******** ESP-NOW ********/

  WiFi.mode(WIFI_STA);


  if(esp_now_init()!=ESP_OK)
  {

    Serial.println("ESP-NOW init error");

    while(true);

  }


  esp_now_register_recv_cb(OnDataRecv);



  Serial.println();

  Serial.println("====================");

  Serial.println("===== NODE 4 OLED =====");

  Serial.println("====================");

}



/******************** Loop ********************/
void loop()
{


  if(dataReceived)
  {

    display.clearDisplay();


    display.setTextSize(1);

    display.setCursor(0,0);


    display.println("PUMP CONTROL");


    display.print("Cistern: ");

    display.print(data.citerneLevel);

    display.println(" cm");



    display.print("Reservoir: ");

    display.print(data.reservoirLevel);

    display.println(" cm");



    display.print("Pump: ");

    if(data.pumpState)
        display.println("ON");
    else
        display.println("OFF");



    if(data.dryRun)
    {
      display.println("ALERT");

      display.println("LOW WATER");
    }


    else if(data.overflow)
    {
      display.println("ALERT");

      display.println("RESERVOIR FULL");
    }


    else
    {

      if(!data.node1Connected ||
         !data.node2Connected)
      {

        display.println("Error");

        display.println("Comm. lost");

      }

      else
      {

        display.println("SYSTEM OK");

      }

    }


    display.display();



    /******** Serial ********/

    Serial.println("----------------");

    Serial.print("Cistern: ");

    Serial.print(data.citerneLevel);

    Serial.println(" cm");


    Serial.print("Reservoir: ");

    Serial.print(data.reservoirLevel);

    Serial.println(" cm");


    Serial.print("Pump: ");

    Serial.println(data.pumpState ? "ON":"OFF");


    Serial.println("----------------");


    dataReceived=false;

  }


  delay(200);

}
