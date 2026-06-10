# Morse Code Transmitter

An interrupt-driven visual Morse code transmitter built with an Arduino Nano (ATmega328P) and a WS2812B LED strip. The project converts text messages into Morse code and displays them using a sweeping lighthouse-style wave effect across the LEDs.

<video src="morse.mp4" width="100%" autoplay loop muted playsinline>
</video>

## Features

* **Interrupt-Driven Timing**: Uses hardware Timer1 for highly precise timing intervals, leaving the main `loop()` completely empty.
* **Lighthouse Wave Effect**: Instead of simple blinking, the LEDs display a smooth sweeping effect that moves across the strip.
* **State Persistence**: Uses EEPROM to remember the playback state across power cycles.
* **Minimalist UI**: System is toggled between "play" and "stop" by simply pressing the Arduino's Reset button.
* **Memory Efficient**: Uses flash memory for string storage and lookup tables to preserve limited RAM.

## Hardware Requirements

* **Microcontroller**: Arduino Nano (ATmega328P)
* **LEDs**: WS2812B Addressable LED Strip (8 LEDs used)
* **Connections**: 
  * LED Data Pin $\rightarrow$ Digital Pin 6 (PD6)
  * Power $\rightarrow$ 5V
  * Ground $\rightarrow$ GND

## How It Works

The system translates strings (e.g., "IN", "SOS") into dots and dashes based on standard Morse timing ratios, spaced out for better visual clarity (Dot = 500ms). When running, the Timer1 interrupt service routine calculates and updates the LED colors and positions seamlessly. 

Pressing the reset button toggles the device on or off. Feedback is provided via startup (Red $\rightarrow$ Green $\rightarrow$ Off) and shutdown (Green $\rightarrow$ Red $\rightarrow$ Off) visual sequences.

## Configuration

You can customize the maximum steps, message, and visual effect by modifying definitions in the `morse.ino` file:
* Update the `MESSAGE` string to transmit different text.
* Change `WAVE_WIDTH` to adjust the size of the sweeping light beam (0 for standard blinking, 5 for default wave).

## Documentation

For a complete documentation directly check out the code in `morse.ino` and read the documentation in `/report/report.pdf`.

