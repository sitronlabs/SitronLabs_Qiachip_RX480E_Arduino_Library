/*
 * RX480E Test Sketch
 *
 * This is a simple test sketch to verify the RX480E library is working
 * correctly. It performs basic functionality tests without requiring actual
 * hardware.
 *
 * Author: Sitron Labs
 * Date: 2025
 */

#include <rx480e.h>

/* RX480E instance */
rx480e receiver;

void setup() {
    /* Initialize serial communication */
    Serial.begin(115200);
    while (!Serial) {
        delay(10);
    }

    Serial.println("RX480E Library Test");
    Serial.println("===================");

    /* Test 1: Library instantiation */
    Serial.println("Test 1: Library instantiation...");
    if (receiver.setup(8) == 0) {
        Serial.println("  [OK] Library instantiation successful");
    } else {
        Serial.println("  [FAIL] Library instantiation failed");
    }

    /* Test 2: Enum values */
    Serial.println("Test 2: Enum values...");
    Serial.print("  Modes: ");
    Serial.print(RX480E_MODE_MOMENTARY);
    Serial.print(", ");
    Serial.print(RX480E_MODE_TOGGLE);
    Serial.print(", ");
    Serial.println(RX480E_MODE_TOGGLE_AND_INTERLOCK);
    Serial.println("  [OK] Enum values accessible");

    /* Test 3: Invalid parameters are rejected without touching the receiver */
    Serial.println("Test 3: Parameter validation...");
    if (receiver.pair((enum rx480e_mode)0) == -EINVAL && receiver.set_mode((enum rx480e_mode)9) == -EINVAL) {
        Serial.println("  [OK] Invalid modes rejected");
    } else {
        Serial.println("  [FAIL] Invalid modes accepted");
    }

    Serial.println("Tests complete.");
}

void loop() {
    delay(1000);
}
