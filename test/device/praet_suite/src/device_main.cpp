/* MMgr - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file device_main.cpp
 * @brief Brings up a serial port, runs the praet correctness suite on the part, and stops.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-09-01
 *
 * @note The only file here that is not the suite. Everything under test is compiled from where it
 *       already lives, so the part runs the same source the host does and there is no second copy to
 *       drift.
 * @note C++ because the Arduino core is, and it is the shortest way to a serial port that works the
 *       same on every board. Nothing under test is C++ and none of it is compiled as C++.
 */
#include <Arduino.h>

extern "C"
{
    /**
     * @brief Sends one character to the serial port, which is where Unity writes.
     *
     * @param[in] letter Character to send, as Unity hands it over.
     * @note UNITY_OUTPUT_CHAR names this. Unity emits a character at a time and never a line, so
     *       buffering is the port's business.
     */
    void device_putchar(int letter);

    /**
     * @brief The suite's own entry, which is the generated runner's main under another name.
     *
     * @return The number of cases that failed, as Unity counts them.
     * @note platformio.ini renames it with -Dmain=praet_suite_main, because the Arduino core already
     *       has a main and the runner is generated with one.
     */
    int praet_suite_main(void);
}

void device_putchar(int letter)
{
    Serial.write((uint8_t)letter);
}

/**
 * @brief Waits for the serial port, runs every case once, and reports what the part said.
 *
 * @note Waits for a host to open the port, with a ceiling. A board nobody is watching still runs and
 *       still reports, which is what lets this be flashed and read later.
 * @note Runs once. The suite is not a loop, and a part that reran it would fill a capture with
 *       repeats of the same answer.
 */
/**
 * @brief What the run reported, kept so a listener that arrived late can still be told.
 */
static int s_failed;

void setup(void)
{
    Serial.begin(115200);

    const unsigned long waited_from = millis();

    // A minute, not five seconds. On a native USB part nothing resets when the host opens the port,
    // so a suite that ran before anyone was listening printed into nothing and the board looks dead.
    // Measured, on a Feather M4 that had already run by the time the port was opened.
    while (!Serial && ((millis() - waited_from) < 60000ul))
    {
        // Nothing. A UART part never asserts this at all, which is what the ceiling is for
    }

    Serial.println();
    Serial.println("DB ==== praet correctness suite, on the part ====");

    s_failed = praet_suite_main();

    Serial.print("DB ==== done, failures: ");
    Serial.print(s_failed);
    Serial.println(" ====");
    Serial.flush();
}

/**
 * @brief Repeats the verdict, so attaching after the run still tells you how it went.
 *
 * @note The case by case output is streamed and gone. This is the summary alone, which is what a
 *       script reads to decide whether the part passed.
 */
void loop(void)
{
    delay(3000);
    Serial.print("DB ==== ran already, failures: ");
    Serial.print(s_failed);
    Serial.println(" ====");
}
