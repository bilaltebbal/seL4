/*
 * Copyright 2017, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DATA61_GPL)
 */

#ifndef __MODE_SMP_H_
#define __MODE_SMP_H_

#include <config.h>

#ifdef ENABLE_SMP_SUPPORT

#if 1

static inline CONST cpu_id_t
getCurrentCPUIndex(void)
{
    cpu_id_t cpu_id;
    asm volatile ("mrs %[cpu_id], mpidr_el1" : [cpu_id] "=r" (cpu_id) ::);
    return cpu_id&0x0f;
}

#else
extern char kernel_stack_alloc[CONFIG_MAX_NUM_NODES][BIT(CONFIG_KERNEL_STACK_BITS)];

/* Get current stack pointer */
static inline void*
getCurESP(void)
{
    uint64_t stack;
    void *result;
    //asm ("mov %[stack_address], %[result]" : [result] "=r"(result) : [stack_address] "r"(&stack));
    //return result;
    result = (void *)&stack;
    return result;
}

static inline CONST cpu_id_t
getCurrentCPUIndex(void)
{
    cpu_id_t cpu_id;
    uint64_t esp = (uint64_t)getCurESP();

    esp -= (uint64_t)kernel_stack_alloc;
    cpu_id = esp >> CONFIG_KERNEL_STACK_BITS;
    return cpu_id&0xff;
}
#endif

#endif /* ENABLE_SMP_SUPPORT */
#endif /* __MODE_SMP_H_ */
