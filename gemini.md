# LoRa-MQTT Gateway (Heltec V3 Receiver)

## Overview

This project implements a LoRaWAN receiver on a Heltec WiFi LoRa 32 (V3) board. Its primary function is to receive predefined, secured LoRa packets, validate their integrity and authenticity, and then publish the decoded data to an MQTT broker in JSON format. It also includes features for remote monitoring, Over-the-Air (OTA) updates, and network diagnostics.

## Features

*   **LoRa Packet Reception**: Utilizes the onboard SX1262 LoRa radio to receive packets.
*   **Custom Packet Protocol**: Implements a `Packet` struct with `volts`, `temp`, `counter`, and `mic` fields for structured data exchange.
*   **Robust Packet Validation**:
    *   Ensures correct packet size.
    *   **Replay Attack Protection**: Employs a "Counter Window" logic to prevent replay attacks and intelligently handle sender reboots and counter wrap-around events.
    *   **Message Integrity Code (MIC)**: Uses SipHash with a hardcoded `SECRET_KEY` to verify packet integrity and authenticity.
*   **MQTT Publishing**: Publishes validated LoRa data in JSON format to a configurable MQTT topic, including RSSI and SNR.
*   **MQTT Heartbeat**: Periodically publishes an MQTT message (`radiolib/heartbeat`) with device uptime, IP address, WiFi RSSI, and UTC timestamp, indicating the device is operational.
*   **NTP Time Synchronization**: Synchronizes device time with NTP servers for accurate UTC timestamps in logs and heartbeat messages.
*   **Over-the-Air (OTA) Updates**: Allows wireless firmware updates over Wi-Fi, simplifying deployment and maintenance.
*   **Remote WebSerial Logging**: Provides a web-based interface at `http://<DEVICE_IP>/webserial` to view real-time log output, mirroring the Serial Monitor.
*   **Web-based Device Reboot**: Includes a button on the device's main web page (`http://<DEVICE_IP>/`) to remotely trigger a soft reboot.
*   **Modular Codebase**: Clean separation of concerns with dedicated files for Logger and Web Route definitions.
*   **Secure Credential Management**: Wi-Fi and MQTT credentials are externalized to `include/env.h` and ignored by Git, ensuring sensitive information is not exposed in version control.

## Hardware

*   **Heltec WiFi LoRa 32 (V3)**: An ESP32-S3 based development board with integrated LoRa SX1262.

## Prerequisites

*   **PlatformIO**: Installed as a VS Code extension or CLI tool.
*   **VS Code**: Recommended IDE for development.
*   **MQTT Broker**: An accessible MQTT broker (e.g., Mosquitto, CloudMQTT).
*   **LoRa Sender**: A compatible LoRa sender device transmitting packets according to the defined protocol and radio settings.

## Getting Started

1.  **Clone this repository**:
    ```bash
    git clone <repository_url>
    cd radiolib_homenode
    ```

2.  **Configure Credentials**:
    *   Copy `env.h.example` to `include/env.h`:
        ```bash
        cp env.h.example include/env.h
        ```
    *   Edit `include/env.h` and replace the placeholder values (`"YOUR_WIFI_SSID"`, `"YOUR_WIFI_PASSWORD"`, `"YOUR_MQTT_BROKER_IP"`, `"YOUR_MQTT_USERNAME"`, `"YOUR_MQTT_PASSWORD"`) with your actual Wi-Fi and MQTT broker credentials.

3.  **Initial Firmware Upload (USB)**:
    *   Connect your Heltec V3 board to your computer via USB.
    *   Use PlatformIO's CLI to upload the firmware, overriding the OTA upload protocol for the initial flash:
        ```bash
        platformio run --target upload --project-option "upload_protocol=esptool"
        ```
    *   If you encounter issues finding the serial port, ensure drivers are installed and the correct port is selected.

4.  **Monitoring and Remote Access**:
    *   **Serial Monitor**: After uploading, open the PlatformIO Serial Monitor (baud rate 115200) to see initial boot logs and Wi-Fi connection status.
    *   **Web Interface**: Once the device connects to Wi-Fi, find its IP address in the serial logs. Navigate to `http://<DEVICE_IP>/` in your web browser for the main interface (with the reboot button) and `http://<DEVICE_IP>/webserial` for the live log viewer.
    *   **MQTT Broker**: Monitor your configured MQTT topics (`radiolib/lora/data` and `radiolib/heartbeat`) to see incoming data.

## ⚠️ Important Note: Radio Settings ⚠️

The LoRa radio settings on this receiver are specifically configured to match the expected protocol of the sender device. **These settings MUST NOT be changed** on the receiver side, as any modification will break communication with the sender.

**Fixed LoRa Settings:**

*   **Frequency**: 868.525 MHz (Finland)
*   **Spreading Factor (SF)**: 12
*   **Bandwidth (BW)**: 125.0 kHz
*   **Coding Rate (CR)**: 4/5
*   **Sync Word**: 0x14

## Protocol Details

The receiver expects a packed `Packet` struct of 10 bytes:
*   `volts` (uint16_t): Sensor voltage.
*   `temp` (int16_t): Scaled temperature (value / 100.0 for actual Celsius).
*   `counter` (uint16_t): A monotonically increasing counter for replay protection.
*   `mic` (uint32_t): A Message Integrity Code generated by `siphash` on the first 6 bytes (`volts`, `temp`, `counter`) using a `SECRET_KEY`.

## Security Considerations

*   **Replay Protection**: The `last_seen_counter` with its "Counter Window" logic prevents old packets from being re-processed and helps detect sender reboots.
*   **Packet Integrity**: The `siphash` generated MIC ensures that received packets have not been tampered with and originate from a device that possesses the `SECRET_KEY`.
*   **Hardcoded Key**: The `SECRET_KEY` is hardcoded. For higher security applications, consider using a key provisioning mechanism.
