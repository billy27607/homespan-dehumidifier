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
- ✅ **Web Dashboard**: Status and pump-out from a browser, captive-portal Wi-Fi setup
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

## Web Interface

Settings live in `include/config.h`. Passwords live in gitignored files that you create from the example templates:

```sh
cp include/secrets.example.h include/secrets.h   # AP_PASSWORD, DASHBOARD_PASSWORD, OTA_PASSWORD
cp secrets.example.ini secrets.ini               # OTA upload password
```

The build stops with an error until `include/secrets.h` exists.

### Wi-Fi setup (captive portal)
The device starts its own access point, **`Dehumidifier-Setup`** (password: `AP_PASSWORD`), when:
- no Wi-Fi credentials are stored (first boot),
- the saved network can't be joined within 3 minutes of boot,
- you press **Change Wi-Fi network** on the dashboard, or
- you type `A` on the serial console.

Join it from a phone and the setup page pops up (or browse to `http://192.168.4.1/`). Choose a network and enter its password. You can also set:
- **Admin password** for the dashboard (at least 8 characters). Until you set one, `DASHBOARD_PASSWORD` from `secrets.h` is used. The serial `E` command erases it and restores that default.
- **HomeKit setup code** (8 digits, such as `123-45-678`), entered when adding the device in the Home app. The default is HomeSpan's `466-37-726`.

Leave either one blank to keep the current value. The device saves everything and restarts. If nothing is saved within 5 minutes, the portal restarts the device so it retries the saved network.

### Dashboard
On your home network, browse to `http://dehumidifier-<mac>.local:8080/` (the MAC is 12 hex digits, lowercase, with no separators) or `http://<device-ip>:8080/` and log in as `admin` with `DASHBOARD_PASSWORD` (HomeKit already uses port 80). The dashboard shows:
- CAN link, humidity, target, temperature and run state (refreshed every 2 seconds, `CAN_POLL_MS`)
- pump state, with a **Pump out water** button to empty the condensate
- Wi-Fi network, signal, IP address, uptime, free memory and firmware version
- HomeKit pairing status, with a **Reset HomeKit pairing** button. It removes all pairings but keeps the device's HomeKit ID. Afterwards, delete the dehumidifier in the Home app and add it again.
- **Factory reset** button: erases all saved data (Wi-Fi, HomeKit pairing and setup code, admin password, saved settings) and restarts into the setup network. The device gets a new HomeKit ID, so the Home app sees it as a new accessory. Its network name is built from the MAC address and doesn't change. The OTA password is set from `OTA_PASSWORD` in `secrets.h` on every boot, so OTA keeps working.

The dashboard uses HTTP basic auth over plain HTTP, so keep it on a trusted LAN.

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

The code reads the dehumidifier over CAN every 2 seconds (`CAN_POLL_MS`, or every 5 seconds while the link is down). HomeKit is updated from those readings every 5 seconds.

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
