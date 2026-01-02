# ESP32 CAN Bus Sniffer - BMW R1200GS K25

An ESP32 WROOM D2 based CAN bus sniffer for BMW R1200GS K25 motorcycles that captures real-time engine and vehicle data and transmits it wirelessly via ESP-NOW protocol.

## Overview

This project reads data from the BMW R1200GS K25 CAN bus and extracts key vehicle parameters including RPM, fuel level, oil temperature, speed, gear position, blinker status, and odometer reading. The data is then transmitted to a remote ESP32 receiver using the ESP-NOW protocol.

Features include:
- **Real-time CAN bus monitoring** at 500 kbps
- **Wireless data transmission** via ESP-NOW (low latency, no WiFi required)
- **Simulation mode** for testing without a CAN bus connection
- **Packed data structure** for efficient transmission (20 bytes)

## Hardware Requirements

### Microcontroller
- **ESP32 WROOM D2** (or compatible ESP32 board)

### CAN Bus Interface
- **MCP2515 CAN Controller** with crystal oscillator
- **TJA1050 CAN Transceiver** (or similar)

### Connection to Motorcycle

The sniffer connects to the **BMW R1200GS K25 alarm connector**, which provides:
- **CAN_H** - CAN High signal
- **CAN_L** - CAN Low signal  
- **12V** - Switched power
- **Switched 12V** - Ignition-switched power
- **GND** - Ground reference

### Wiring

**ESP32 to CAN Module (SPI Interface):**

| ESP32 Pin | CAN Module Pin | Signal |
|-----------|---|--------|
| GPIO 27 | CS | Chip Select |
| GPIO 26 | INT | Interrupt |
| GPIO 18 | SCK | SPI Clock |
| GPIO 19 | MISO | SPI Data In |
| GPIO 23 | MOSI | SPI Data Out |
| GND | GND | Ground |
| 3V3 | VCC | Power |

**CAN Module to Motorcycle (Alarm Connector):**

| CAN Module Pin | Bike Alarm Connector | Signal |
|---|---|---|
| CAN_H | CAN_H | CAN High |
| CAN_L | CAN_L | CAN Low |
| GND | GND | Ground |
| VCC | 12V or Switched 12V | Power Supply |

## Data Structure

The transmitted data packet is 20 bytes and contains the following fields:

```cpp
typedef struct __attribute__((packed)) {
  uint16_t rpm;           // Real RPM (0-16000)
  uint8_t  fuel;          // Fuel level (0-100%)
  uint8_t  oilTemp;       // Oil temperature in °C (offset: +40)
  uint8_t  speed;         // Speed in km/h
  uint8_t  gear;          // Gear position (0=N, 1-6=1st-6th, 8=between gears)
  uint8_t  infoButton;    // Info button state (4=Off, 5=Short Press, 6=Long Press)
  uint16_t blinkers;      // Blinker status (0=Off, 1=Left, 2=Right, 3=Both)
  int      odometer;      // Odometer reading in km
} bike_data_t;
```

### CAN Message IDs

| CAN ID | Data | Notes |
|--------|------|-------|
| 0x10C | RPM | Bytes 2-3, divide by 4 |
| 0x2D0 | Fuel, Info Button | Fuel: byte 3, Info: byte 5 |
| 0x130 | Blinkers | Byte 7 |
| 0x2BC | Oil Temp, Gear | Temp: byte 2 (×0.75 - 24), Gear: byte 5 (upper nibble) |
| 0x3F8 | Odometer | Bytes 1-3 |

## Installation

### Dependencies
- **Arduino IDE** or **PlatformIO**
- **Libraries:**
  - `WiFi.h` (built-in)
  - `esp_now.h` (built-in)
  - `mcp_can.h` (install via Library Manager)
  - `SPI.h` (built-in)

### Steps

1. Install the MCP_CAN library:
   - Arduino IDE: Sketch → Include Library → Manage Libraries → search "MCP_CAN" → install by Corrado Ubezio

2. Update the receiver MAC address in the code:
   ```cpp
   uint8_t receiverMac[] = {0x1C, 0xDB, 0xD4, 0x37, 0xBA, 0x6C}; // CHANGE THIS
   ```

3. Upload to your ESP32 WROOM D2

## Usage

### Normal Operation
The device will attempt to initialize the CAN bus at startup. If successful, it will read CAN messages and transmit data to the receiver every 100ms.

### Simulation Mode
If the CAN bus is not detected, the device automatically enters simulation mode and generates synthetic data for testing. You can also force simulation mode by uncommenting the line:
```cpp
// simulate = true;
```

### Serial Monitor
Open the Serial Monitor at **115200 baud** to view:
- CAN bus initialization status
- Transmission success/failure confirmation
- Raw CAN data values (for debugging)
- Simulation mode status

## Configuration

### Change CAN Bitrate
Modify the initialization:
```cpp
CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ);  // Change CAN_500KBPS if needed
```

### Change Transmission Interval
Modify the delay (default: 100ms):
```cpp
if (millis() - last > 100) {  // Change 100 to desired milliseconds
```

### Change Receiver Address
Update the MAC address at the top of the code:
```cpp
uint8_t receiverMac[] = {0x1C, 0xDB, 0xD4, 0x37, 0xBA, 0x6C};
```

## Troubleshooting

| Issue | Solution |
|-------|----------|
| "CAN NOT FOUND" | Check wiring, verify 8MHz crystal on MCP2515, check CS and INT pins |
| No ESP-NOW transmission | Verify receiver MAC address, check receiver is powered on |
| Incorrect temperature readings | Oil temp has -24°C offset; formula is `(buf[2] × 0.75) - 24` |
| Wrong gear values | Upper nibble of byte 5 encodes gear; uses lookup table |
| Blinkers showing wrong status | Values: 0xCF=Off, 0xD7=Left, 0xE7=Right, 0xEF=Both |

## Notes
- Code the esp receiver and dashboard is [here](https://github.com/ghilesdev/R1200GS_CANDashBoard)
- All data is packed without padding for efficient transmission
- Temperature offset of +40°C is applied for transmission
- Gear values use custom mapping (see `getGearLabel()` function)
- Blinker values use custom mapping (see `getBlinkersStatus()` function)
- ESP-NOW does not require WiFi connection - direct peer-to-peer communication

## License

MIT License - Feel free to use and modify

## Credits & References

### CAN Protocol Documentation
The CAN message IDs and data decoding are based on the [BMW Motorrad CAN Bus Community Spreadsheet](https://docs.google.com/spreadsheets/d/1tUrOES5fQZa92Robr6uP8v2dzQDq9ohHjUiTU3isqdc), a collaborative effort by the BMW motorcycle enthusiast community documenting the K25 CAN bus protocol.

## Author

Created for BMW R1200GS K25 telemetry project
