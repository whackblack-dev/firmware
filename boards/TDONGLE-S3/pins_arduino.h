#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

// Required because platformio.ini sets board_build.variants_dir = boards,
// which makes PlatformIO look for this file under boards/TDONGLE-S3/
// instead of the framework's own variants folder. Arduino.h always
// #includes "pins_arduino.h" unconditionally, so this file must exist
// even though TFT/button/LED pins are set explicitly elsewhere
// (TDONGLE-S3.ini build_flags + interface.cpp bruceConfigPins).

#define USB_VID 0x303a
#define USB_PID 0x1001

// No I2C bus broken out on this board. Values below are unused, free GPIOs
// (not the TFT, button, SD or RGB LED pins) so the Arduino Wire defaults
// stay harmless if anything references them.
static const uint8_t SDA = 8;
static const uint8_t SCL = 9;

// Generic SPI aliases. Not used by TFT_eSPI (reads TFT_MOSI/TFT_SCLK/TFT_CS
// from build_flags) or by our interface.cpp (explicit bruceConfigPins buses),
// but the Arduino core expects these to exist. Picked free, unused GPIOs.
static const uint8_t SS   = 10;
static const uint8_t MOSI = 11;
static const uint8_t MISO = 7;
static const uint8_t SCK  = 6;

// UART0 TX/RX placeholders (this board only exposes the native USB port,
// no separate debug UART header) — free, unused GPIOs.
static const uint8_t TX = 43;
static const uint8_t RX = 44;

#endif /* Pins_Arduino_h */
