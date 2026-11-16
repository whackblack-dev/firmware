#include "core/bus_HAL.h"
#include "core/powerSave.h"
#include <interface.h>

// LilyGO T-Dongle-S3 — unofficial Bruce port
//
// This board has exactly ONE physical button, on GPIO0 (shared with the
// BOOT strap pin — only matters at power-on, safe to use normally at runtime).
// No battery/PMIC, no mic, no RTC, no touch.
//
// Nav scheme (confirmed with user):
//   short press             -> NextPress  (cycle to next menu item)
//   long press (>500ms)     -> SelPress   (select)
//   double short press      -> EscPress   (back)
//
// NOT YET WIRED (stage 2, see TDONGLE-S3.ini for pin references):
//   - SD card (ESP32-S3 SDMMC peripheral, 4-bit — not a plain SPI bus)
//   - RGB status LED (APA102, clock+data — not a single-wire WS2812)

#define BTN_PIN 0

/***************************************************************************************
** Function name: _setup_gpio()
** Description:   initial setup for the device
***************************************************************************************/
void _setup_gpio() {
    bruceConfigPins.rotation = 1; // adjust after first boot if the screen reads sideways/upside down

    // This board has no physically wired I2C/SPI/IR/RF headers. Bruce's core
    // still calls pinMode() on every bus field at boot regardless, so every
    // field below must point at a real, unused GPIO instead of staying at
    // its default GPIO_NUM_NC (-1) — leaving them unset caused a
    // "pinMode(): Invalid IO 255" crash. These pins do nothing electrically;
    // they only exist to keep the core's generic init from touching -1.
    bruceConfigPins.i2c_bus = {(gpio_num_t)8, (gpio_num_t)9};   // sda, scl
    bruceConfigPins.outer_bus = {
        (gpio_num_t)6, (gpio_num_t)7, (gpio_num_t)11, (gpio_num_t)10
    }; // sck,miso,mosi,cs
    bruceConfigPins.PN532_bus = {
        (gpio_num_t)6, (gpio_num_t)7, (gpio_num_t)11, (gpio_num_t)10
    }; // sck,miso,mosi,cs
    bruceConfigPins.SDCARD_bus = {
        (gpio_num_t)6, (gpio_num_t)7, (gpio_num_t)11, (gpio_num_t)10
    }; // sck,miso,mosi,cs — placeholder; real SD uses the SDMMC peripheral (stage 2)
    bruceConfigPins.CC1101_bus = {
        (gpio_num_t)6, (gpio_num_t)7, (gpio_num_t)11, (gpio_num_t)10, (gpio_num_t)13, GPIO_NUM_NC
    }; // sck,miso,mosi,cs,gdo0,gdo2
    bruceConfigPins.NRF24_bus = {
        (gpio_num_t)6, (gpio_num_t)7, (gpio_num_t)11, (gpio_num_t)10, (gpio_num_t)13
    }; // sck,miso,mosi,cs(ss),ce
    bruceConfigPins.badusb_bus = {(gpio_num_t)44, (gpio_num_t)43}; // rx, tx
    bruceConfigPins.uart_bus = {(gpio_num_t)44, (gpio_num_t)43};   // rx, tx
    bruceConfigPins.gps_bus = {(gpio_num_t)44, (gpio_num_t)43};    // rx, tx
    bruceConfigPins.irTx = 9;
    bruceConfigPins.irRx = 8;
    bruceConfigPins.rfTx = 8;
    bruceConfigPins.rfRx = 9;

    // TODO (stage 2): SD_MMC.begin() with CLK=12 CMD=16 D0=14 D1=17 D2=21 D3=18
    // TODO (stage 2): APA102 RGB LED driver, CLK=39 DIN=40 (e.g. FastLED <APA102>)

    pinMode(BTN_PIN, INPUT_PULLUP); // GPIO0 idles HIGH, pressed = LOW
}

/*********************************************************************
** Function: setBrightness
** set brightness value
**********************************************************************/
void _setBrightness(uint8_t brightval) {
    // Backlight is a plain active-LOW GPIO here, not a PWM rail — on/off only
    // for this first pass. TODO (stage 2): ledcWrite() on TFT_BL for real dimming.
    digitalWrite(TFT_BL, brightval > 0 ? LOW : HIGH);
}

/*********************************************************************
** Function: InputHandler
** Handles PrevPress, NextPress, SelPress, AnyKeyPress and EscPress
** derived from a SINGLE physical button (GPIO0).
**********************************************************************/
void InputHandler(void) {
    static bool lastState = HIGH;
    static unsigned long pressStart = 0;
    static unsigned long lastReleaseTime = 0;
    static unsigned long lastChangeTime = 0;
    static bool waitingForDoublePress = false;
    static bool longPressFired = false;

    const unsigned long DEBOUNCE_MS = 30;
    const unsigned long LONG_PRESS_MS = 500;
    const unsigned long DOUBLE_PRESS_WINDOW_MS = 350;

    bool state = digitalRead(BTN_PIN);

    // Simple electrical debounce: ignore state changes that happen too fast
    if (state != lastState && (millis() - lastChangeTime) < DEBOUNCE_MS) { return; }
    if (state != lastState) lastChangeTime = millis();

    // Button just pressed
    if (lastState == HIGH && state == LOW) {
        pressStart = millis();
        longPressFired = false;
        if (wakeUpScreen()) {
            lastState = state;
            return;
        }
    }

    // Button held -> fires once when it crosses the long-press threshold
    if (state == LOW && !longPressFired) {
        if (millis() - pressStart >= LONG_PRESS_MS) {
            longPressFired = true;
            waitingForDoublePress = false;
            AnyKeyPress = true;
            SelPress = true;
        }
    }

    // Button just released
    if (lastState == LOW && state == HIGH) {
        unsigned long heldFor = millis() - pressStart;
        if (!longPressFired && heldFor < LONG_PRESS_MS) {
            if (waitingForDoublePress && (millis() - lastReleaseTime) < DOUBLE_PRESS_WINDOW_MS) {
                // second short press within the window -> Esc/back
                AnyKeyPress = true;
                EscPress = true;
                waitingForDoublePress = false;
            } else {
                // first short press -> wait to see if a second one follows
                waitingForDoublePress = true;
                lastReleaseTime = millis();
            }
        }
    }

    // Double-press window expired with no second press -> resolve as a single Next
    if (waitingForDoublePress && (millis() - lastReleaseTime) >= DOUBLE_PRESS_WINDOW_MS) {
        AnyKeyPress = true;
        NextPress = true;
        waitingForDoublePress = false;
    }

    lastState = state;
}

/*********************************************************************
** Function: powerOff
** No battery/PMIC on this board — it's USB-powered. Nothing to switch off
** in software; the user unplugs it. Left as a no-op rather than calling
** esp_deep_sleep_start(), since re-waking a deep-sleeping dongle would need
** its own design (stage 2, if wanted).
**********************************************************************/
void powerOff() {}

void checkReboot() {}
