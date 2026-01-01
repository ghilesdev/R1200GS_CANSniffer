#include <WiFi.h>
#include <esp_now.h>
#include <mcp_can.h>
#include <SPI.h>
#include <math.h>

// TODO 
// debug temp : shows 52 cold wtf!!! maybe offset??
// no blinkers
// no gears >>> done


// -------- PIN DEFINITIONS --------
#define CAN_CS   27
#define CAN_INT  26
#define SPI_SCK  18
#define SPI_MISO 19
#define SPI_MOSI 23

MCP_CAN CAN(CAN_CS);

// -------- DATA STRUCT --------
typedef struct __attribute__((packed)) {
  uint16_t rpm;      // real RPM
  uint8_t  fuel;     // 0–100 %
  uint8_t  oilTemp;  // °C + 40 offset
  uint8_t  speed;  
  uint8_t  gear;      // 1=1st, 2=N, 4=2nd, 7=3rd, 8=4th, B=5th, D=6th, F=In between Gears  HIGH NIBBLE
  uint8_t  infoButton; // 4=Off, 5=Short Press, 6=Long Press(>2secs)  
  uint16_t  blinkers; // CF=Off, D7=Left On, E7=Right On, EF=Both On
  int odometer; // (d3,d2,d1)
} bike_data_t;

bike_data_t data;

// -------- STATE --------
bool can_ok = false;
bool simulate = false;
// 1C:DB:D4:37:BA:6C
uint8_t receiverMac[] = {0x1C, 0xDB, 0xD4, 0x37, 0xBA, 0x6C}; // CHANGE

// -------- ESP-NOW CALLBACK --------
void onSent(const wifi_tx_info_t *info, esp_now_send_status_t status)  {
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}

const int getGearLabel(uint8_t value) {
  switch (value) {
    case 0x01: return 1;
    case 0x02: return 0;
    case 0x04: return 2;
    case 0x07: return 3;
    case 0x08: return 4;
    case 0x0B: return 5;
    case 0x0D: return 6;
    case 0x0F: return 8;
    default: return 9;
  }
}
const int getBlinkersStatus(unsigned char value) {
  switch (value) {
    case 0xCF: return 0;
    case 0xD7: return 1;
    case 0xE7: return 2;
    case 0xEF: return 3;
    default: return 9;
  }
}

void setup() {
  Serial.begin(115200);

  // -------- WIFI (ESP-NOW) --------
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  WiFi.setSleep(false);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    while (true);
  }

  esp_now_register_send_cb(onSent);
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, receiverMac, 6);
  peer.channel = 0;
  peer.encrypt = false;

  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("Peer add failed");
  }
  else {
    Serial.println("Peer add successful");
  }


  // -------- SPI / CAN --------
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, CAN_CS);
  pinMode(CAN_INT, INPUT_PULLUP);

  if (CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
    CAN.setMode(MCP_NORMAL);
    can_ok = true;
    simulate = false;
    Serial.println("CAN OK");
  } else {
    simulate = true;
    Serial.println("CAN NOT FOUND → SIMULATION MODE");
  }

  // force sim if needed
  // simulate = true;
}

void loop() {
  // -------- CAN MODE --------
  if (can_ok && CAN_MSGAVAIL == CAN.checkReceive()) {
    long unsigned int rxId;
    unsigned char len, buf[8];
    CAN.readMsgBuf(&rxId, &len, buf);

    if (rxId == 0x10C) {
      data.rpm = (buf[3] * 256 + buf[2]) / 4;                           // rpm             tested
    }
    else if (rxId == 0x2D0) {
      data.fuel = map(buf[3], 0, 255, 0, 100);                          // fuel            tested
      Serial.print("info button raw value: ");
      Serial.println(buf[5]);
      data.infoButton = (uint8_t) buf[5] & 0x0F;                        // info            tested
    }
    else if (rxId == 0x130) {
      unsigned char blinks = buf[7];
      Serial.print("Blinks raw value: ");
      Serial.println(blinks);
      Serial.print("Blinks transformed value: ");
      Serial.println((uint8_t)  getBlinkersStatus(blinks));
      data.blinkers = (uint8_t)  getBlinkersStatus(blinks);             // blinkers        tested
    }
    else if (rxId == 0x2BC) {
      int temp = buf[2] * 0.75 - 24;
      temp = constrain(temp, -40, 215);
      data.oilTemp = temp;                                          // oil             tested
      uint8_t shifted = (uint8_t) (buf[5] >> 4) & 0x0F;
      data.gear = (uint8_t) getGearLabel(shifted);                      // gear            tested
      Serial.print("gears raw value: ");
      Serial.println(buf[5]);
      Serial.print("Gears shifted value: ");
      Serial.println(shifted);
      Serial.print("Gears final value: ");
      Serial.println((uint8_t) getGearLabel(shifted));
    }
    else if (rxId == 0x3F8) {
      data.odometer = (int) (buf[3] << 16) + (buf[2] << 8) + buf[1];     // odo             tested
    }
  }

  // -------- SIMULATION MODE --------
  if (simulate) {
    float t = millis();
    data.rpm = 4000 + 4000 * sin(t / 1000.0);
    float f = 50.0 + 50.0 * sin(t / 5000.0);
    data.fuel = (uint8_t) constrain(f, 0, 100);
    int temp = 80 + 10 * sin(t / 3000.0);
    data.oilTemp = temp + 40;
    data.gear = random(7);
    data.speed = (uint8_t) 199;
    data.odometer = (int)  (0xFA << 16) + (0xAB << 8) + 0x15;
    
    char info =  random(0xB4,0xB7);
    data.infoButton = (uint8_t) info & 0x0F;
    unsigned char  blinker[4];
    blinker[0] = 0xCF;
    blinker[1] = 0xD7;
    blinker[2] = 0xE7;
    blinker[3] = 0xEF;
    data.blinkers = (uint8_t)  getBlinkersStatus(blinker[random(4)]);               // blinkers
  }

  // -------- ESP-NOW SEND --------
  static uint32_t last = 0;
  if (millis() - last > 100) {
    esp_now_send(receiverMac, (uint8_t *)&data, sizeof(data));
    last = millis();
  }

  delay(1);
}
