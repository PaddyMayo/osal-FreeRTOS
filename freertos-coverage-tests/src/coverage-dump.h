/*
 * File: coverage-dump.h
 *
 * Purpose:
 *   Writes out the gcov counters of the test executable, see coverage-dump.c.
 */

#ifndef COVERAGE_DUMP_H
#define COVERAGE_DUMP_H

/* Call once, after the tests ran and before the program ends */
void CoverageDump(void);

#endif /* COVERAGE_DUMP_H */
