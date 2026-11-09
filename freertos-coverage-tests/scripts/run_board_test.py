"""Run a firmware image on its QEMU board through gdb and report how it ended.

A Cortex-M board has no operating system to return a status to, and its QEMU
process keeps running after the image halts. So the image is neither modified nor
instrumented: QEMU starts it halted and serves a gdb stub on a pipe, and
breakpoints on functions the image already has say how it ended:

    CoverageTestExit              a coverage test runner finished, r0 is its status
    OS_FreeRTOS_ApplicationExit   the BSP's application finished, r0 is its status
    OS_FreeRTOS_AssertFailed      configASSERT failed, r0 and r1 are file and line
    OS_FreeRTOS_StackOverflow     a task overflowed its stack, r0 is its name
    HardFault_Handler, NMI_Handler  the core faulted

Only the breakpoints for functions that are in the image are set. A status of 0 is
a pass. A failure prints where it happened. A run that reaches none of them within
the timeout is stopped and reported with where it was.

Run as:  gdb-multiarch -q -batch -x freertos-coverage-tests/scripts/run_board_test.py --args <firmware.elf>
Environment:
    QEMU_MACHINE       the machine to run on, for example mps2-an385
    QEMU_SYSTEM        the qemu-system binary, default qemu-system-arm
    FIRMWARE_TIMEOUT   seconds before the run is stopped, default 30
"""

import os
import threading

import gdb

QEMU_SYSTEM = os.environ.get("QEMU_SYSTEM", "qemu-system-arm")
QEMU_MACHINE = os.environ["QEMU_MACHINE"]
TIMEOUT = float(os.environ.get("FIRMWARE_TIMEOUT", "30"))

# What the run ended with, set by the breakpoint that stopped it
outcome = None


class EndPoint(gdb.Breakpoint):
    """A breakpoint that records which way the firmware ended."""

    def __init__(self, function, name):
        super().__init__(function, type=gdb.BP_HARDWARE_BREAKPOINT)
        self.outcome_name = name

    def stop(self):
        global outcome
        outcome = self.outcome_name
        return True


def has_symbol(name):
    """Whether the image has a function or variable of this name."""
    try:
        gdb.execute("info address %s" % name, to_string=True)
        return True
    except gdb.error:
        return False


def register(name):
    return int(gdb.parse_and_eval("$" + name)) & 0xFFFFFFFF


def read_word(address):
    return int.from_bytes(bytes(gdb.selected_inferior().read_memory(address, 4)), "little")


def read_string(address):
    try:
        return gdb.parse_and_eval("(const char *)%d" % address).string()
    except gdb.error:
        return "<unreadable>"


def symbol_address(name):
    return int(gdb.parse_and_eval("(unsigned long)&%s" % name)) & 0xFFFFFFFF


def write_coverage():
    """Write the gcda files the image left in its coverage buffer, if it has one.

    The image cannot write files, so coverage-dump.c collects them in RAM, as records
    of a file name and a 0 byte, a four byte length (least significant first) and the
    data. The name is the path GCC gave the file in the build tree. Returns False when
    the data is incomplete.
    """
    if not has_symbol("CoverageBuffer"):
        return True

    lost = read_word(symbol_address("CoverageBufferLost"))
    if lost:
        print("the coverage buffer was %d bytes too small" % lost)
        return False

    used = read_word(symbol_address("CoverageBufferUsed"))
    data = bytes(gdb.selected_inferior().read_memory(symbol_address("CoverageBuffer"), used))
    position = 0
    while position < len(data):
        end = data.index(b"\0", position)
        name = data[position:end].decode()
        length = int.from_bytes(data[end + 1 : end + 5], "little")
        start = end + 5
        if name:
            with open(name, "wb") as gcda:
                gcda.write(data[start : start + length])
        position = start + length
    return True


def finish(code, message):
    print(message)
    # gdb leaves QEMU running when it exits, and QEMU holds the output pipe open
    try:
        gdb.execute("monitor quit")
    except gdb.error:
        pass
    gdb.execute("quit %d" % code)


def show_location(address):
    for command in ("info symbol %#x" % address, "info line *%#x" % address):
        try:
            gdb.execute(command)
        except gdb.error:
            pass


def show_backtrace():
    try:
        gdb.execute("backtrace")
    except gdb.error:
        pass


def describe_fault(name):
    """Print the frame the core stacked when it took the fault."""
    link = register("lr")
    try:
        stack = register("psp") if link & 4 else register("msp")
    except gdb.error:
        stack = register("sp")
    names = ("r0", "r1", "r2", "r3", "r12", "lr", "pc", "xpsr")
    frame = {n: read_word(stack + 4 * i) for i, n in enumerate(names)}
    print("%s, stacked frame:" % name)
    for n in names:
        print("  %-4s %#010x" % (n, frame[n]))
    try:
        print("  cfsr %#010x  hfsr %#010x" % (read_word(0xE000ED28), read_word(0xE000ED2C)))
    except gdb.error:
        pass
    show_location(frame["pc"])


def report():
    if outcome == "exit":
        status = register("r0")
        if not write_coverage():
            finish(1, "FAIL: the coverage data could not be read")
        if status == 0:
            finish(0, "PASS: the image ended with status 0")
        finish(1, "FAIL: the image ended with status %d" % status)
    elif outcome == "assert":
        print("configASSERT failed at %s:%d" % (read_string(register("r0")), register("r1")))
        show_backtrace()
        finish(1, "FAIL: assertion")
    elif outcome == "overflow":
        print("stack overflow in task %s" % read_string(register("r0")))
        show_backtrace()
        finish(1, "FAIL: stack overflow")
    elif outcome in ("hardfault", "nmi"):
        describe_fault(outcome)
        finish(1, "FAIL: %s" % outcome)
    else:
        print("no exit within %g s, the image was at:" % TIMEOUT)
        show_location(register("pc"))
        show_backtrace()
        finish(1, "FAIL: timeout")


def main():
    firmware = gdb.objfiles()[0].filename

    gdb.execute("set pagination off")
    gdb.execute("set confirm off")
    gdb.execute(
        "target remote | %s -M %s -display none -serial none -monitor none -S -gdb stdio -kernel %s"
        % (QEMU_SYSTEM, QEMU_MACHINE, firmware)
    )

    endpoints = (
        ("CoverageTestExit", "exit"),
        ("OS_FreeRTOS_ApplicationExit", "exit"),
        ("OS_FreeRTOS_AssertFailed", "assert"),
        ("OS_FreeRTOS_StackOverflow", "overflow"),
        ("HardFault_Handler", "hardfault"),
        ("NMI_Handler", "nmi"),
    )
    if not any(has_symbol(function) for function, name in endpoints if name == "exit"):
        finish(1, "FAIL: the image has no exit point to stop at")
    for function, name in endpoints:
        if has_symbol(function):
            EndPoint(function, name)

    timer = threading.Timer(TIMEOUT, gdb.interrupt)
    timer.start()
    gdb.execute("continue")
    timer.cancel()

    report()


main()
