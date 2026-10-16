"""Check that a halted POSIX firmware sleeps instead of spinning.

The POSIX counterpart of run_halt_test.py: the firmware is a Linux program run
under qemu-user, so there is no gdb and no core to look at, only the process.
It has no console either, so a halt is told from a run that is still going by
the process being alive long after startup (a firmware that does not halt ends
the scheduler and exits), and a sleeping halt from a spinning one by how much
CPU the process uses.

The busy expectation is the control: a firmware that never halts and spins must
read as using CPU, or the measurement would pass whatever the halt did.

Run as:  python3 run_halt_test_posix.py <idle|busy> <qemu-user> <sysroot> <firmware>
"""

import os
import subprocess
import sys
import time

SETTLE = 1.5  # seconds for the firmware to start and halt
MEASURE = 2.0  # seconds to watch it

# Of one host CPU, what a halted process may use and what a spinning one must
IDLE_LIMIT = 0.20
BUSY_FLOOR = 0.50


def cpu_seconds(pid):
    """CPU used by the process and every thread in it, in seconds."""
    with open("/proc/%d/stat" % pid) as f:
        fields = f.read().rsplit(")", 1)[1].split()
    return (int(fields[11]) + int(fields[12])) / os.sysconf("SC_CLK_TCK")


def main():
    expect, qemu, sysroot, firmware = sys.argv[1:5]

    process = subprocess.Popen([qemu, "-L", sysroot, firmware], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        time.sleep(SETTLE)
        if process.poll() is not None:
            print("FAIL: the firmware ended with status %d, it did not halt" % process.returncode)
            return 1

        cpu_before = cpu_seconds(process.pid)
        wall_before = time.monotonic()
        time.sleep(MEASURE)
        cpu_used = cpu_seconds(process.pid) - cpu_before
        wall = time.monotonic() - wall_before
        share = cpu_used / wall
    finally:
        process.kill()
        process.wait()

    print("process used %.2f s of CPU in %.2f s of wall time (%.0f%%)" % (cpu_used, wall, share * 100))

    if expect == "busy":
        if share < BUSY_FLOOR:
            print("FAIL: the control did not read as busy, so this measurement proves nothing")
            return 1
        print("PASS: the control reads as busy")
        return 0

    if share > IDLE_LIMIT:
        print("FAIL: the halted process used %.0f%% of a host CPU, it is spinning" % (share * 100))
        return 1
    print("PASS: halted and asleep")
    return 0


sys.exit(main())
