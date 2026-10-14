/**
 * \file
 *
 * Purpose: Startup code slot for the GCC_ARM_CM0 build.
 *
 * Placeholder: the osal target is a static library, so nothing links yet. A
 * board port replaces this with its vector table and reset handler, pointing
 * NMI and HardFault at a spin loop (see OS_BSP_MaskAllInterrupts in bsp_console.c).
 */

typedef int osal_freertos_cortex_m0_startup_placeholder;
