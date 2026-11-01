/**
 * \file
 *
 * Purpose: The vector table of the shared Cortex-M startup code
 * (cortex_m_startup.c), which a platform's own startup code reads.
 */

#ifndef CORTEX_M_STARTUP_H
#define CORTEX_M_STARTUP_H

/* 16 entries: the initial stack pointer, then the 15 system exception vectors */
extern void (*const OSAL_VectorTable[16])(void);

#endif
