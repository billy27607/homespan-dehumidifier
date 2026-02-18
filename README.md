# HomeSpan Alorair HDi65 Dehumidifier

A HomeKit integration for the Alorair HDi65 dehumidifier using ESP32-C3 and the HomeSpan library. This project enables control of the Alorair HDi65 dehumidifier through Apple Home, allowing you to monitor and adjust humidity settings from your iPhone, iPad, or Mac.

## Overview

This project provides a HomeKit bridge for the Alorair HDi65 dehumidifier, transforming it into a smart home device. It communicates with the dehumidifier via CAN bus and exposes standard HomeKit characteristics for humidity control, temperature monitoring, and device status.

## Hardware Requirements

- **Microcontroller**: Seeed XIAO ESP32-C3
- **Dehumidifier**: Alorair HDi65
- **Communication**: CAN bus connection (pin D7)
- **Network**: WiFi connection for HomeKit integration

## Features

- ✅ **HomeKit Integration**: Full Apple Home app support
- ✅ **Power Control**: Turn the dehumidifier on/off remotely
- ✅ **Humidity Monitoring**: Real-time humidity readings
- ✅ **Temperature Monitoring**: Track current temperature
- ✅ **Target Humidity**: Set desired humidity level (dehumidifier threshold)
- ✅ **OTA Updates**: Over-the-air firmware updates via HomeSpan
- ✅ **Status Monitoring**: Track active/inactive and operating states
- ✅ **Persistent Settings**: All settings stored in non-volatile storage (NVS)

## HomeKit Services

### 1. Humidifier/Dehumidifier Service
- **Active**: Power control (on/off)
- **Current Relative Humidity**: Real-time humidity percentage
- **Current State**: Operating status (inactive/idle/dehumidifying)
- **Target State**: Dehumidifier mode
- **Dehumidifier Threshold**: Target humidity setpoint (40-80%)

### 2. Temperature Sensor Service
- **Current Temperature**: Real-time temperature reading (0-110°F)

### 3. Humidity Sensor Service
- **Current Relative Humidity**: Real-time humidity percentage (0-100%)

## Dependencies

This project uses PlatformIO and requires the following libraries:

- **HomeSpan** (v2.1.7+): HomeKit implementation for ESP32
- **CAN_BUS_Shield** (v2.3.3+): CAN bus communication library
- **AlorairHDi65**: Custom library for Alorair HDi65 control (see [AlorairHDi65 repository](https://github.com/billy27607/AlorairHDi65))

## Installation

### 1. Clone the Repository

```bash
git clone https://github.com/billy27607/AlorairHDi65.git
cd homespan-dehumidifier
```

### 2. Open in PlatformIO

This project is configured for PlatformIO. Open it in Visual Studio Code with the PlatformIO extension installed.

### 3. Configure WiFi

On first boot, the ESP32 will create a WiFi access point for configuration. Connect to it and enter your WiFi credentials through the HomeSpan setup process.

### 4. Build and Upload

```bash
pio run --target upload
```

### 5. Add to Apple Home

1. Open the Home app on your iPhone/iPad
2. Tap the '+' button and select "Add Accessory"
3. Scan the HomeKit QR code displayed in the serial monitor
4. Follow the prompts to add the "AlorAirHDi65" accessory

## Configuration

### OTA Updates

OTA (Over-The-Air) updates are enabled by default. After initial setup, you can update the firmware wirelessly:

1. Update the `upload_port` in `platformio.ini` with your device's IP address
2. Run: `pio run --target upload`
3. Enter the OTA password when prompted (default: `homespan-ota`)

### CAN Bus Connection

The dehumidifier communicates via CAN bus on pin **D7**. Ensure proper CAN bus wiring:
- **CAN_H**: Connect to dehumidifier CAN_H
- **CAN_L**: Connect to dehumidifier CAN_L
- **GND**: Common ground

## Usage

Once added to Apple Home, you can:

- **Turn on/off** the dehumidifier
- **Set target humidity** using the humidity threshold slider
- **Monitor current humidity** in real-time
- **Monitor temperature** from the dehumidifier's sensor
- **Create automations** based on humidity levels
- **Use Siri** for voice control: "Hey Siri, turn on the dehumidifier"

## Project Structure

```
homespan-dehumidifier/
├── src/
│   └── main.cpp           # Main application code
├── include/               # Header files
├── lib/                   # Local libraries
├── test/                  # Test files
├── platformio.ini         # PlatformIO configuration
└── README.md             # This file
```

## Code Overview

The main application (`main.cpp`) implements three HomeKit services:

1. **HumidifierDehumidifier**: Main service for controlling the dehumidifier
   - Handles power state changes
   - Updates target humidity settings
   - Monitors and reports current state

2. **TempSensor**: Provides temperature readings from the dehumidifier

3. **HumiditySensor**: Provides humidity readings from the dehumidifier

The code polls the dehumidifier every 5 seconds to update status and environmental readings.

## Technical Details

- **Platform**: Espressif32 (custom build)
- **Framework**: Arduino
- **Partitions**: Minimal SPIFFS configuration
- **Upload Protocol**: ESP OTA (Over-The-Air)
- **Default Upload IP**: 192.168.101.143 (update this in platformio.ini)

## Troubleshooting

### CAN Bus Connection Failed
- Check wiring connections to the dehumidifier
- Verify CAN_H and CAN_L are not reversed
- Ensure proper termination resistors are in place
- Check serial monitor for "Can connect failed..." message

### Device Not Appearing in Home App
- Verify WiFi credentials are correct
- Ensure the ESP32 is on the same network as your Apple device
- Check serial monitor for HomeKit pairing code
- Try resetting the HomeSpan configuration

### OTA Upload Fails
- Verify the IP address in `platformio.ini` matches your device
- Ensure the device is connected to WiFi
- Check that the OTA password matches (default: `homespan-ota`)
- Try uploading via USB if OTA continues to fail

## Version History

- **0.9.0**: Initial release
  - Basic HomeKit integration
  - Power control and humidity monitoring
  - Temperature sensor support
  - OTA update capability

## License

This project is open source. Please refer to the LICENSE file for details.

## Acknowledgments

- **HomeSpan**: Fantastic HomeKit library by [Gregg E. Berman](https://github.com/HomeSpan/HomeSpan)
- **Alorair**: For creating a quality dehumidifier suitable for modification
- **Seeed Studio**: For the excellent XIAO ESP32-C3 board

## Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for bugs and feature requests.

## Author

Billy ([billy27607](https://github.com/billy27607))

## Support

For issues and questions:
- Open an issue on GitHub
- Check the [HomeSpan documentation](https://github.com/HomeSpan/HomeSpan)
- Review the [AlorairHDi65 library](https://github.com/billy27607/AlorairHDi65)
