/*
 * File: firmware.h
 *
 * Purpose:
 *   What a test runner needs when it is built as Cortex-M firmware, see
 *   firmware-handlers.c.
 */

#ifndef FIRMWARE_H
#define FIRMWARE_H

/*
 * The end of a test run: Status is 0 when every test passed. A board has no
 * operating system to return it to, so run_board_test.py stops the board here and
 * reads the status from r0. Never returns.
 */
void CoverageTestExit(int Status) __attribute__((noreturn));

#endif /* FIRMWARE_H */
