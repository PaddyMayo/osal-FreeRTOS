"""Run a firmware under QEMU through gdb and report how it ended.

The firmware is neither modified nor instrumented. QEMU starts it halted and
serves a gdb stub on a pipe; breakpoints on functions the BSP already has say
how it ended:

    OS_FreeRTOS_ApplicationExit   the application finished, r0 is its status
    OS_FreeRTOS_AssertFailed      configASSERT failed, r0 and r1 are file and line
    OS_FreeRTOS_StackOverflow     a task overflowed its stack, r0 is its name
    HardFault_Handler, NMI_Handler  the core faulted

A failure prints where it happened. A run that reaches none of them within the
timeout is stopped and reported with where it was.

Run as:  gdb-multiarch -q -batch -x run_firmware_test.py --args <firmware.elf>
Environment:
    QEMU_SYSTEM        the qemu-system binary
    QEMU_MACHINE       the machine to run on
    FIRMWARE_TIMEOUT   seconds before the run is stopped, default 30
"""

import os
import sys
import threading

import gdb

QEMU_SYSTEM = os.environ["QEMU_SYSTEM"]
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


def register(name):
    return int(gdb.parse_and_eval("$" + name)) & 0xFFFFFFFF


def read_word(address):
    return int.from_bytes(bytes(gdb.selected_inferior().read_memory(address, 4)), "little")


def read_string(address):
    try:
        return gdb.parse_and_eval("(const char *)%d" % address).string()
    except gdb.error:
        return "<unreadable>"


def finish(code, message):
    print(message)
    sys.stdout.flush()
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
        if status == 0:
            finish(0, "PASS: the application ended with status 0")
        finish(1, "FAIL: the application ended with status %d" % status)
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
        print("no exit within %g s, the firmware was at:" % TIMEOUT)
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

    EndPoint("OS_FreeRTOS_ApplicationExit", "exit")
    EndPoint("OS_FreeRTOS_AssertFailed", "assert")
    EndPoint("OS_FreeRTOS_StackOverflow", "overflow")
    EndPoint("HardFault_Handler", "hardfault")
    EndPoint("NMI_Handler", "nmi")

    timer = threading.Timer(TIMEOUT, gdb.interrupt)
    timer.start()
    gdb.execute("continue")
    timer.cancel()

    report()


main()
