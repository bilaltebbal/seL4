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
#include <model/statedata.h>
#include <kernel/thread.h>
#include <kernel/vspace.h>

#include <mode/model/statedata.h>
#include <mode/machine/hardware.h>
#include <api/sglib.h>

#ifdef CONFIG_PRINTING
typedef enum {
    PERMS_NONE = 0x0,
    PERMS_X = 0x1,
    PERMS_W = 0x2,
    PERMS_WX = 0x3,
    PERMS_R = 0x4,
    PERMS_RX = 0x5,
    PERMS_RW = 0x6,
    PERMS_RWX = 0x7,
    PERMS_PageTable = 0x8, 
    /* Used to separate page tables from pages */
    SEL4_FORCE_LONG_ENUM(seL4_Page_Permissions),
} page_permissions_t;

typedef struct proc_map proc_map_t;

struct proc_map {
    paddr_t start;
    paddr_t end;
    page_permissions_t permissions;
    tcb_t *thread;
    bool_t entry_valid;
    proc_map_t *next;
    proc_map_t *prev;
};

#define NUM_REGIONS (1 << 18)
#define PAGE_TABLE_PERMS 0x8

proc_map_t maps[NUM_REGIONS];
paddr_t* regions[2* NUM_REGIONS];

static inline int get_num_cores(void) {
    #ifdef ENABLE_SMP_SUPPORT
        return CONFIG_MAX_NUM_NODES;
    #else
        return 1;
    #endif
}

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
#endif /* CONFIG_ARCH_ARM */
#ifdef CONFIG_ARCH_x86
    case Entry_VMExit:
        printf("VMExit\n");
        break;
#endif /* CONFIG_ARCH_x86 */
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
#endif /* CONFIG_VTX */
    case ThreadState_IdleThreadState:
        state = "idle";
        break;
    default:
        fail("Unknown thread state");
    }

    word_t core = SMP_TERNARY(tcb->tcbAffinity, 0);
    printf("%15s\t%p\t%20lu\t%lu\n", state, (void *) getRestartPC(tcb), tcb->tcbPriority, core);
}

static inline void
debug_dumpScheduler(void)
{
    int core;
    printf("Dumping all tcbs!\n");
    printf("Name                                    \tState          \tIP                  \t Prio \t Core\n");
    printf("--------------------------------------------------------------------------------------\n");
    for (core = 0; core < get_num_cores(); core++)
    {
        for (tcb_t *curr = NODE_STATE_ON_CORE(ksDebugTCBs, core); curr != NULL; curr = curr->tcbDebugNext) {
            debug_printTCB(curr);
        }
    }
}

/*
 * Helper functions for debug_procMap
 */

static inline exception_t
insert_proc_map(proc_map_t *map, word_t *current_index, word_t map_size, 
                paddr_t addr, word_t size, page_permissions_t permissions, tcb_t *thread) {
    exception_t status = EXCEPTION_NONE;
    if (*current_index >= map_size) {
        return EXCEPTION_FAULT;
    }

    proc_map_t current_region;

    current_region.start = addr;
    current_region.end = addr + size - 1;
    current_region.permissions = permissions;
    current_region.thread = thread;
    current_region.entry_valid = true;

    map[*current_index] = current_region;
    *current_index += 1;

    if (current_region.thread == NULL) {
        return EXCEPTION_SYSCALL_ERROR;
    }
    return status;
}

/*
 * The next two functions use some bit manipulation to help manage which proc map a given paddr
 *  comes from. This prevents the need to use a separate data structure, as bi-directional pointers
 *  increase memory overhead
 */

static inline exception_t
region_get_proc_map(paddr_t *region_ptr, proc_map_t *map, word_t map_size, proc_map_t **proc_map_ptr)
{
    word_t offset = (word_t) region_ptr;
    word_t rem = 0;
    if (offset < (word_t) map || offset >= (word_t) (map + map_size) )
    {
        return EXCEPTION_SYSCALL_ERROR;
    }

    /*
     * GCC doesn't support modular division for AARCH 32 in some version
     * Code below is equivalent to:
     * offset -= map;
     * offset %= sizeof(proc_map_t);
     */
    rem = (offset - (word_t) map) / (word_t) sizeof(proc_map_t);
    offset -= rem * sizeof(proc_map_t) + (word_t) map  ;
    
    *proc_map_ptr = (proc_map_t *) (((word_t) region_ptr) - offset);
    return EXCEPTION_NONE;
}

static inline bool_t
region_is_start(paddr_t *region_ptr, proc_map_t *base)
{
    return ( (word_t) &(base->start) == (word_t) region_ptr );
}

static int
compare_regions(paddr_t *a, paddr_t *b) {
    if (*a < *b) {
        return -1;
    } else if (*b < *a) {
        return 1;
    } else {
        return 0;
    }
}

static int
compare_thread_regions(proc_map_t a, proc_map_t b) {
    return compare_regions(&(a.start), &(b.start));
}

static int
compare_threads(proc_map_t a, proc_map_t b) {
    if ((word_t) a.thread < (word_t) b.thread) {
        return -1;
    } else if ( (word_t) b.thread < (word_t) a.thread) {
        return 1;
    } else {
        return compare_thread_regions(a, b);
    }
}

/*
 * Page Tables and Page Directories are valid for all of ARM, 
 * whereas Page Upper Directories and Page Global Directories
 * are AARCH_64 specific. All x86 functionalities are not
 * currently supported.
 */
#ifdef CONFIG_ARCH_ARM

static inline page_permissions_t
permissionsFromAP(word_t AP, bool_t executable)
{
    page_permissions_t perms = PERMS_NONE;
    vm_rights_t current_rights = 0;
    for (current_rights = 0; current_rights <= 4 && AP != APFromVMRights(current_rights); ++current_rights){};
    
    switch (current_rights) {
    case VMReadWrite:
        perms |= PERMS_RW;
        break;

    case VMReadOnly:
        perms |= PERMS_R;
        break;
     
#ifdef CONFIG_ARCH_AARCH64
    case VMKernelReadOnly:
#elif CONFIG_ARCH_AARCH32
    case VMNoAccess:
#endif
    case VMKernelOnly:
        perms |= PERMS_NONE;
        break;

    default:
        fail("Invalid VM rights");
    }

    if (executable) {
        perms |= PERMS_X;
    }

    return perms;
}

static inline exception_t
debug_printPT(tcb_t *tcb, pte_t *pt, proc_map_t *map, word_t *current_index, word_t map_size)
{
    exception_t status = EXCEPTION_NONE;
    UNUSED pte_t *slot;
    UNUSED paddr_t frameBase;
    UNUSED word_t frameSize;
    UNUSED word_t count = 0;
    UNUSED page_permissions_t permissions;

#ifdef CONFIG_ARCH_AARCH64
    for(slot=pt; slot < pt + (paddr_t) BIT(PT_INDEX_BITS); slot += (paddr_t) 1)
    {
        if (pte_ptr_get_present(slot)) {
            frameBase = pte_ptr_get_page_base_address(slot);
            frameSize = BIT(pageBitsForSize(ARMSmallPage));
            permissions = permissionsFromAP(pte_ptr_get_AP(slot), !((bool_t) pte_ptr_get_UXN(slot)));
            status = insert_proc_map(map, current_index, map_size, 
                                     frameBase, frameSize, permissions, tcb);

            if (unlikely(status != EXCEPTION_NONE)) {
                return status;
            }
        }
    }
#elif CONFIG_ARCH_AARCH32
    for(slot=pt; slot < pt + (paddr_t) BIT(PT_INDEX_BITS); slot += (paddr_t) 1)
    {
        if (pte_ptr_get_pteType(slot) == pte_pte_small) {
            frameBase = pte_pte_small_ptr_get_address(slot);
    #ifdef CONFIG_ARM_HYPERVISOR_SUPPORT
            if (pte_pte_small_ptr_get_contiguous_hint(slot)) {
                /* Entries are represented as 16 contiguous small frames. We need to mask
                to get the large frame base */
                if (frameBase != frameBase & ~MASK(pageBitsForSize(ARMLargePage))) {
                    continue; /* We have already mapped this page. continue */
                } else {
                    frameBase &= ~MASK(pageBitsForSize(ARMLargePage));
                    frameSize = BIT(pageBitsForSize(ARMLargePage));
                }
            } else {
                frameSize = BIT(pageBitsForSize(ARMSmallPage));
            }
    #else /* if ( ! CONFIG_ARM_HYPERVISOR_SUPPORT ) */
            frameSize = BIT(pageBitsForSize(ARMSmallPage));
    #endif /* CONFIG_ARM_HYPERVISOR_SUPPORT */
            permissions = permissionsFromAP(pte_pte_small_ptr_get_AP(slot), 
                                            pte_pte_small_ptr_get_XN(slot) == 0);
            status = insert_proc_map(map, current_index, map_size,
                                     frameBase, frameSize, permissions, tcb);
        }
    #ifndef CONFIG_ARM_HYPERVISOR_SUPPORT
        else if (pte_ptr_get_pteType(slot) == pte_pte_large)
        {
            frameBase = pte_pte_large_ptr_get_address(slot);
            frameSize = BIT(pageBitsForSize(ARMLargePage));
            permissions = permissionsFromAP(pte_pte_large_ptr_get_AP(slot),
                                            pte_pte_large_ptr_get_XN(slot) == 0);
            status = insert_proc_map(map, current_index, map_size,
                                     frameBase, frameSize, permissions, tcb);
            slot += BIT(pageBitsForSize(ARMLargePage) - pageBitsForSize(ARMSmallPage)) -1;
        }
    #endif /* CONFIG_ARM_HYPERVISOR_SUPPORT */
        if (unlikely(status != EXCEPTION_NONE)) {
            return status;
        }
    }
#endif /* CONFIG_ARCH_AARCH64 || CONFIG_ARCH_AARCH32 */

    return status;
}

static inline exception_t
debug_printPD(tcb_t *tcb, pde_t *pd, proc_map_t *map, word_t *current_index, word_t map_size)
{
    exception_t status = EXCEPTION_NONE;
    UNUSED pde_t *slot;
    UNUSED pte_t *current_pte;
    UNUSED paddr_t frameBase;
    UNUSED word_t frameSize;
    UNUSED page_permissions_t permissions;

#ifdef CONFIG_ARCH_AARCH64    
    for(slot=pd; slot < pd + (paddr_t) BIT(PD_INDEX_BITS); slot += (paddr_t) 1)
    {
        if(pde_ptr_get_pde_type(slot) == pde_pde_large \
            && pde_pde_large_ptr_get_present(slot))
        {
            frameBase = pde_pde_large_ptr_get_page_base_address(slot);
            frameSize = BIT(pageBitsForSize(ARMLargePage));
            permissions = permissionsFromAP(pde_pde_large_ptr_get_AP(slot),
                                            !((bool_t) pde_pde_large_ptr_get_UXN(slot)));
            status = insert_proc_map(map, current_index, map_size,
                                     frameBase, frameSize, permissions, tcb);
        }
        else if(pde_ptr_get_pde_type(slot) == pde_pde_small \
                && pde_pde_small_ptr_get_present(slot))
        {
            frameBase = pde_pde_small_ptr_get_pt_base_address(slot);
            frameSize = BIT(seL4_PageTableBits);
            // printf("0x%18lx\t0x%18lx\t%s (PageTable)\n", frameBase, frameBase + frameSize - 1, tcb->tcbName);
            status = insert_proc_map(map, current_index, map_size,
                                     frameBase, frameSize, PAGE_TABLE_PERMS, tcb);
            if (unlikely(status != EXCEPTION_NONE)) {
                return status;
            }
            
            current_pte = paddr_to_pptr(frameBase);
            status = debug_printPT(tcb, current_pte, map, current_index, map_size);
        }

        if (unlikely(status != EXCEPTION_NONE)) {
            return status;
        }
    }

#elif CONFIG_ARCH_AARCH32
    /*
     * The PD for ARM_AARCH32 may copy global entries into the page directory
     * We don't need to see these, as they will obscure the regions we care about
     */
    paddr_t maxIndex = 0;
    #ifndef CONFIG_ARM_HYPERVISOR_SUPPORT
        maxIndex = kernelBase >> ARMSectionBits;
    #else
    #ifdef CONFIG_IPC_BUF_GLOBALS_FRAME
        maxIndex = BIT(PD_INDEX_BITS) - 1;
    #else
        maxIndex = BIT(PD_INDEX_BITS);
    #endif
    #endif

    for(slot=pd; slot < pd + maxIndex; slot += (paddr_t) 1)
    {
        switch (pde_ptr_get_pdeType(slot)) {
        case pde_pde_section:
            frameBase = pde_pde_section_ptr_get_address(slot);
    #ifndef CONFIG_ARM_HYPERVISOR_SUPPORT
            if (pde_pde_section_ptr_get_size(slot)) {
                frameSize = BIT(pageBitsForSize(ARMSuperSection));
                slot += BIT(pageBitsForSize(ARMSuperSection) - pageBitsForSize(ARMSection)) - 1; 
                /* the next n slots all correspond to this section */
            } else {
                frameSize = BIT(pageBitsForSize(ARMSection));
            }
    #else /* ifdef (CONFIG_ARM_HYPERVISOR_SUPPORT) */
            if (pde_pde_section_ptr_get_contiguous_hint(slot)) {
                if(frameBase != frameBase & ~MASK(pageBitsForSize(ARMSuperSection))) {
                    continue; /* We have already mapped this region. Continue */
                }
                else {
                    frameBase &= ~MASK(pageBitsForSize(ARMSuperSection));
                    frameSize = BIT(pageBitsForSize(ARMSuperSection));
                }
            } else {
                frameSize = BIT(pageBitsForSize(ARMSection));
            }
    #endif /* CONFIG_ARM_HYPERVISOR_SUPPORT */
            permissions = permissionsFromAP(pde_pde_section_ptr_get_AP(slot),
                                            pde_pde_section_ptr_get_XN(slot) == 0);
            status = insert_proc_map(map, current_index, map_size,
                                     frameBase, frameSize, permissions, tcb);
            break;

        case pde_pde_coarse:
            frameBase = pde_pde_coarse_ptr_get_address(slot);
            frameSize = BIT(seL4_PageTableBits);
            status = insert_proc_map(map, current_index, map_size,
                                     frameBase, frameSize, PAGE_TABLE_PERMS, tcb);
            if (unlikely(status != EXCEPTION_NONE)) {
                return status;
            }
            
            current_pte = paddr_to_pptr(frameBase);
            status = debug_printPT(tcb, current_pte, map, current_index, map_size);
            break;
        }

        if (unlikely(status != EXCEPTION_NONE)) {
            return status;
        }
    }
#endif /* CONFIG_ARCH_AARCH64 || CONFIG_ARCH_AARCH32 */
    return status;
}

# ifdef CONFIG_ARCH_AARCH64
static inline exception_t
debug_printPUD(tcb_t *tcb, pude_t *pude, proc_map_t *map, word_t *current_index, word_t map_size)
{
    exception_t status = EXCEPTION_NONE;
    UNUSED pude_t *slot;
    UNUSED pde_t *current_pde;
    UNUSED paddr_t frameBase;
    UNUSED word_t frameSize;
    UNUSED page_permissions_t permissions;

    for(slot=pude; slot < pude + (paddr_t) BIT(PUD_INDEX_BITS); slot += (paddr_t) 1)
    {

        if(pude_ptr_get_pude_type(slot) == pude_pude_1g \
            && pude_pude_1g_ptr_get_present(slot))
        {
            frameBase = pude_pude_1g_ptr_get_page_base_address(slot);
            frameSize = BIT(pageBitsForSize(ARMHugePage));
            permissions = permissionsFromAP(pude_pude_1g_ptr_get_AP(slot),
                                            pude_pude_1g_ptr_get_UXN(slot) == 0);
            status = insert_proc_map(map, current_index, map_size,
                                     frameBase, frameSize, permissions, tcb);
        }
        else if(pude_ptr_get_pude_type(slot) == pude_pude_pd \
                 && pude_pude_pd_ptr_get_present(slot))
        {
            frameBase = pude_pude_pd_ptr_get_pd_base_address(slot);
            frameSize = BIT(seL4_PageDirBits);
            // printf("0x%18lx\t0x%18lx\t%s (PD)\n", frameBase, frameBase + frameSize - 1, tcb->tcbName);
            status = insert_proc_map(map, current_index, map_size,
                                     frameBase, frameSize, PAGE_TABLE_PERMS, tcb);
            if (unlikely(status != EXCEPTION_NONE)) {
                return status;
            }

            current_pde = paddr_to_pptr(frameBase);
            status = debug_printPD(tcb, current_pde, map, current_index, map_size);
        }

        if (unlikely(status != EXCEPTION_NONE)) {
            return status;
        }
    }
    return status;
}

static inline exception_t
debug_printPGD(tcb_t *tcb, pgde_t *pgd, proc_map_t *map, word_t *current_index, word_t map_size)
{
    exception_t status = EXCEPTION_NONE;
    UNUSED pgde_t *slot;
    UNUSED pude_t *current_pude;
    UNUSED paddr_t frameBase;
    UNUSED word_t frameSize;

    frameBase = pptr_to_paddr(pgd);
    frameSize = BIT(seL4_PGDBits);
    // printf("0x%18lx\t0x%18lx\t%s (PGD)\n", frameBase, frameBase + frameSize - 1, tcb->tcbName);
    status = insert_proc_map(map, current_index, map_size,
                             frameBase, frameSize, PAGE_TABLE_PERMS, tcb);
    if (unlikely(status != EXCEPTION_NONE)) {
        return status;
    }

    for(slot=pgd; slot < pgd + (paddr_t) BIT(PGD_INDEX_BITS); slot += (paddr_t) 1)
    {
        if(pgde_ptr_get_present(slot))
        {
            
            frameBase = pgde_ptr_get_pud_base_address(slot);
            frameSize = BIT(seL4_PUDBits);
            // printf("0x%18lx\t0x%18lx\t%s (PUD)\n", frameBase, frameBase + frameSize - 1, tcb->tcbName);
            status = insert_proc_map(map, current_index, map_size,
                                     frameBase, frameSize, PAGE_TABLE_PERMS, tcb);
            
            if (unlikely(status != EXCEPTION_NONE)) {
                return status;
            }

            current_pude = paddr_to_pptr(frameBase);
            status = debug_printPUD(tcb, current_pude, map, current_index, map_size);

            if (unlikely(status != EXCEPTION_NONE)) {
                return status;
            }
        }
    }
    return status;
}
#endif /* CONFIG_ARCH_AARCH64 */

static inline vspace_root_t*
tcb_ptr_get_vspace_root_ptr(tcb_t *tcb)
{
    vspace_root_t *vspace = NULL;
    UNUSED cap_t threadRoot;

    threadRoot = TCB_PTR_CTE_PTR(tcb, tcbVTable)->cap;

#ifdef CONFIG_ARCH_AARCH64
    if (isValidNativeRoot(threadRoot)) {
        vspace = PGD_PTR(cap_page_global_directory_cap_get_capPGDBasePtr(threadRoot));
    }
#elif CONFIG_ARCH_AARCH32
    if (cap_get_capType(threadRoot) == cap_page_directory_cap &&
        cap_page_directory_cap_get_capPDIsMapped(threadRoot)) {
        vspace = PDE_PTR(cap_page_directory_cap_get_capPDBasePtr(threadRoot));
    }
#endif /* CONFIG_ARCH_AARCH64 || CONFIG_ARCH_AARCH32 */
    
    return vspace;
}

static inline exception_t
debug_print_from_vroot(tcb_t *tcb, vspace_root_t *vspace, proc_map_t *map, word_t *current_index, word_t map_size)
{
#ifdef CONFIG_ARCH_AARCH64
    return debug_printPGD(tcb, vspace, map, current_index, map_size);
#elif CONFIG_ARCH_AARCH32
    return debug_printPD(tcb, vspace, map, current_index, map_size);
#else
    return EXCEPTION_NONE;
#endif /* CONFIG_ARCH_AARCH64 || CONFIG_ARCH_AARCH32 */
}
#endif /* CONFIG_ARCH_ARM */

static inline exception_t
debug_traverseVSpace(tcb_t *tcb, proc_map_t *map, word_t *current_index, word_t map_size)
{
    vspace_root_t *vspace = NULL;
    vspace = tcb_ptr_get_vspace_root_ptr(tcb);
    if(vspace == NULL)
    {
        printf("%15s\t%20s\n", tcb->tcbName, "Unknown VSpace Root");
        return EXCEPTION_NONE;
    }
    return debug_print_from_vroot(tcb, vspace, map, current_index, map_size);
}

// static inline void
// debug_printProcMap(proc_map_t *map, word_t current_index)
// {
//     word_t index = 0;
//     paddr_t max_seen = 0;
//     printf("%20s\t%20s\t%20s\n", "Start", "Size", "Thread");
//     printf("------------------------------------------------------------\n");
//     for(index = 0; index < current_index; ++index)
//     {
//         if (index > 0 && map[index].start < max_seen) {
//             printf("0x%18lx\t0x%18lx\t%s (Overlap)\n", map[index].start, map[index].end - map[index].start + 1, map[index].thread->tcbName);
//         } else {
//             printf("0x%18lx\t0x%18lx\t%s\n", map[index].start, map[index].end - map[index].start + 1, map[index].thread->tcbName);
//         }

//         if (max_seen < map[index].end){
//             max_seen = map[index].end;
//         }
//     }
// }

static inline char* CONST
rights_string(page_permissions_t perms) {
    switch(perms) {
    case PERMS_NONE:
        return "---";
    case PERMS_X:
        return "--x";
    case PERMS_W:
        return "-w-";
    case PERMS_WX:
        return "-wx";
    case PERMS_R:
        return "r--";
    case PERMS_RX:
        return "r-x";
    case PERMS_RW:
        return "rw-";
    case PERMS_RWX:
        return "rwx";
    case PERMS_PageTable:
        return "PageTable";
    default:
        fail("Unknown Permissions Type");
    }
    return "";
}

static inline void
debug_printProcMap(proc_map_t *map, paddr_t **region, word_t current_index)
{
    UNUSED exception_t status;
    word_t index = 0;
    word_t start_index = 0;
    paddr_t start_addr = 0;
    paddr_t end_addr = 0;
    proc_map_t *a = NULL;
    proc_map_t *head = NULL;
    proc_map_t *curr = NULL;
    UNUSED word_t num_executable = 0;
    UNUSED word_t usedSpace = 0;
    UNUSED word_t totalSpace = 0;
    UNUSED word_t freeSpace = 0;

    printf("\n%20s\t", "Start");
    printf("%20s\t", "Size");
    printf("%20s\n", "Thread(s)");
    printf("--------------------------------------------------------------------------\n");
    while(index < 2 * current_index)
    {
        start_index = index;
        
        /* If we are at the end of a region, we want to start our next region at the next possible address */
        status = region_get_proc_map(region[start_index], map, current_index, &a);
        start_addr = *region[start_index] + (region_is_start(region[start_index], a) ? 0 : 1);

        while(*region[start_index] == *region[index] && index < 2 * current_index)
        {
            status = region_get_proc_map(region[index], map, current_index, &a);
            if (unlikely(status != EXCEPTION_NONE)) {
                printf("ProcMap Printing Error\n");
                return;
            }

            if(region_is_start(region[index], a)) {
                if(a->next != NULL || a->prev != NULL) {
                    printf("ProcMap Printing Error\n");
                    return;
                }

                if(head != NULL) {
                    head->prev = a;
                }
                a->next = head;
                head = a;
            } else {
                if (head == a) {
                    head = a->next;
                }
                if(a->prev != NULL) {
                    a->prev->next = a->next;
                }
                if(a->next != NULL) {
                    a->next->prev = a->prev;
                }
                a->next = NULL;
                a->prev = NULL;
            }

            index += 1;
        }

        /* If we are at the start of a region (region overlap), we want to start end the current region at the previous address */
        status = region_get_proc_map(region[index], map, current_index, &a);
        end_addr = *region[index] + (region_is_start(region[index], a) ? -1 : 0);

        if( head != NULL && index < 2 * current_index)
        {
            usedSpace += end_addr - start_addr + 1;
            printf("0x%18lx\t", start_addr);
            printf("0x%18lx\t", end_addr - start_addr + 1);
            
            for (curr = head; curr != NULL; curr = curr->next) {
                    printf("%s (%s); ", curr->thread->tcbName, rights_string(curr->permissions));
            }
            printf("\n");
        }
    }

    #ifdef CONFIG_ARCH_ARM
    p_region_t current_region;
    for(index = 0; index < get_num_avail_p_regs(); ++index)
    {
        current_region = get_avail_p_reg(index);
        totalSpace += current_region.end - current_region.start;
    }
    freeSpace = totalSpace - usedSpace;
    #endif

    printf("SUMMARY:\n");
    if(totalSpace > 0) {
        printf("Total Space (Bytes): %lu\n", (long unsigned) totalSpace);
    } else {
        printf("Total Space Unknown\n");
    }

    printf("Used Space (Bytes): %lu\n", (long unsigned) usedSpace);
    
    if (totalSpace > 0) {
        printf("Free Space (Bytes): %lu\n", (long unsigned) freeSpace);
    }

}

/* 
 * Note: The proc map has been set up to not print multiple threads in the same VSpace
 * to save space, but in SMP systems, it will print the oldest thread from each core from
 * a VSpace because the debug TCB list is core-specific
 */
static inline void
debug_procMap(void)
{
    UNUSED exception_t status = EXCEPTION_NONE;
    UNUSED word_t working_index = 0;
    word_t index = 0;
    word_t current_index = 0;
    word_t size = ARRAY_SIZE(maps);
    tcb_t *curr = NULL;
    int core = 0;
    tcb_t *potential_parent = NULL;
    bool_t curr_is_parent = true;

    memzero(maps, sizeof(maps));
    memzero(regions, sizeof(regions));

    printf("Dumping all tcb addresses!\n");
    for (core = 0; core < get_num_cores() && status == EXCEPTION_NONE; core++)
    {
        for (curr = NODE_STATE_ON_CORE(ksDebugTCBs, core); curr != NULL && status == EXCEPTION_NONE; curr = curr->tcbDebugNext) {
            curr_is_parent = true;
            for (potential_parent = curr->tcbDebugNext; 
                potential_parent != NULL && curr_is_parent; 
                potential_parent = potential_parent->tcbDebugNext)
            {
                curr_is_parent &= ( tcb_ptr_get_vspace_root_ptr(curr) == NULL \
                                || tcb_ptr_get_vspace_root_ptr(curr) != tcb_ptr_get_vspace_root_ptr(potential_parent));
            }
            if(curr_is_parent)
            {
                status = debug_traverseVSpace(curr, maps, &current_index, size);
            }
        }
    }

    if( unlikely((status == EXCEPTION_FAULT) || (current_index * 2 > ARRAY_SIZE(regions))) ) {
        printf("Error: Not enough memory to track all memory addresses.\n");
        return;
    }

    if( unlikely(status == EXCEPTION_SYSCALL_ERROR)) {
        printf("Error: Improper value in TCB\n");
        return;
    }

    /*
     * Combine contiguous regions held by a single thread
     */

    SGLIB_ARRAY_SINGLE_QUICK_SORT(UNUSED proc_map_t, maps, current_index, compare_threads);

    working_index = 0;
    for(index=0; index < current_index; ++index)
    {
        regions[working_index++] = &(maps[index].start);
        regions[working_index++] = &(maps[index].end);
        if (index + 1 >= current_index || maps[index + 1].thread != maps[index].thread)
        {
            proc_map_t *a = NULL;
            proc_map_t *b = NULL;
            /* Combine regions for this tcb */
            working_index -= 1;
            for(; working_index > 1; --working_index)
            {
                if( *regions[working_index - 1] + 1 == *regions[working_index])
                {
                    status = region_get_proc_map(regions[working_index], maps, current_index, &a);
                    status = region_get_proc_map(regions[working_index - 1], maps, current_index, &b);
                    if( a != NULL && b != NULL && \
                        region_is_start(regions[working_index], a) && \
                        !region_is_start(regions[working_index - 1], b) &&
                        b->permissions == a->permissions)
                    {
                        a->entry_valid = false;
                        b->end = a->end;
                        working_index -= 1;
                    }
                }
            }
            working_index = 0;
        }
    }

    working_index = 0;
    for(index = 0; index < current_index; ++index)
    {
        if(maps[index].entry_valid)
        {
            maps[working_index] = maps[index];
            maps[index].entry_valid = (working_index == index) ? true : false;
            working_index += 1;
        }
    }

    // for(index=1; index < current_index; ++index)
    // {
    //     /* Combine Contiguous regions held by a single thread */
    //     if(maps[working_index].start + maps[working_index].size == maps[index].start \
    //         && maps[working_index].thread == maps[index].thread) {
    //         maps[working_index].size += maps[index].size;
    //     } else {
    //         working_index += 1;
    //         maps[working_index] = maps[index];
    //     }
    // }
    // current_index = working_index + 1;

    /*
     * Iterate through the regions and print
     */

    current_index = working_index;
    working_index = 0;
    for(index = 0; index < current_index; ++index)
    {
        regions[2*index] = &(maps[index].start);
        regions[2*index + 1] = &(maps[index].end);
    }

    SGLIB_ARRAY_SINGLE_QUICK_SORT(UNUSED paddr_t *, regions, (current_index * 2), compare_regions);

    proc_map_t *check = NULL;
    status = region_get_proc_map(regions[0], maps, current_index, &check);
    if (unlikely(status != EXCEPTION_NONE || check == NULL || !region_is_start(regions[0], check)))
    {
        printf("ProcMap Printing Error\n");
        return;
    }

    debug_printProcMap(maps, regions, current_index);

}

#endif /* CONFIG_PRINTING */
#endif /* __API_DEBUG_H */
#endif /* CONFIG_DEBUG_BUILD */
