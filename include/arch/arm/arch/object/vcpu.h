/*
 * Copyright 2014, General Dynamics C4 Systems
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(GD_GPL)
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


#ifndef __ARCH_OBJECT_VCPU_H
#define __ARCH_OBJECT_VCPU_H

#include <config.h>

#ifdef CONFIG_ARM_HYPERVISOR_SUPPORT

#include <api/failures.h>
#include <linker.h>

#define GIC_VCPU_MAX_NUM_LR 64

struct cpXRegs {
    uint32_t sctlr;
    uint32_t actlr;
};

struct gicVCpuIface {
    uint32_t hcr;
    uint32_t vmcr;
    uint32_t apr;
    virq_t lr[GIC_VCPU_MAX_NUM_LR];
};

struct vcpu {
    /* TCB associated with this VCPU. */
    struct tcb *vcpuTCB;
    struct cpXRegs cpx;
    struct gicVCpuIface vgic;
#ifdef CONFIG_ARCH_AARCH64
    word_t regs[seL4_VCPUReg_Num];
#else
    /* Banked registers */
    word_t lr_svc, sp_svc;
    word_t lr_abt, sp_abt;
    word_t lr_und, sp_und;
    word_t lr_irq, sp_irq;
    word_t lr_fiq, sp_fiq, r8_fiq, r9_fiq, r10_fiq, r11_fiq, r12_fiq;
#endif
};
typedef struct vcpu vcpu_t;
compile_assert(vcpu_size_correct, sizeof(struct vcpu) <= BIT(VCPU_SIZE_BITS))

void VGICMaintenance(void);
void handleVCPUFault(word_t hsr);

void vcpu_init(vcpu_t *vcpu);

/* Performs one off initialization of VCPU state and structures. Should be
 * called in boot code before any other VCPU functions */
BOOT_CODE void vcpu_boot_init(void);

void vcpu_finalise(vcpu_t *vcpu);

void associateVCPUTCB(vcpu_t *vcpu, tcb_t *tcb);

void dissociateVCPUTCB(vcpu_t *vcpu, tcb_t *tcb);

exception_t decodeARMVCPUInvocation(
    word_t label,
    unsigned int length,
    cptr_t cptr,
    cte_t* slot,
    cap_t cap,
    extra_caps_t extraCaps,
    bool_t call,
    word_t* buffer
);

void vcpu_restore(vcpu_t *cpu);
void vcpu_switch(vcpu_t *cpu);

exception_t decodeVCPUWriteReg(cap_t cap, unsigned int length, word_t* buffer);
exception_t decodeVCPUReadReg(cap_t cap, unsigned int length, bool_t call, word_t* buffer);
exception_t decodeVCPUInjectIRQ(cap_t cap, unsigned int length, word_t* buffer);
exception_t decodeVCPUSetTCB(cap_t cap, extra_caps_t extraCaps);

exception_t invokeVCPUWriteReg(vcpu_t *vcpu, uint32_t field, uint32_t value);
exception_t invokeVCPUReadReg(vcpu_t *vcpu, uint32_t field, bool_t call);
exception_t invokeVCPUInjectIRQ(vcpu_t *vcpu, unsigned long index, virq_t virq);
exception_t invokeVCPUSetTCB(vcpu_t *vcpu, tcb_t *tcb);

#else /* end of CONFIG_ARM_HYPERVISOR_SUPPORT */

/* used in boot.c with a guard, use a marco to avoid exposing vcpu_t */
#define vcpu_boot_init() do {} while(0)
#define vcpu_switch(x) do {} while(0)
static inline void VGICMaintenance(void) {}

#endif /* end of !CONFIG_ARM_HYPERVISOR_SUPPORT */

#endif /* __ARCH_OBJECT_VCPU_H */
