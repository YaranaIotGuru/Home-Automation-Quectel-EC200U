# Home Automation System Using Quectel EC200U

Welcome to the **Home Automation System** project! This repository contains the code and resources to build a smart home automation system using the **Quectel EC200U GSM module** and Arduino. With this project, you can control home appliances (like lights, fans, or motors) remotely via **DTMF signals**, **SMS commands**, and even get **call-end feedback** through SMS. It’s an advanced IoT solution perfect for DIY enthusiasts and smart home builders!

This project was created and explained by **Yarana IoT Guru**—check out the full video tutorial on my [YouTube channel](https://www.youtube.com/@YaranaIoTGuru) for a step-by-step guide!

---

## Features

- **DTMF-Based Relay Control**: Toggle relays ON/OFF in real-time by pressing phone keys during a call.  
- **SMS-Controlled Automation**: Send text messages to switch appliances remotely.  
- **Call-End Feedback**: Receive an SMS with relay status after disconnecting a call.  
- **EEPROM Logging**: Stores relay states and activity logs, even after power loss.  
- **Wireless Control**: Manage devices from anywhere using mobile networks (no Wi-Fi needed).  
- **Failsafe Operation**: Optimized AT command handling for stable communication with the Quectel EC200U.  

---

## Hardware Requirements

- **Quectel EC200U GSM Module**: For mobile connectivity (SMS, calls).  
- **Arduino (Uno/Nano)**: The microcontroller to run the code.  
- **Relays (e.g., 5V, 4-channel)**: To control appliances.  
- **SIM Card**: Active with SMS/call support.  
- **Jumper Wires & Breadboard**: For connections.  
- **Power Supply**: 5V for Arduino, 3.3V-4.2V for EC200U.  
- **Optional**: LED indicators or small appliances for testing.  

---

## Software Requirements

- **Arduino IDE**: To upload the code to your Arduino.  
- **Libraries**:  
  - `SoftwareSerial` (usually built-in with Arduino IDE).  
- A computer to program the Arduino.  

---

## Wiring Diagram

| Quectel EC200U Pin | Arduino Pin | Description            |  
|---------------------|-------------|------------------------|  
| TXD                | D2          | Transmit data to Arduino |  
| RXD                | D3          | Receive data from Arduino |  
| VCC                | 3.3V-4.2V   | Power supply (check module specs) |  
| GND                | GND         | Ground connection      |  

| Relay Pin          | Arduino Pin | Description            |  
|---------------------|-------------|------------------------|  
| IN1                | D4          | Relay 1 control        |  
| IN2                | D5          | Relay 2 control        |  
| VCC                | 5V          | Relay power            |  
| GND                | GND         | Ground connection      |  

**Note**: Double-check your relay module’s voltage requirements and use appropriate safety measures for high-voltage appliances.

---

## Installation

1. **Clone the Repository**:  
   ```bash
   git clone https://github.com/yaranaiotguru/Home-Automation-Quectel-EC200U.git

  # Home Automation with EC200U

This project demonstrates a home automation system using the EC200U module with an Arduino. Follow the steps below to set up and run the code.

## Getting Started

### 1. Download and Extract
- Download the ZIP file of this repository.
- Extract the contents to a folder on your computer.

### 2. Open the Code
- Launch the **Arduino IDE**.
- Open the `HomeAutomation_EC200U.ino` file located in the repository folder.

### 3. Install Libraries
- Ensure the `SoftwareSerial` library is available.  
  *(Note: This library is included by default in the Arduino IDE.)*

### 4. Configure the Code
- Update the **SIM card phone number** in the code to receive feedback SMS.
- Adjust the **pin definitions** if your hardware wiring differs from the default setup.

### 5. Upload the Code
- Connect your Arduino board to your computer via USB.
- In the Arduino IDE, select the correct **board** and **port** from the Tools menu.
- Click the **Upload** button to flash the code to your Arduino.

## Video Tutorial
Check out the detailed step-by-step tutorial for this project on our **Yarana IoT Guru YouTube channel**:  
[Home Automation with EC200U - Arduino Tutorial](https://www.youtube.com/@YaranaIotGuru)  
Subscribe for more IoT projects and guides!

## Additional Notes
- Ensure your hardware connections match the pin configuration in the code.
- Test the setup after uploading to verify functionality.

Happy automating!
