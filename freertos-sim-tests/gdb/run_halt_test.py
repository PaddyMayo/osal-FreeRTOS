"""Check that a halted firmware sleeps instead of spinning, on a QEMU board.

The firmware is neither modified nor instrumented, as in run_firmware_test.py.
Once it reaches one of the BSP's halt functions this lets it run on and looks
from outside at what a halted core must be doing:

    the emulator uses (almost) no CPU     WFI sleeps the vCPU, a spin loop does not
    SysTick is off and the tick is frozen the halt masked the interrupts that
                                          would wake WFI at once

EXPECT=busy is the control: a firmware that never halts and spins must read as
using CPU, or the measurement above would pass whatever the halt did.

Run as:  gdb-multiarch -q -batch -x run_halt_test.py --args <firmware.elf>
Environment:
    QEMU_SYSTEM        the qemu-system binary
    QEMU_MACHINE       the machine to run on
    EXPECT             idle (default) or busy
    FIRMWARE_TIMEOUT   seconds to wait for the halt, default 30
    MEASURE_SECONDS    how long to watch the halted firmware, default 2
"""

import os
import sys
import threading
import time

import gdb

QEMU_SYSTEM = os.environ["QEMU_SYSTEM"]
QEMU_MACHINE = os.environ["QEMU_MACHINE"]
EXPECT = os.environ.get("EXPECT", "idle")
TIMEOUT = float(os.environ.get("FIRMWARE_TIMEOUT", "30"))
MEASURE = float(os.environ.get("MEASURE_SECONDS", "2"))

# Of one host CPU, what a sleeping emulator may use and what a spinning one must
IDLE_LIMIT = 0.20
BUSY_FLOOR = 0.50

SYST_CSR = 0xE000E010
SYST_ENABLE = 1

HALT_FUNCTIONS = {
    "OS_FreeRTOS_ApplicationExit": "exit",
    "OS_FreeRTOS_AssertFailed": "assert",
    "OS_FreeRTOS_StackOverflow": "overflow",
}

# Which halt function ended the run, set by its breakpoint
halted_in = None


class HaltPoint(gdb.Breakpoint):
    def __init__(self, function, name):
        super().__init__(function, type=gdb.BP_HARDWARE_BREAKPOINT)
        self.halt_name = name

    def stop(self):
        global halted_in
        halted_in = self.halt_name
        return True


def finish(code, message):
    print(message)
    sys.stdout.flush()
    try:
        gdb.execute("monitor quit")
    except gdb.error:
        pass
    gdb.execute("quit %d" % code)


def qemu_pid():
    """The emulator gdb started, found among gdb's descendants."""
    parents = {}
    for entry in os.listdir("/proc"):
        if entry.isdigit():
            try:
                with open("/proc/%s/stat" % entry) as f:
                    parents[int(entry)] = int(f.read().rsplit(")", 1)[1].split()[1])
            except (OSError, IndexError, ValueError):
                pass
    mine = {os.getpid()}
    grew = True
    while grew:
        grew = False
        for pid, ppid in parents.items():
            if ppid in mine and pid not in mine:
                mine.add(pid)
                grew = True
    for pid in mine:
        try:
            with open("/proc/%d/cmdline" % pid, "rb") as f:
                if os.path.basename(f.read().split(b"\0")[0].decode()) == os.path.basename(QEMU_SYSTEM):
                    return pid
        except OSError:
            pass
    return None


def cpu_seconds(pid):
    with open("/proc/%d/stat" % pid) as f:
        fields = f.read().rsplit(")", 1)[1].split()
    # utime and stime, in clock ticks, are the 12th and 13th after the name
    return (int(fields[11]) + int(fields[12])) / os.sysconf("SC_CLK_TCK")


def read_word(address):
    return int.from_bytes(bytes(gdb.selected_inferior().read_memory(address, 4)), "little")


def tick_count():
    # xTickCount is static in the kernel and the build has no debug info
    return int(gdb.parse_and_eval("*(unsigned int *)&xTickCount")) & 0xFFFFFFFF


def run_for(seconds):
    """Let the firmware run for a while, then stop it."""
    timer = threading.Timer(seconds, gdb.interrupt)
    timer.start()
    gdb.execute("continue")
    timer.cancel()


def main():
    firmware = gdb.objfiles()[0].filename

    gdb.execute("set pagination off")
    gdb.execute("set confirm off")
    gdb.execute(
        "target remote | %s -M %s -display none -serial none -monitor none -S -gdb stdio -kernel %s"
        % (QEMU_SYSTEM, QEMU_MACHINE, firmware)
    )

    pid = qemu_pid()
    if pid is None:
        finish(1, "FAIL: could not find the emulator process")

    if EXPECT == "busy":
        # Never halts: give it a moment to get going, the measurement below is the test
        run_for(1.0)
    else:
        for function, name in HALT_FUNCTIONS.items():
            HaltPoint(function, name)
        run_for(TIMEOUT)
        if halted_in is None:
            finish(1, "FAIL: the firmware did not halt within %g s" % TIMEOUT)
        print("halted in the %s path" % halted_in)

    # The halt itself is not run to completion: release the breakpoints, then watch it
    gdb.execute("delete breakpoints")

    tick_before = tick_count()
    cpu_before = cpu_seconds(pid)
    wall_before = time.monotonic()
    run_for(MEASURE)
    cpu_used = cpu_seconds(pid) - cpu_before
    wall = time.monotonic() - wall_before
    tick_after = tick_count()
    share = cpu_used / wall

    print("emulator used %.2f s of CPU in %.2f s of wall time (%.0f%%)" % (cpu_used, wall, share * 100))
    print("tick count %d -> %d" % (tick_before, tick_after))

    if EXPECT == "busy":
        if share < BUSY_FLOOR:
            finish(1, "FAIL: the control did not read as busy, so this measurement proves nothing")
        finish(0, "PASS: the control reads as busy")

    systick = read_word(SYST_CSR)
    print("SYST_CSR %#010x" % systick)

    if systick & SYST_ENABLE:
        finish(1, "FAIL: SysTick still runs after the halt")
    if tick_after != tick_before:
        finish(1, "FAIL: the tick advanced after the halt")
    if share > IDLE_LIMIT:
        finish(1, "FAIL: the halted core used %.0f%% of a host CPU, it is spinning" % (share * 100))
    finish(0, "PASS: halted, SysTick stopped and the core is asleep")


main()
