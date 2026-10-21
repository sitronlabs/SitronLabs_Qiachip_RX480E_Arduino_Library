/* Self header */
#include "rx480e.h"

/* Config */
#ifndef CONFIG_RX480E_LOG_FUNCTION
#define CONFIG_RX480E_LOG_FUNCTION(...) \
    (void)0  //!< Replace by { Serial.printf(__VA_ARGS__); Serial.println(); } to
             //!< output on serial port
#endif
#ifndef CONFIG_RX480E_KEY_PRESS_MS
#define CONFIG_RX480E_KEY_PRESS_MS 100  //!< Duration of each simulated press of the learning key
#endif
#ifndef CONFIG_RX480E_KEY_GAP_MS
#define CONFIG_RX480E_KEY_GAP_MS 150  //!< Delay between simulated presses of the learning key
#endif

/* Timings of the receiver, measured on a RX480E-4 */
#define RX480E_RESET_PRESSES 8                 //!< Number of presses that clear the codes
#define RX480E_RESET_FLASHES_MIN 4             //!< Flashes confirming a reset (4 in the manual, 5 measured)
#define RX480E_RESET_FEEDBACK_TIMEOUT_MS 3000  //!< Max delay between last press and first flash (1.2 s measured)
#define RX480E_LEARNING_START_TIMEOUT_MS 3000  //!< Max delay between last press and learning LED on (1.0 s measured)
#define RX480E_LEARNING_DURATION_MS 17000      //!< Learning state ends by itself after 16.5 s
#define RX480E_LEARNING_EXPIRED_MS 16000       //!< Leaving learning state after this delay means that nothing was received
#define RX480E_LEARNED_FLASH_WINDOW_MS 600     //!< Window in which the flashes of a new keyfob are counted (2 flashes within 0.4 s measured)
#define RX480E_LEARNED_FLASHES_MIN 2           //!< Flashes after the LED turned off that mean "new keyfob" (3 flashes in total, first one is merged with the learning LED)
#define RX480E_FLASHES_QUIET_MS 500            //!< LED stays off this long after the last flash (flashes have a 200 ms period)
#define RX480E_IDLE_TIMEOUT_MS 17000           //!< Max time to wait for the receiver to be idle (learning state lasts 16.5 s)
#define RX480E_IDLE_STABLE_MS 100              //!< LED must stay off this long to be considered idle
#define RX480E_RECEIVING_TIMEOUT_MS 5000       //!< Max time to wait for the keyfob to be released after learning

/**
 * @brief Initializes the library with the pin connected to VT.
 *
 * The pin is released (high impedance) so that the LED state can be read.
 *
 * @param[in] vt_pin Arduino pin connected to VT through a 1 kOhm resistor
 * @return 0 on success, or a negative error code otherwise
 */
int rx480e::setup(const uint8_t vt_pin) {
    /* Save parameters */
    m_pin = (int8_t)vt_pin;

    /* Release the pin */
    key_up();

    /* Return success */
    return 0;
}

/**
 * Registers a function called each time the receiver enters learning state.
 * @param[in] callback Function to call, or NULL to disable.
 */
void rx480e::set_learning_callback(void (*callback)(void)) {
    m_callback = callback;
}

/**
 * @brief Clears all the paired keyfobs.
 *
 * Presses the learning key 8 times, then checks that the LED flashes. The
 * manual announces 4 flashes, 5 have been measured, so at least 4 are required.
 *
 * @return 0 in case of success, or a negative error code otherwise.
 */
int rx480e::reset_pairing(void) {
    int res;

    /* Ensure library has been configured */
    if (m_pin < 0) {
        return -EINVAL;
    }

    /* Presses are ignored while the receiver is learning or receiving */
    res = wait_idle();
    if (res < 0) {
        return res;
    }

    /* Press the key */
    res = key_press_count(RX480E_RESET_PRESSES);
    if (res < 0) {
        return res;
    }

    /* Check the confirmation flashes */
    uint8_t flashes = 0;
    res = count_flashes(RX480E_RESET_FEEDBACK_TIMEOUT_MS, flashes);
    if (res < 0) {
        return res;
    }
    if (flashes < RX480E_RESET_FLASHES_MIN) {
        CONFIG_RX480E_LOG_FUNCTION("Reset not confirmed, %u flashes", (unsigned int)flashes);
        return -EIO;
    }

    /* Return success */
    return 0;
}

/**
 * @brief Pairs a keyfob button.
 *
 * Selects the mode by pressing the learning key, waits for the learning LED,
 * then waits for the keyfob button. The outcome is decoded from the LED:
 * - New keyfob button: the LED turns off, then flashes 3 times in total
 *   (the first flash is hidden by the LED being on).
 * - Already paired keyfob button: the LED turns off without further flashes.
 * - Nothing received: the receiver leaves learning state after 16.5 s.
 *
 * @param[in] mode Operating mode to apply to the new keyfob button
 * @return 0 if paired, -EEXIST if already paired, -ETIMEDOUT if no keyfob has
 * been pressed, or another negative error code otherwise.
 */
int rx480e::pair(const enum rx480e_mode mode) {
    int res;

    /* Ensure library has been configured */
    if (m_pin < 0) {
        return -EINVAL;
    }
    if (mode < RX480E_MODE_MOMENTARY || mode > RX480E_MODE_TOGGLE_AND_INTERLOCK) {
        return -EINVAL;
    }

    /* Presses are ignored while the receiver is learning or receiving */
    res = wait_idle();
    if (res < 0) {
        return res;
    }

    /* Select the mode, the receiver enters learning state when the presses stop */
    res = key_press_count((uint8_t)mode);
    if (res < 0) {
        return res;
    }
    if (wait_led(true, RX480E_LEARNING_START_TIMEOUT_MS) < 0) {
        CONFIG_RX480E_LOG_FUNCTION("Learning state not entered");
        return -EIO;
    }
    if (m_callback != NULL) {
        m_callback();
    }

    /* Wait for the LED to turn off, which happens when a keyfob has been received or when the receiver gives up */
    const uint32_t learning_start = millis();
    if (wait_led(false, RX480E_LEARNING_DURATION_MS) < 0) {
        return -EIO;
    }
    const uint32_t learning_time = millis() - learning_start;
    if (learning_time >= RX480E_LEARNING_EXPIRED_MS) {
        CONFIG_RX480E_LOG_FUNCTION("Learning timed out");
        return -ETIMEDOUT;
    }

    /* Count the rising edges that follow, a new keyfob makes the LED flash two more times */
    uint8_t flashes = 0;
    bool previous = false;
    const uint32_t window_start = millis();
    while ((millis() - window_start) < RX480E_LEARNED_FLASH_WINDOW_MS) {
        bool on;
        res = read_led(on);
        if (res < 0) {
            return res;
        }
        if (on && !previous) {
            flashes++;
        }
        previous = on;
    }

    /* Wait for the flashes to end, or for a held keyfob to be released */
    const uint32_t settle_start = millis();
    uint32_t off_since = millis();
    while ((millis() - off_since) < RX480E_IDLE_STABLE_MS) {
        bool on;
        res = read_led(on);
        if (res < 0) {
            return res;
        }
        if (on) {
            off_since = millis();
        }
        if ((millis() - settle_start) > RX480E_RECEIVING_TIMEOUT_MS) {
            break;
        }
    }

    /* Return the outcome */
    if (flashes >= RX480E_LEARNED_FLASHES_MIN) {
        return 0;
    }
    return -EEXIST;
}

/**
 * @brief Changes the operating mode.
 *
 * The mode is stored with the paired keyfob buttons, so the pairing memory is
 * cleared first, and a keyfob button is paired in the new mode.
 *
 * @param[in] mode New operating mode
 * @return 0 in case of success, or a negative error code otherwise.
 */
int rx480e::set_mode(const enum rx480e_mode mode) {
    /* Validate before clearing anything */
    if (mode < RX480E_MODE_MOMENTARY || mode > RX480E_MODE_TOGGLE_AND_INTERLOCK) {
        return -EINVAL;
    }

    /* Clear all pairings */
    int res = reset_pairing();
    if (res < 0) {
        return res;
    }

    /* Pair a keyfob button in the new mode */
    return pair(mode);
}

/**
 * Presses the learning key, holds it, and releases it.
 * @param[in] duration_ms Hold duration
 * @return 0 in case of success, or a negative error code otherwise.
 */
int rx480e::key_press(const uint32_t duration_ms) {
    /* Ensure library has been configured */
    if (m_pin < 0) {
        return -EINVAL;
    }

    key_down();
    delay(duration_ms);
    key_up();

    /* Return success */
    return 0;
}

/**
 * Reads the LED state through the VT pin.
 * @param[out] on True if the LED is on
 * @return 0 in case of success, or a negative error code otherwise.
 */
int rx480e::read_led(bool &on) {
    /* Ensure library has been configured */
    if (m_pin < 0) {
        return -EINVAL;
    }

    on = (digitalRead(m_pin) != LOW);

    /* Return success */
    return 0;
}

/**
 * Drives VT high, which is what the learning key does.
 */
void rx480e::key_down(void) {
    digitalWrite(m_pin, HIGH);
    pinMode(m_pin, OUTPUT);
}

/**
 * Releases VT. The output latch is cleared first so that no pull-up is
 * enabled by the high output value on some architectures (AVR).
 */
void rx480e::key_up(void) {
    digitalWrite(m_pin, LOW);
    pinMode(m_pin, INPUT);
}

/**
 * Presses the learning key several times, and leaves it released.
 * @param[in] count Number of presses
 * @return 0 in case of success, or a negative error code otherwise.
 */
int rx480e::key_press_count(const uint8_t count) {
    for (uint8_t i = 0; i < count; i++) {
        key_press(CONFIG_RX480E_KEY_PRESS_MS);
        if ((i + 1) < count) {
            delay(CONFIG_RX480E_KEY_GAP_MS);
        }
    }
    return 0;
}

/**
 * Waits for the LED to stay off for a short time. This is the case when the
 * receiver is idle. It is not the case while receiving a keyfob or while
 * learning, during which key presses are ignored by the receiver.
 * @return 0 if idle, -EBUSY if the LED stays on, or a negative error code otherwise.
 */
int rx480e::wait_idle(void) {
    const uint32_t start = millis();
    uint32_t off_since = millis();
    while ((millis() - off_since) < RX480E_IDLE_STABLE_MS) {
        bool on;
        int res = read_led(on);
        if (res < 0) {
            return res;
        }
        if (on) {
            off_since = millis();
        }
        if ((millis() - start) > RX480E_IDLE_TIMEOUT_MS) {
            return -EBUSY;
        }
    }
    return 0;
}

/**
 * Waits for the LED to reach a level.
 * @param[in] level Level to wait for
 * @param[in] timeout_ms Maximum time to wait
 * @return Time waited in milliseconds, or -ETIMEDOUT.
 */
int32_t rx480e::wait_led(const bool level, const uint32_t timeout_ms) {
    const uint32_t start = millis();
    while ((millis() - start) < timeout_ms) {
        bool on;
        int res = read_led(on);
        if (res < 0) {
            return res;
        }
        if (on == level) {
            return (int32_t)(millis() - start);
        }
    }
    return -ETIMEDOUT;
}

/**
 * Counts the flashes of the LED. The first flash must start within the given
 * time, and the count ends when the LED has been off for a while.
 * @param[in] first_timeout_ms Maximum time to wait for the first flash
 * @param[out] count Number of flashes seen (0 if none)
 * @return 0 in case of success, or a negative error code otherwise.
 */
int rx480e::count_flashes(const uint32_t first_timeout_ms, uint8_t &count) {
    count = 0;

    /* Wait for the first flash */
    if (wait_led(true, first_timeout_ms) < 0) {
        return 0;
    }
    count = 1;

    /* Count the next ones until the LED stays off */
    bool previous = true;
    uint32_t off_since = millis();
    while ((millis() - off_since) < RX480E_FLASHES_QUIET_MS) {
        bool on;
        int res = read_led(on);
        if (res < 0) {
            return res;
        }
        if (on) {
            if (!previous) {
                count++;
            }
            off_since = millis();
        }
        previous = on;
    }

    /* Return success */
    return 0;
}
