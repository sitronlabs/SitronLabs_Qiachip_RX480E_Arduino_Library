#ifndef RX480E_H
#define RX480E_H

/* Arduino libraries */
#include <Arduino.h>

/* C/C++ libraries */
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

/*
 * On AVR, avr-libc defines every error code (EIO, EBUSY...) as the same dummy
 * value (ENOERR), which would make errors indistinguishable from each other.
 * The standard values are used instead in that case.
 */
#ifdef ENOERR
#undef EIO
#undef EBUSY
#undef EEXIST
#undef EINVAL
#undef ETIMEDOUT
#define EIO 5
#define EBUSY 16
#define EEXIST 17
#define EINVAL 22
#define ETIMEDOUT 110
#endif

/**
 * @brief RX480E operating mode
 *
 * The operating mode defines how the four outputs (D0 to D3) behave when the
 * corresponding keyfob button is pressed. The value is the number of times the
 * learning key has to be pressed to select the mode.
 *
 * The mode is stored together with the keyfob button that gets learned right
 * after it has been selected. It can therefore only be changed for new keyfob
 * buttons, after the pairing memory has been cleared with
 * rx480e::reset_pairing().
 */
enum rx480e_mode {
    RX480E_MODE_MOMENTARY = 1,                    //!< Outputs are high only while the keyfob button is pressed
    RX480E_MODE_TOGGLE = 2,                       //!< Each keyfob press toggles the output (self-locking)
    RX480E_MODE_INTERLOCK = 3,                    //!< Like toggle, but turning on one output turns off the others (mutually exclusive)
    RX480E_MODE_TOGGLE_AND_MOMENTARY = 4,         //!< Two outputs toggle, two outputs are momentary
    RX480E_MODE_MOMENTARY_AND_INTERLOCK = 5,      //!< Two outputs are momentary, two outputs are interlocked
    RX480E_MODE_TOGGLE_AND_INTERLOCK = 6,         //!< Two outputs toggle, two outputs are interlocked
};

/**
 * @brief Qiachip RX480E 433.92 MHz EV1527 learning receiver - Main Control Class
 *
 * The RX480E has a single dual-use pin, VT. It is an output that follows the
 * state of the on-board LED (high while a valid keyfob code is received, and
 * while the receiver shows its status during programming), and it is also the
 * input of the on-board learning key. A press of the key is simulated by
 * driving VT high through a resistor, which is why this library only needs
 * one GPIO.
 *
 * Hardware:
 * - Connect the GPIO to VT through a 1 kOhm resistor.
 * - The GPIO is either driven high (key pressed), or left as a high impedance
 *   input (key released, LED state can be read).
 *
 * All the programming functions are blocking. They simulate presses of the
 * learning key and decode the flashes of the LED as feedback:
 * - Clearing the codes: 8 presses, then the LED flashes 5 times.
 * - Selecting a mode: 1 to 6 presses, then the LED turns on (learning state).
 * - Learning a new keyfob button: the LED turns off, then flashes 3 times.
 * - Learning an already paired keyfob button: the LED turns off without the
 *   3 flashes, and nothing is stored.
 * - If no keyfob is pressed within about 16 s the receiver leaves the learning
 *   state by itself.
 *
 * Basic Usage:
 * 1. Call setup() with the VT pin
 * 2. Call reset_pairing() to clear all keyfobs
 * 3. Call pair() with the desired mode, and press the keyfob button when the
 *    receiver is in learning state (the LED is on)
 */
class rx480e {
   public:
    /** @name Setup Functions
     * Basic initialization
     */
    /**@{*/

    /**
     * Initialize the library and release the VT pin (high impedance)
     * @param vt_pin Arduino pin connected to VT through a 1 kOhm resistor
     * @return 0 on success, negative error code on failure
     */
    int setup(const uint8_t vt_pin);

    /**
     * Register a function called each time the receiver enters its learning
     * state, i.e. when the keyfob button has to be pressed.
     * Can be used to prompt the user.
     * @param callback Function to call, or NULL to disable
     */
    void set_learning_callback(void (*callback)(void));

    /**@}*/

    /** @name Programming Functions
     * Blocking functions that simulate presses of the learning key and decode
     * the LED feedback
     */
    /**@{*/

    /**
     * Clear all the paired keyfobs and restore the factory settings
     *
     * Presses the learning key 8 times and checks that the LED flashes to
     * confirm. Blocks for about 5 seconds.
     * @return 0 on success, -EBUSY if the receiver stays busy (LED on),
     * -EIO if the confirmation flashes were not seen, or another negative error code
     */
    int reset_pairing(void);

    /**
     * Pair a keyfob button
     *
     * Selects the mode, then waits for the keyfob button to be pressed while the
     * receiver is in its learning state (LED on). The receiver stores up to
     * four different keyfob buttons. The mode is only applied when the button
     * is new, and is ignored (the stored mode is kept) when the button is
     * already paired. Blocks until the button has been pressed, or until the
     * receiver gives up after about 16 seconds.
     * @param mode Operating mode to apply to the new keyfob button
     * @return 0 if a new keyfob button has been paired,
     * -EEXIST if the keyfob button was already paired (nothing changed),
     * -ETIMEDOUT if no keyfob button has been pressed in time,
     * -EBUSY if the receiver stays busy (LED on),
     * -EIO if the receiver did not enter learning state,
     * -EINVAL if the mode is invalid, or another negative error code
     */
    int pair(const enum rx480e_mode mode = RX480E_MODE_MOMENTARY);

    /**
     * Change the operating mode
     *
     * The mode is attached to the paired keyfob buttons, so this clears all
     * pairings with reset_pairing() and then calls pair() to learn a keyfob
     * button in the new mode. Other buttons can be added afterwards with pair().
     * @param mode New operating mode
     * @return Same as pair(), or the error of reset_pairing()
     */
    int set_mode(const enum rx480e_mode mode);

    /**@}*/

    /** @name Low Level Functions
     * Direct access to the learning key and to the LED
     */
    /**@{*/

    /**
     * Press the learning key, hold it, and release it
     * @param duration_ms How long to hold the key
     * @return 0 on success, negative error code on failure
     */
    int key_press(const uint32_t duration_ms);

    /**
     * Read the LED state, which is also the VT output of the receiver
     *
     * The pin is left in high impedance, so this is only meaningful while the
     * key is not pressed.
     * @param on Output: true if the LED is on
     * @return 0 on success, negative error code on failure
     */
    int read_led(bool &on);

    /**@}*/

   protected:
    /** Drives VT high to simulate the key being pressed */
    void key_down(void);

    /** Releases VT (high impedance) */
    void key_up(void);

    /** Presses the key count times, and leaves it released */
    int key_press_count(const uint8_t count);

    /** Waits until the LED is off, to make sure the receiver is idle */
    int wait_idle(void);

    /** Waits for the LED to reach a level, returns the waited time in ms, or -ETIMEDOUT */
    int32_t wait_led(const bool level, const uint32_t timeout_ms);

    /** Counts the LED flashes until the LED stays off for a while */
    int count_flashes(const uint32_t first_timeout_ms, uint8_t &count);

    int8_t m_pin = -1;                   //!< Pin connected to VT, or -1 if not set up
    void (*m_callback)(void) = NULL;     //!< Called when learning state is entered
};

#endif
