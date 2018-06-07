/*
 * Copyright 2014, General Dynamics C4 Systems
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(GD_GPL)
 */

#include <config.h>

#ifdef CONFIG_DEBUG_BUILD
#ifndef __API_DEBUG_H
#define __API_DEBUG_H

#include <benchmark/benchmark_track.h>
#include <arch/api/syscall.h>
#include <arch/kernel/vspace.h>
#include <model/statedata.h>
#include <kernel/thread.h>

#include <arch/object/structures_gen.h>
#include <types.h>
#include <benchmark/benchmark.h>
#include <api/failures.h>
#include <kernel/boot.h>
#include <kernel/cspace.h>
#include <kernel/stack.h>
#include <machine/io.h>
#include <machine/debug.h>
#include <object/cnode.h>
#include <object/untyped.h>
#include <arch/api/invocation.h>
#include <linker.h>
#include <plat/machine/devices.h>
#include <plat/machine/hardware.h>
#include <armv/context_switch.h>
#include <arch/object/iospace.h>
#include <arch/object/vcpu.h>
#include <arch/machine/tlb.h>

#include <mode/model/statedata.h>

#ifdef CONFIG_PRINTING

static inline void
debug_printKernelEntryReason(void)
{
    printf("\nKernel entry via ");
    switch (ksKernelEntry.path) {
    case Entry_Interrupt:
        printf("Interrupt, irq %lu\n", (unsigned long) ksKernelEntry.word);
        break;
    case Entry_UnknownSyscall:
        printf("Unknown syscall, word: %lu", (unsigned long) ksKernelEntry.word);
        break;
    case Entry_VMFault:
        printf("VM Fault, fault type: %lu\n", (unsigned long) ksKernelEntry.word);
        break;
    case Entry_UserLevelFault:
        printf("User level fault, number: %lu", (unsigned long) ksKernelEntry.word);
        break;
#ifdef CONFIG_HARDWARE_DEBUG_API
    case Entry_DebugFault:
        printf("Debug fault. Fault Vaddr: 0x%lx", (unsigned long) ksKernelEntry.word);
        break;
#endif
    case Entry_Syscall:
        printf("Syscall, number: %ld, %s\n", (long) ksKernelEntry.syscall_no, syscall_names[ksKernelEntry.syscall_no]);
        if (ksKernelEntry.syscall_no == -SysSend ||
                ksKernelEntry.syscall_no == -SysNBSend ||
                ksKernelEntry.syscall_no == -SysCall) {

            printf("Cap type: %lu, Invocation tag: %lu\n", (unsigned long) ksKernelEntry.cap_type,
                   (unsigned long) ksKernelEntry.invocation_tag);
        }
        break;
#ifdef CONFIG_ARCH_ARM
    case Entry_VCPUFault:
        printf("VCPUFault\n");
        break;
#endif
#ifdef CONFIG_ARCH_x86
    case Entry_VMExit:
        printf("VMExit\n");
        break;
#endif
    default:
        printf("Unknown\n");
        break;

    }
}

/* Prints the user context and stack trace of the current thread */
static inline void
debug_printUserState(void)
{
    tcb_t *tptr = NODE_STATE(ksCurThread);
    printf("Current thread: %s\n", tptr->tcbName);
    printf("Next instruction adress: %lx\n", getRestartPC(tptr));
    printf("Stack:\n");
    Arch_userStackTrace(tptr);
}

static inline void
debug_printTCB(tcb_t *tcb)
{
    printf("%40s\t", tcb->tcbName);
    char* state;
    switch (thread_state_get_tsType(tcb->tcbState)) {
    case ThreadState_Inactive:
        state = "inactive";
        break;
    case ThreadState_Running:
        state = "running";
        break;
    case ThreadState_Restart:
        state = "restart";
        break;
    case ThreadState_BlockedOnReceive:
        state = "blocked on recv";
        break;
    case ThreadState_BlockedOnSend:
        state = "blocked on send";
        break;
    case ThreadState_BlockedOnReply:
        state = "blocked on reply";
        break;
    case ThreadState_BlockedOnNotification:
        state = "blocked on ntfn";
        break;
#ifdef CONFIG_VTX
    case ThreadState_RunningVM:
        state = "running VM";
        break;
#endif
    case ThreadState_IdleThreadState:
        state = "idle";
        break;
    default:
        fail("Unknown thread state");
    }

    word_t core = SMP_TERNARY(tcb->tcbAffinity, 0);
    printf("%15s\t%p\t%20lu\t%lu\n", state, (void *) getRestartPC(tcb), tcb->tcbPriority, core);
}
/*
static inline void
debug_dumpScheduler(void)
{
    printf("Dumping all tcbs!\n");
    printf("Name                                    \tState          \tIP                  \t Prio \t Core\n");
    printf("--------------------------------------------------------------------------------------\n");
    for (tcb_t *curr = NODE_STATE(ksDebugTCBs); curr != NULL; curr = curr->tcbDebugNext) {
        debug_printTCB(curr);
    }
}
*/

static inline void
cap_print_cap_type(cap_t cap)
{
    cap_tag_t ctag;

    ctag = cap_get_capType(cap);

    switch (ctag) {
    case cap_frame_cap:
        printf("cap_frame_cap\n");
        break;

    case cap_page_table_cap:
        printf("cap_page_table_cap\n");
        break;

    case cap_page_directory_cap:
        printf("cap_page_directory_cap\n");
        break;

    case cap_page_upper_directory_cap:
        printf("cap_page_upper_directory_cap\n");
        break;

    case cap_page_global_directory_cap:
        printf("cap_page_global_directory_cap\n");
        break;

    case cap_asid_pool_cap:
        printf("cap_asid_pool_cap\n");
        break;

    case cap_asid_control_cap:
        printf("cap_asid_control_cap\n");
        break;

    default:
        printf("Unknown: %d\n", ctag);
        break;
    }
}

static inline vspace_root_t*
TCB_VROOT(tcb_t *tcb)
{
    vspace_root_t *vspace = NULL;
    cap_t threadRoot;

#ifdef CONFIG_ARCH_AARCH64
    threadRoot = TCB_PTR_CTE_PTR(tcb, tcbVTable)->cap;

    if (isValidNativeRoot(threadRoot)) {
        vspace = PGD_PTR(cap_page_global_directory_cap_get_capPGDBasePtr(threadRoot));
    }
#endif /* CONFIG_ARCH_AARCH64 */
    
    return vspace;
}

#ifdef CONFIG_ARCH_ARM
/* Nested defines are always a good time, but more readable than one really big bit shift */
#define MASK_SHIFT(bits, shift) (MASK(bits) << shift)
#define SET_TABLE_INDEX(vaddr, index, index_bits, index_offset) \
    ( ((vaddr) & ~ MASK_SHIFT(index_bits, index_offset)) \
    + ((index) &   MASK_SHIFT(index_bits, index_offset)) )
#define SET_PGD_INDEX(vaddr, index) (SET_TABLE_INDEX(vaddr, index, PGD_INDEX_BITS, PGD_INDEX_OFFSET))
#define SET_PUD_INDEX(vaddr, index) (SET_TABLE_INDEX(vaddr, index, PUD_INDEX_BITS, PUD_INDEX_OFFSET))
#define SET_PD_INDEX(vaddr, index) (SET_TABLE_INDEX(vaddr, index, PD_INDEX_OFFSET, PD_INDEX_BITS))
#define SET_PT_INDEX(vaddr, index) (SET_TABLE_INDEX(vaddr, index, PT_INDEX_OFFSET, PT_INDEX_BITS))

static inline void
debug_printPT(pte_t *pt)
{

}

static inline void
debug_printPD(pde_t *pd)
{
    pde_t *slot;
    UNUSED pte_t *current_pte;
    for(slot=pd; slot <= pd + (word_t) MASK(PD_INDEX_BITS); slot += (word_t) 1)
    {
        if(pde_pde_small_ptr_get_present(slot))
        {
            printf("\t\t\tPD slot found: 0x%lx\n", pptr_to_paddr(slot));
            current_pte = paddr_to_pptr(pde_pde_small_ptr_get_pt_base_address(slot));
            /* Now we recurse */
        }
    }
}

#ifdef CONFIG_ARCH_AARCH64

static inline void
debug_printPUD(pude_t *pude)
{
    pude_t *slot;
    UNUSED pde_t *current_pde;
    for(slot=pude; slot <= pude + (word_t) MASK(PUD_INDEX_BITS); slot += (word_t) 1)
    {
        if(pude_pude_pd_ptr_get_present(slot))
        {
            printf("\t\tPUD slot found: 0x%lx\n", pptr_to_paddr(slot));
            current_pde = paddr_to_pptr(pude_pude_pd_ptr_get_pd_base_address(slot));
            debug_printPD(current_pde);
        }
    }
}

static inline void
debug_printPGD(pgde_t *pgd)
{
    pgde_t *slot;
    UNUSED pude_t *current_pude;
    for(slot=pgd; slot <= pgd + (word_t) MASK(PGD_INDEX_BITS); slot += (word_t) 1)
    {
        if(pgde_ptr_get_present(slot))
        {
            printf("\tPGD slot found: 0x%lx\n", pptr_to_paddr(slot));
            current_pude = paddr_to_pptr(pgde_ptr_get_pud_base_address(slot));
            debug_printPUD(current_pude);
        }
    }
}

#endif /* CONFIG_ARCH_AARCH64 */
#endif /* CONFIG_ARCH_ARM */

static inline void
debug_print_from_vroot(vspace_root_t *vspace)
{
#ifdef CONFIG_ARCH_AARCH64
    debug_printPGD(vspace);
#elif CONFIG_ARCH_AARCH32
    debug_printPD(vspace);
#endif
}

static inline void
debug_printAddr(tcb_t *tcb)
{
    vspace_root_t *vspace = NULL;
    paddr_t addr;

    vspace = TCB_VROOT(tcb);
    if(vspace == NULL)
    {
        printf("%15s\t%20s\n", tcb->tcbName, "Unknown VSpace Root");
        return;
    }
    addr = pptr_to_paddr(vspace);
    printf("%15s - Base Address: 0x%15lx \n", tcb->tcbName, addr);
    debug_print_from_vroot(vspace);
}


static inline void
debug_dumpScheduler(void)
{
    printf("Dumping all tcb addresses!\n");
    // printf("Thread                                    \tAddress          \n");
    // printf("--------------------------------------------------------------------------------------\n");
    for (tcb_t *curr = NODE_STATE(ksDebugTCBs); curr != NULL; curr = curr->tcbDebugNext) {
        debug_printAddr(curr);
    }
}
#endif /* CONFIG_PRINTING */
#endif /* __API_DEBUG_H */
#endif /* CONFIG_DEBUG_BUILD */
