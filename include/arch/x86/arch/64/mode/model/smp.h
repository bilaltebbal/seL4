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

/*
 * Copyright 2018, Intelligent Automation, Inc.
 * This software was developed in part under Air Force contract number FA8750-15-C-0066 and DARPA 
 *  contract number 140D6318C0001.
 * This software was released under DARPA, public release number 1.0.
 * This software may be distributed and modified according to the terms of the BSD 2-Clause license.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 * WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(IAI_BSD)
 */


#ifndef __MODE_MODEL_SMP_H_
#define __MODE_MODEL_SMP_H_

#include <config.h>

#ifdef ENABLE_SMP_SUPPORT

typedef struct nodeInfo {
    void *stackTop;
    void *irqStack;
    /* This is the address (+8 bytes) of the 'Error' register of the user
     * context of the current thread. This address is designed so that the
     * 'syscall' trap code can do
     * movq %gs:16, %rsp
     * pushq $-1
     * pushq %rcx (rcx holds NextIP)
     * pushq %r11 (r11 holds RFLAGS)
     * etc etc
     * This value needs to be updated every time we switch to the user
     */
    word_t currentThreadUserContext;
    cpu_id_t index;
    PAD_TO_NEXT_CACHE_LN(sizeof(void*) + sizeof(void*) + sizeof(word_t) + sizeof(cpu_id_t));
} nodeInfo_t;
compile_assert(nodeInfoIsCacheSized, (sizeof(nodeInfo_t) % L1_CACHE_LINE_SIZE) == 0)

extern nodeInfo_t node_info[CONFIG_MAX_NUM_NODES] ALIGN(L1_CACHE_LINE_SIZE);

#ifdef CONFIG_KERNEL_SKIM_WINDOW
/* we only need 1 word of scratch space, which we know we will at least get by the size
 8 of the nodeInfo_t struct so we define this array as char to ensure it is sized correctly.
 We need each element of the array to be precisely the same size as each element of node_info
 so that the offset between the element of each array is a constant */
extern char nodeSkimScratch[CONFIG_MAX_NUM_NODES][sizeof(nodeInfo_t)] ALIGN(L1_CACHE_LINE_SIZE) VISIBLE SKIM_BSS;
compile_assert(nodeInfoAndScratchSameSize, sizeof(node_info) == sizeof(nodeSkimScratch))
/* this will be declared in the linker script as the offset between node_info and
 * nodeSkimScratch */
extern char nodeSkimScratchOffset[];
#endif

#if 0
static inline CONST cpu_id_t getCurrentCPUIndex(void)
{
    cpu_id_t index;
    asm ("movq %%gs:%c[offset], %[result]"
         : [result] "=r" (index)
         : [offset] "i" (OFFSETOF(nodeInfo_t, index)));
    return index;
}
#endif

BOOT_CODE void
mode_init_tls(cpu_id_t cpu_index);

#endif /* ENABLE_SMP_SUPPORT */

#endif /* __MODE_MODEL_SMP_H_ */
