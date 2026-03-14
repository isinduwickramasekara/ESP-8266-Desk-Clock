# ESP8266 OLED Desk Clock

A compact WiFi-synchronised desk clock built using an ESP8266 microcontroller and a 0.96" OLED display.
The device automatically synchronises time using NTP servers and displays the current time on a monochrome OLED screen.

This project was designed as a small embedded systems build combining firmware, PCB design, and enclosure development.



## Project Overview

This desk clock uses an ESP8266 microcontroller to connect to WiFi and obtain accurate time from internet NTP servers.
The time is displayed on a low-power OLED screen, creating a simple and compact desk companion.

The project includes:

• Embedded firmware written in Arduino C++

• Custom schematic and PCB design

• Optional Li-Po battery charging module (currently under development)



## Features

* Automatic time synchronisation via WiFi (NTP)
* OLED display for time and date
* Compact low-power design
* Custom PCB layout
* Status indicators for charging module
* Designed for small desktop enclosures



## Hardware

Main components used in the system:

| Component          | Description                                           |
| ------------------ | ----------------------------------------------------- |
| ESP8266            | Main microcontroller with WiFi connectivity           |
| 0.96" OLED Display | SSD1306 I²C display module                            |
| TP4056             | Lithium-ion battery charging controller               |
| Li-Po Battery      | Optional battery power source                         |
| AO3401A MOSFET     | Power path control                                    |
| Passive Components | Resistors and capacitors for regulation and filtering |



## Display Interface

The OLED display communicates using the I²C protocol.

| OLED Pin | ESP8266 Pin |
| -------- | ----------- |
| VCC      | 3.3V        |
| GND      | GND         |
| SDA      | D2          |
| SCL      | D1          |



## Schematic

The schematic was designed using KiCad.

Example structure of the system:

Power Input → Charging Module → Battery → Voltage Regulation → ESP8266 → OLED Display

Note: The Li-Po charging module is currently under development and may be revised in future PCB versions.



## PCB Design

The PCB was designed using KiCad and manufactured through a PCB fabrication service.

The board integrates:

* ESP8266 module
* battery charging circuit
* display connection
* power filtering
* status LEDs



## Firmware

The firmware was developed using the Arduino IDE.

Main functions implemented:

* WiFi connection
* NTP time synchronisation
* OLED display control
* periodic time updates



## Required Libraries

Install the following libraries in Arduino IDE:

* ESP8266WiFi
* NTPClient
* Adafruit GFX
* Adafruit SSD1306

---

## Images

* finished clock
  ![Clock](images/clock_photo.jpg)
* PCB render
  ![PCB](images/PCB_3D_render.png)
* schematic
  ![schematic](images/Schematic.png)
* 3D Model
  ![3Dmodel](images/3Dmodel(1).png)


---

## Future Improvements

Planned improvements for future versions include:

* alarm functionality
* weather display using APIs
* improved enclosure design
* power optimisation
* refined battery charging circuit

---


## License

This project is released as an open hardware and open source project for educational purposes.

---

## Author

Designed and developed by
Isindu Anusara Wickramasekara

