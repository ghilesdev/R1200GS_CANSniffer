#include <WiFi.h>
#include <esp_now.h>
#include <mcp_can.h>
#include <SPI.h>
#include <math.h>

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

  // Broadcast peer
  // esp_now_peer_info_t peer = {};
  // memset(peer.peer_addr, 0xFF, 6);
  // peer.channel = 0;
  // peer.encrypt = false;
  // esp_now_add_peer(&peer);

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
  simulate = true;
}

void loop() {
  // -------- CAN MODE --------
  if (can_ok && CAN_MSGAVAIL == CAN.checkReceive()) {
    long unsigned int rxId;
    unsigned char len, buf[8];
    CAN.readMsgBuf(&rxId, &len, buf);

    if (rxId == 0x10C) {
      data.rpm = (buf[3] * 256 + buf[2]) / 4;
    }
    else if (rxId == 0x2D0) {
      data.fuel = map(buf[3], 0, 255, 0, 100);
    }
    else if (rxId == 0x2BC) {
      int temp = buf[2] * 0.75 - 24;
      temp = constrain(temp, -40, 215);
      data.oilTemp = temp + 40;
    }
  }

  // -------- SIMULATION MODE --------
  if (simulate) {
    float t = millis();
    data.rpm = 2500 + 2500 * sin(t / 1000.0);
    float f = 50.0 + 50.0 * sin(t / 5000.0);
    data.fuel = (uint8_t) constrain(f, 0, 100);
    int temp = 80 + 10 * sin(t / 3000.0);
    data.oilTemp = temp + 40;
  }

  // -------- ESP-NOW SEND --------
  static uint32_t last = 0;
  if (millis() - last > 100) {
    esp_now_send(receiverMac, (uint8_t *)&data, sizeof(data));
    last = millis();
  }

  delay(1);
}
