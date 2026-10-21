/*
 * RX480E Basic Example
 *
 * This example clears all the pairings of the receiver, then pairs a keyfob
 * button in momentary mode (the outputs are high while the button is pressed).
 *
 * Hardware Connections:
 * - RX480E VT to pin 8 through a 1 kOhm resistor
 * - RX480E +V to 3.3V or 5V
 * - RX480E GND to GND
 * - An antenna on the RX480E ANT pad (17.3 cm of wire for 433.92 MHz)
 *
 * Author: Sitron Labs
 * Date: 2025
 */

#include <rx480e.h>

/* RX480E instance */
rx480e receiver;

/* Pin connected to VT through a 1 kOhm resistor */
const uint8_t VT_PIN = 8;

void setup() {
    /* Initialize serial communication */
    Serial.begin(115200);
    while (!Serial) {
        delay(10);
    }

    Serial.println("RX480E Basic Example");
    Serial.println("====================");

    /* Setup the receiver */
    if (receiver.setup(VT_PIN) < 0) {
        Serial.println("Failed to setup RX480E!");
        while (1) {
            delay(1000);
        }
    }

    /* Clear all pairings */
    Serial.println("Clearing all pairings...");
    if (receiver.reset_pairing() < 0) {
        Serial.println("Reset failed! Check the wiring (VT through 1k) and the power supply.");
        while (1) {
            delay(1000);
        }
    }
    Serial.println("Pairings cleared.");

    /* Pair a keyfob button */
    Serial.println("Press a keyfob button once the receiver LED is on (up to 16 seconds)...");
    const int res = receiver.pair(RX480E_MODE_MOMENTARY);
    if (res == 0) {
        Serial.println("Keyfob button paired!");
    } else if (res == -EEXIST) {
        Serial.println("Keyfob button was already paired.");
    } else if (res == -ETIMEDOUT) {
        Serial.println("No keyfob button pressed in time.");
    } else {
        Serial.print("Pairing failed with error ");
        Serial.println(res);
    }
}

void loop() {
    /* The receiver now works on its own, VT follows the LED */
    bool led;
    if (receiver.read_led(led) == 0) {
        Serial.println(led ? "Keyfob received" : "Idle");
    }
    delay(500);
}
