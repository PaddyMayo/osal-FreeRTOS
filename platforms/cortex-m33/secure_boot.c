/**
 * \file
 *
 * Purpose: Secure start-up for the Arm MPS2 AN505 (Cortex-M33). The core resets
 * into Secure state, but the FreeRTOS port this platform builds
 * (GCC_ARM_CM33_NTZ_NONSECURE) runs in Non-secure state, so this code opens the
 * memory to Non-secure access and hands over to the shared startup code
 * (../common/cortex_m_startup.c) in Non-secure state.
 *
 * It runs once, from its own vector table in the Secure alias of the code RAM,
 * on its own stack. The link.ld of this platform places both.
 */

#include <stdint.h>

/* The Non-secure vector table, from the shared startup code */
extern void (*const OSAL_VectorTable[])(void);

/* Top of the Secure stack, defined by link.ld */
extern uint32_t _secure_estack;

#define REG32(address) (*(volatile uint32_t *)(address))

#define SCB_AIRCR   REG32(0xE000ED0CUL)
#define SCB_NSACR   REG32(0xE000ED8CUL)
#define SAU_CTRL    REG32(0xE000EDD0UL)
#define SCB_VTOR_NS REG32(0xE002ED08UL)

#define AIRCR_VECTKEY   (0x05FAUL << 16)
#define AIRCR_BFHFNMINS (1UL << 13) /* BusFault, HardFault and NMI target Non-secure state */
#define NSACR_CP10_CP11 (3UL << 10) /* Non-secure access to the FPU */
#define SAU_CTRL_ALLNS  (1UL << 1)  /* with the SAU off, only the IDAU decides what is Secure */

/*
 * The memory protection controllers in front of the three RAMs. Each divides
 * its RAM into blocks, and a bit set in the lookup table makes a block
 * accessible to Non-secure code and closes it to Secure code. Every block is
 * opened except the ones the stub itself uses: the first SECURE_RESERVE_BYTES
 * of the code RAM (its vector table and code) and the last of the data RAM (its
 * stack), as link.ld lays them out.
 */
#define MPC_CTRL    0x00UL
#define MPC_BLK_MAX 0x10UL
#define MPC_BLK_CFG 0x14UL
#define MPC_BLK_IDX 0x18UL
#define MPC_BLK_LUT 0x1CUL

#define MPC_CTRL_AUTOINC (1UL << 8)

#define SECURE_RESERVE_BYTES 4096UL

typedef struct
{
    uintptr_t Base;
    uint32_t  KeepFirstSecure; /* keep the start of the RAM Secure */
    uint32_t  KeepLastSecure;  /* keep the end of the RAM Secure */
} Secure_Mpc_t;

__attribute__((section(".secure_rodata"))) static const Secure_Mpc_t Mpc[] = {
    { 0x58007000UL, 1U, 0U }, /* code RAM */
    { 0x58008000UL, 0U, 1U }, /* data RAM */
    { 0x58009000UL, 0U, 0U }, /* second data RAM */
};

/* Everything the stub runs is in .secure_text: Secure state cannot fetch from Non-secure memory */
__attribute__((section(".secure_text"))) static void Secure_AllowNonSecureAccess(const Secure_Mpc_t *Controller)
{
    uint32_t BlockBytes = 1UL << (REG32(Controller->Base + MPC_BLK_CFG) + 5UL);
    uint32_t Words      = REG32(Controller->Base + MPC_BLK_MAX) + 1U;
    uint32_t Blocks     = Words * 32U;
    uint32_t Reserved   = (SECURE_RESERVE_BYTES + BlockBytes - 1U) / BlockBytes;
    uint32_t Word;
    uint32_t Bit;

    REG32(Controller->Base + MPC_BLK_IDX)  = 0U;
    REG32(Controller->Base + MPC_CTRL)    |= MPC_CTRL_AUTOINC;

    for (Word = 0U; Word < Words; Word++)
    {
        uint32_t Lookup = 0U;

        for (Bit = 0U; Bit < 32U; Bit++)
        {
            uint32_t Block = (Word * 32U) + Bit;

            if (!(((Controller->KeepFirstSecure != 0U) && (Block < Reserved))
                  || ((Controller->KeepLastSecure != 0U) && (Block >= (Blocks - Reserved)))))
            {
                Lookup |= (1UL << Bit);
            }
        }

        REG32(Controller->Base + MPC_BLK_LUT) = Lookup;
    }
}

__attribute__((section(".secure_text"))) void Secure_Reset_Handler(void)
{
    uint32_t NonSecureStack = (uint32_t)OSAL_VectorTable[0];
    /* BXNS takes the Non-secure state from bit 0 of the address, which must be clear */
    uint32_t NonSecureReset = (uint32_t)OSAL_VectorTable[1] & ~1UL;
    uint32_t Index;

    /* The vector table is read first: it is in memory this stub is about to lose access to */
    for (Index = 0U; Index < (sizeof(Mpc) / sizeof(Mpc[0])); Index++)
    {
        Secure_AllowNonSecureAccess(&Mpc[Index]);
    }

    SAU_CTRL  = SAU_CTRL_ALLNS;
    SCB_NSACR = NSACR_CP10_CP11;

    /* Faults and NMI go to the Non-secure handlers, where the firmware's own are */
    SCB_AIRCR = (SCB_AIRCR & 0xFFFFUL) | AIRCR_VECTKEY | AIRCR_BFHFNMINS;

    SCB_VTOR_NS = (uint32_t)OSAL_VectorTable;
    __asm volatile("msr msp_ns, %0" ::"r"(NonSecureStack));
    __asm volatile("bxns %0" ::"r"(NonSecureReset));

    for (;;)
    {
    }
}

__attribute__((section(".secure_text"))) static void Secure_Spin_Handler(void)
{
    for (;;)
    {
    }
}

// The table is read by the core, never by C, and the linker script keeps its section
// cppcheck-suppress unusedStructMember
__attribute__((section(".secure_isr_vector"), used)) void (*const OSAL_SecureVectorTable[])(void) = {
    (void (*)(void)) & _secure_estack, /* initial Secure stack pointer */
    Secure_Reset_Handler,              /* reset */
    Secure_Spin_Handler,               /* NMI */
    Secure_Spin_Handler,               /* HardFault */
};
