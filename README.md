[![Designed by Sitron Labs](https://img.shields.io/badge/Designed_by-Sitron_Labs-FCE477.svg)](https://www.sitronlabs.com/)
[![Join the Discord community](https://img.shields.io/discord/552242187665145866.svg?logo=discord&logoColor=white&label=Discord&color=%237289da)](https://discord.gg/btnVDeWhfW)
[![PayPal Donate](https://img.shields.io/badge/PayPal-Donate-00457C.svg?logo=paypal&logoColor=white)](https://www.paypal.com/donate/?hosted_button_id=QLX8VU9Q3PFFL)
![License](https://img.shields.io/github/license/sitronlabs/SitronLabs_Qiachip_RX480E_Arduino_Library.svg)
![Latest Release](https://img.shields.io/github/release/sitronlabs/SitronLabs_Qiachip_RX480E_Arduino_Library.svg)
[![Arduino Library Manager](https://www.ardu-badge.com/badge/Sitron%20Labs%20RX480E%20Arduino%20Library.svg)](https://www.ardu-badge.com/Sitron%20Labs%20RX480E%20Arduino%20Library)
[![PlatformIO Registry](https://badges.registry.platformio.org/packages/sitronlabs/library/Sitron_Labs_RX480E_Arduino_Library.svg)](https://registry.platformio.org/libraries/sitronlabs/Sitron_Labs_RX480E_Arduino_Library)

# Sitron Labs Qiachip RX480E Arduino Library

Arduino library for programming the Qiachip RX480E (RX480E-4) 433.92 MHz EV1527 learning receiver from a microcontroller, using a single GPIO connected to the VT pin.

## Description

The RX480E decodes EV1527 keyfobs and exposes four outputs (D0 to D3). It has no digital interface: it is programmed with a learning key and an LED. Both are wired on the dual-use VT pin, which this library exploits to simulate the key and read back the LED with one GPIO:

- Simulated key press: the GPIO drives VT high (through a 1 kΩ resistor).
- Key released and LED readback: the GPIO is left as a high impedance input, so VT follows the LED (high = LED on).

The library offers blocking functions that perform the whole programming sequences and decode the LED feedback:
- Clear all pairings (factory reset)
- Pair a keyfob button, and tell apart a new button from an already paired one
- Select the operating mode (momentary, toggle, interlock and mixed modes)

The D0 to D3 outputs are not used by the library, they are read directly by your application.

## Installation

### Arduino IDE

Install via the Arduino Library Manager by searching for "Sitron Labs RX480E".

Alternatively, install manually:
1. Download or clone this repository
2. Place it in your Arduino `libraries` folder
3. Restart the Arduino IDE

### PlatformIO

Install via the PlatformIO Library Manager by searching for "Sitron Labs RX480E".

Alternatively, add the library manually to your `platformio.ini` file:

```ini
lib_deps =
    https://github.com/sitronlabs/SitronLabs_Qiachip_RX480E_Arduino_Library.git
```

## Hardware Connections

| RX480E | Microcontroller |
|--------|-----------------|
| +V     | 3.3V or 5V (3.3V to 5V supported) |
| GND    | GND |
| VT     | Any GPIO, **through a 1 kΩ resistor** |
| D0-D3  | Optional, GPIOs used as inputs by your application |
| ANT    | Antenna: 17.3 cm of wire for 433.92 MHz |

Notes:
- The 1 kΩ resistor is the same value as the one of the on-board key, and limits the current if the receiver drives VT while the GPIO is driving it.
- Power the receiver from the same voltage as the microcontroller logic. On 3.3 V microcontrollers that are not 5 V tolerant, never power the receiver with 5 V.
- The antenna is required, otherwise the receiver barely sees the keyfob.

## Usage

### Basic Example

```cpp
#include <rx480e.h>

rx480e receiver;

void setup() {
  Serial.begin(115200);

  // VT pin on pin 8, through a 1k resistor
  receiver.setup(8);

  // Forget all keyfobs
  if (receiver.reset_pairing() < 0) {
    Serial.println("Reset failed, check the wiring");
    return;
  }

  // Learn one keyfob button, press it when the LED of the receiver turns on
  int res = receiver.pair(RX480E_MODE_MOMENTARY);
  if (res == 0) Serial.println("Paired");
  else if (res == -EEXIST) Serial.println("Already paired");
  else if (res == -ETIMEDOUT) Serial.println("No keyfob pressed");
}

void loop() {}
```

### Changing the mode

```cpp
// Forget all keyfobs, then pair one in toggle mode
receiver.set_mode(RX480E_MODE_TOGGLE);

// Add another button (the mode is stored per newly learned button)
receiver.pair(RX480E_MODE_TOGGLE);
```

### Prompting the user

```cpp
void on_learning() { digitalWrite(LED_BUILTIN, HIGH); }  // "press the keyfob now"

receiver.set_learning_callback(on_learning);
```

See the Advanced example for a serial menu that exercises every function.

## How the receiver behaves

These behaviours were measured on a RX480E-4 (433.92 MHz), the library relies on them.

| Action | What the library does | Receiver feedback (LED) |
|--------|-----------------------|-------------------------|
| Clear codes | 8 key presses | About 1.2 s after the last press, 5 flashes (100 ms on, 100 ms off). The manuals say 4. |
| Select mode N (1 to 6) | N key presses | About 1.0 s after the last press, the LED turns on: learning state |
| Keyfob pressed, **new** | | The LED turns off, then flashes 2 more times (3 flashes in total, the first one is hidden by the learning LED), the receiver leaves learning state |
| Keyfob pressed, **already paired** | | The LED turns off, nothing else (the receiver leaves learning state, nothing stored, the mode is not changed). The LED may light up again if the button is still pressed. |
| Nothing pressed | | The LED turns off by itself after about 16.5 s |

Other measured details:
- The learning key is ignored while the receiver is in learning state or receiving a keyfob. The library waits for the LED to be off before starting.
- The mode is stored with each newly learned button. Selecting another mode when the button is already paired has no effect, which is why `set_mode()` clears the pairings first.
- Modes 1 (momentary) and 2 (toggle) were verified on the outputs. Modes 3 to 6 are accepted by the receiver (the LED goes into learning state), and a keyfob button was paired in mode 3 with the usual 3 flashes, but the output behaviour of modes 3 to 6 has not been verified. Qiachip's manual says mode 3 needs two different buttons (ON and OFF, 5 flashes each), but a single keyfob button was learned with the usual 3 flashes on the module tested.
- With only one keyfob, the "memory full" feedback (4 stored buttons) could not be tested.
- Presses of 100 ms with 150 ms between them are used. These can be changed with `CONFIG_RX480E_KEY_PRESS_MS` and `CONFIG_RX480E_KEY_GAP_MS`.

## API Reference

### setup(uint8_t vt_pin)

Initializes the library and releases the pin (high impedance).

- `vt_pin`: Pin connected to VT through a 1 kΩ resistor

Returns 0 on success, or a negative error code otherwise.

### set_learning_callback(void (*callback)(void))

Registers a function called each time the receiver enters learning state, i.e. when the keyfob button has to be pressed. Pass `NULL` to disable.

### reset_pairing(void)

Clears all the paired keyfobs (8 presses) and checks the confirmation flashes. Blocks for about 5 seconds.

Returns 0 on success, -EBUSY if the LED stays on, -EIO if the flashes are not seen, or another negative error code.

### pair(enum rx480e_mode mode = RX480E_MODE_MOMENTARY)

Selects the mode and waits for a keyfob button to be pressed. Blocks until the button is pressed, or for about 16.5 seconds.

- `mode`: Mode applied to a new keyfob button

Returns:
- 0 if a new keyfob button has been paired
- `-EEXIST` if the button was already paired (nothing changed)
- `-ETIMEDOUT` if no button was pressed
- `-EBUSY` if the receiver stays busy
- `-EIO` if the receiver did not enter learning state (check wiring)
- `-EINVAL` if the mode is invalid

### set_mode(enum rx480e_mode mode)

Clears all pairings with `reset_pairing()`, then pairs a button in the new mode with `pair()`. Returns the same as `pair()`, or the error of `reset_pairing()`.

### key_press(uint32_t duration_ms)

Presses the learning key for the given time, then releases it. Returns 0 on success.

### read_led(bool &on)

Reads the LED state through VT (the pin must not be pressed). Returns 0 on success.

### Modes

| Value | Number of presses | Behaviour |
|-------|-------------------|-----------|
| `RX480E_MODE_MOMENTARY` | 1 | Output high while the keyfob button is pressed |
| `RX480E_MODE_TOGGLE` | 2 | Each press toggles the output (self-locking) |
| `RX480E_MODE_INTERLOCK` | 3 | Like toggle, but only one output on at a time |
| `RX480E_MODE_TOGGLE_AND_MOMENTARY` | 4 | Two outputs toggle, two are momentary |
| `RX480E_MODE_MOMENTARY_AND_INTERLOCK` | 5 | Two outputs momentary, two interlocked |
| `RX480E_MODE_TOGGLE_AND_INTERLOCK` | 6 | Two outputs toggle, two interlocked |

Note that the receiver assigns keyfob buttons to D0 to D3 on its own, not necessarily in pairing order.

## Troubleshooting

- **-EIO when pairing, or reset not confirmed**: check that VT goes through a 1 kΩ resistor, and that the receiver is powered. The red LED blinks once at power-up.
- **-EBUSY**: the LED stays on, for example because a keyfob button is held, or VT is stuck high.
- **-EEXIST on a keyfob you want to change**: the mode of an already paired button cannot be changed, use `set_mode()` (this clears all pairings).
- **-ETIMEDOUT**: the keyfob was not received. Check the antenna, the frequency (433.92 MHz receiver and keyfob) and the battery of the keyfob.
- **AVR boards**: avr-libc defines all the `E*` error codes to the same dummy value, so the header redefines `EIO`, `EBUSY`, `EEXIST`, `EINVAL` and `ETIMEDOUT` to their usual values on AVR.

## Specifications

- Frequency: 433.92 MHz (RX480E-4)
- Modulation: ASK/OOK, EV1527 and PT2262 keyfobs
- Supply voltage: 3.3 V to 5 V
- Quiescent current: 5 mA
- Outputs: 4 channels, CMOS level
- Sensitivity: -108 dBm
