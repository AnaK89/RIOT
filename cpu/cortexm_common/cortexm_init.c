/*
 * Copyright (C) 2015 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     cpu_cortexm_common
 * @{
 *
 * @file
 * @brief       Cortex-M specific configuration and initialization options
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 *
 * @}
 */

#include "cpu.h"
#include "lpm.h"

/**
 * @name   Pattern to write into the co-processor Access Control Register to
 *         allow full FPU access
 */
#define FULL_FPU_ACCESS         (0x00f00000)

void cortexm_init(void)
{
    /* initialize the FPU on Cortex-M4F CPUs */
#ifdef CPU_ARCH_CORTEX_M4F
    /* give full access to the FPU */
    SCB->CPACR |= (uint32_t)FULL_FPU_ACCESS;
#endif

    /* configure the vector table location to internal flash */
#if defined(CPU_ARCH_CORTEX_M3) || defined(CPU_ARCH_CORTEX_M4) || \
    defined(CPU_ARCH_CORTEX_M4F)
    SCB->VTOR = CPU_FLASH_BASE;
#endif

    /* initialize the interrupt priorities */
    /* set pendSV interrupt to same priority as the rest */
    NVIC_SetPriority(PendSV_IRQn, CPU_DEFAULT_IRQ_PRIO);
    /* set SVC interrupt to same priority as the rest */
    NVIC_SetPriority(SVCall_IRQn, CPU_DEFAULT_IRQ_PRIO);
    /* initialize all vendor specific interrupts with the same value */
    for (int i = 0; i < CPU_IRQ_NUMOF; i++) {
        NVIC_SetPriority((IRQn_Type) i, CPU_DEFAULT_IRQ_PRIO);
    }

    /* enable wake up on events for __WFE CPU sleep */
    SCB->SCR |= SCB_SCR_SEVONPEND_Msk;

    /* for Cortex-M3 r1p0 and up the STKALIGN option was added, but not automatically
     * enabled until revision r2p0. For 64bit function arguments to work properly this
     * needs to be enabled.
     */
#ifdef SCB_CCR_STKALIGN_Msk
    SCB->CCR |= SCB_CCR_STKALIGN_Msk;
#endif
}

bool cpu_check_address(volatile const char *address)
{
#if defined(CPU_ARCH_CORTEX_M3) || defined(CPU_ARCH_CORTEX_M4) || defined(CPU_ARCH_CORTEX_M4F)
    bool is_valid = true;

    /* Clear BFAR ADDRESS VALID flag */
    SCB->CFSR |= SCB_CFSR_BFARVALID;

    SCB->CCR |= SCB_CCR_BFHFNMIGN;
    __asm volatile ("cpsid f;");
    
    *address;
    if ((SCB->CFSR & SCB_CFSR_BFARVALID) != 0)
    {
        /* Bus Fault occured reading the address */
        is_valid = false;
    }
    
    __asm volatile ("cpsie f;");
    SCB->CCR &= ~SCB_CCR_BFHFNMIGN;

    return is_valid;
#else
    /* Cortex-M0 doesn't have BusFault */
    puts("Cortex-M0 doesn't have BusFault");
    assert(false);
    return true;
#endif
}

#include "xtimer.h"
static xtimer_t timer_yield;
static volatile uint32_t sleep_status;

static void thread_yield_later(void *arg) {
    (void)arg;
    lpm_prevent_sleep = sleep_status;
    thread_yield();
}

void cortexm_isr_end(void)
{
    if (sched_context_switch_request == 1) {
        thread_yield();
    } else {
    /* dirty workaround for a strange bug (?) in STM32L1 with RTC IRQs */
        if (sched_context_switch_request == 2) {
            sleep_status = lpm_prevent_sleep;
            lpm_prevent_sleep = 1;
            timer_yield.callback = thread_yield_later;
            /* 1 RTCCLK = 30.5 us seems to be the right delay */
            xtimer_set(&timer_yield, 32);
        }
    }
}