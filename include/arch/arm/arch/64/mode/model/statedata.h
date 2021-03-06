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


#ifndef __ARCH_MODEL_STATEDATA_64_H
#define __ARCH_MODEL_STATEDATA_64_H

#include <config.h>
#include <types.h>
#include <arch/types.h>
#include <util.h>
#include <object/structures.h>

/* The top level asid mapping table */
extern asid_pool_t *armKSASIDTable[BIT(asidHighBits)] VISIBLE;

/* This is the temporary userspace page table in kernel. It is required before running
 * user thread to avoid speculative page table walking with the wrong page table. */
extern pgde_t armKSGlobalUserPGD[BIT(PGD_INDEX_BITS)] VISIBLE;
extern pgde_t armKSGlobalKernelPGD[BIT(PGD_INDEX_BITS)] VISIBLE;

extern pude_t armKSGlobalKernelPUD[BIT(PUD_INDEX_BITS)] VISIBLE;
extern pde_t armKSGlobalKernelPDs[BIT(PUD_INDEX_BITS)][BIT(PD_INDEX_BITS)] VISIBLE;
extern pte_t armKSGlobalKernelPT[BIT(PT_INDEX_BITS)] VISIBLE;

#ifdef CONFIG_ARM_HYPERVISOR_SUPPORT
extern vcpu_t *armHSCurVCPU;
extern bool_t armHSVCPUActive;
#endif
#endif /* __ARCH_MODEL_STATEDATA_64_H */
