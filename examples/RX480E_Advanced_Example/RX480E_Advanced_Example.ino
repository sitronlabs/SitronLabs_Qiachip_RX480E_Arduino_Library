/*
 * RX480E Advanced Example
 *
 * This example lets you drive the receiver programming from the serial
 * monitor: clear the pairings, pair keyfob buttons, and change the operating
 * mode. It also shows the live state of the LED while idle.
 *
 * Serial commands (send one character):
 * - r : Clear all pairings (factory reset)
 * - 1 : Pair a keyfob button, momentary mode
 * - 2 : Pair a keyfob button, toggle (self-locking) mode
 * - 3 : Pair a keyfob button, interlock (mutually exclusive) mode
 * - 4 : Pair a keyfob button, 2 toggle + 2 momentary
 * - 5 : Pair a keyfob button, 2 momentary + 2 interlock
 * - 6 : Pair a keyfob button, 2 toggle + 2 interlock
 * - m : Change mode: clear all pairings, then pair a button in the mode given
 *       by the next character (1 to 6)
 *
 * The mode is stored with each paired button, and it is ignored when the
 * button is already paired. To change the mode of a button, use 'm'.
 *
 * Hardware Connections:
 * - RX480E VT to pin 8 through a 1 kOhm resistor
 * - RX480E +V to 3.3V or 5V
 * - RX480E GND to GND
 *
 * Author: Sitron Labs
 * Date: 2025
 */

#include <rx480e.h>

/* RX480E instance */
rx480e receiver;

/* Pin connected to VT through a 1 kOhm resistor */
const uint8_t VT_PIN = 8;

/* Called when the receiver is waiting for the keyfob button */
void on_learning(void) {
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.println("Receiver is learning, press the keyfob button now...");
}

void print_result(const int res) {
    digitalWrite(LED_BUILTIN, LOW);
    switch (res) {
        case 0:
            Serial.println("Result: new keyfob button paired");
            break;
        case -EEXIST:
            Serial.println("Result: keyfob button was already paired (nothing changed)");
            break;
        case -ETIMEDOUT:
            Serial.println("Result: no keyfob button pressed in time");
            break;
        case -EBUSY:
            Serial.println("Result: receiver busy (LED stays on)");
            break;
        case -EIO:
            Serial.println("Result: no LED feedback from the receiver, check wiring");
            break;
        default:
            Serial.print("Result: error ");
            Serial.println(res);
            break;
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial) {
        delay(10);
    }
    pinMode(LED_BUILTIN, OUTPUT);

    Serial.println("RX480E Advanced Example");
    Serial.println("=======================");

    if (receiver.setup(VT_PIN) < 0) {
        Serial.println("Failed to setup RX480E!");
        while (1) {
            delay(1000);
        }
    }
    receiver.set_learning_callback(on_learning);

    Serial.println("Commands: r = reset, 1-6 = pair in mode, m<1-6> = reset then pair in mode");
}

void loop() {
    /* Show the LED state while idle */
    static bool previous_led = false;
    bool led;
    if (receiver.read_led(led) == 0 && led != previous_led) {
        previous_led = led;
        Serial.println(led ? "LED on" : "LED off");
    }

    /* Handle commands */
    if (Serial.available() > 0) {
        const char c = Serial.read();
        if (c == 'r') {
            Serial.println("Clearing all pairings...");
            const int res = receiver.reset_pairing();
            Serial.println(res == 0 ? "Result: pairings cleared" : "Result: reset failed");
        } else if (c >= '1' && c <= '6') {
            Serial.print("Pairing in mode ");
            Serial.println(c);
            print_result(receiver.pair((enum rx480e_mode)(c - '0')));
        } else if (c == 'm') {
            while (Serial.available() == 0) {
                delay(1);
            }
            const char m = Serial.read();
            if (m >= '1' && m <= '6') {
                Serial.print("Changing to mode ");
                Serial.println(m);
                print_result(receiver.set_mode((enum rx480e_mode)(m - '0')));
            }
        }
    }
}
