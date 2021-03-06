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


#include <config.h>
#include <arch/machine/gic_pl390.h>

static inline CONST cpu_id_t
MYgetCurrentCPUIndex(void)
{
    cpu_id_t cpu_id;
    asm volatile ("mrs %[cpu_id], mpidr_el1" : [cpu_id] "=r" (cpu_id) ::);
    return cpu_id&0x0f;
}

#define TARGET_CPU_ALLINT(CPU) ( \
        ( ((CPU)&0xff)<<0u  ) |\
        ( ((CPU)&0xff)<<8u  ) |\
        ( ((CPU)&0xff)<<16u ) |\
        ( ((CPU)&0xff)<<24u ) \
    )
#define TARGET_CPU0_ALLINT   TARGET_CPU_ALLINT(BIT(0))
#define IRQ_SET_ALL 0xffffffff;

/* Shift positions for GICD_SGIR register */
#define GICD_SGIR_SGIINTID_SHIFT          0
#define GICD_SGIR_CPUTARGETLIST_SHIFT     16
#define GICD_SGIR_TARGETLISTFILTER_SHIFT  24

#ifndef GIC_PL390_DISTRIBUTOR_PPTR
#error GIC_PL390_DISTRIBUTOR_PPTR must be defined for virtual memory access to the gic distributer
#else  /* GIC_DISTRIBUTOR_PPTR */
volatile struct gic_dist_map * gic_dist =
    (volatile struct gic_dist_map*)(GIC_PL390_DISTRIBUTOR_PPTR);
#endif /* GIC_DISTRIBUTOR_PPTR */

#ifndef GIC_PL390_CONTROLLER_PPTR
#error GIC_PL390_CONTROLLER_PPTR must be defined for virtual memory access to the gic cpu interface
#else  /* GIC_CONTROLLER_PPTR */
volatile struct gic_cpu_iface_map * gic_cpuiface =
    (volatile struct gic_cpu_iface_map*)(GIC_PL390_CONTROLLER_PPTR);
#endif /* GIC_CONTROLLER_PPTR */

uint32_t active_irq[CONFIG_MAX_NUM_NODES] = {IRQ_NONE};

#ifdef ENABLE_SMP_SUPPORT
/*
* 25-24: target lister filter
* 0b00 - send the ipi to the CPU interfaces specified in the CPU target list
* 0b01 - send the ipi to all CPU interfaces except the cpu interface.
*        that requrested teh ipi
* 0b10 - send the ipi only to the CPU interface that requested the IPI.
* 0b11 - reserved
*.
* 23-16: CPU targets list
* each bit of CPU target list [7:0] refers to the corresponding CPU interface.
* 3-0:   SGIINTID
* software generated interrupt id, from 0 to 15...
*/
void ipiBroadcast(irq_t irq, bool_t includeSelfCPU)
{
//printf("ipiBroadcast val 0x%x, irq 0x%x, include = %s\n", (!includeSelfCPU << GICD_SGIR_TARGETLISTFILTER_SHIFT) | (irq << GICD_SGIR_SGIINTID_SHIFT), irq, includeSelfCPU?"true":"false");
    gic_dist->sgi_control = (!includeSelfCPU << GICD_SGIR_TARGETLISTFILTER_SHIFT) | (irq << GICD_SGIR_SGIINTID_SHIFT);
}

void ipi_send_target(irq_t irq, word_t cpuTargetList)
{
//printf("ipi_send_target val 0x%x, irq 0x%x, target 0x%x\n", (int)((cpuTargetList << GICD_SGIR_CPUTARGETLIST_SHIFT) | (irq << GICD_SGIR_SGIINTID_SHIFT)), irq, (int)cpuTargetList);
    gic_dist->sgi_control = (cpuTargetList << GICD_SGIR_CPUTARGETLIST_SHIFT) | (irq << GICD_SGIR_SGIINTID_SHIFT);
}
#endif /* ENABLE_SMP_SUPPORT */


#ifndef CONFIG_PLAT_ZYNQMP

BOOT_CODE static void
dist_init(void)
{
    word_t i;
    int nirqs = 32 * ((gic_dist->ic_type & 0x1f) + 1);
    gic_dist->enable = 0;

    for (i = 0; i < nirqs; i += 32) {
        /* disable */
        gic_dist->enable_clr[i >> 5] = IRQ_SET_ALL;
        /* clear pending */
        gic_dist->pending_clr[i >> 5] = IRQ_SET_ALL;
    }

    /* reset interrupts priority */
    for (i = 32; i < nirqs; i += 4) {
        if (config_set(CONFIG_ARM_HYPERVISOR_SUPPORT)) {
            gic_dist->priority[i >> 2] = 0x80808080;
        } else {
            gic_dist->priority[i >> 2] = 0;
        }
    }

    /*
     * reset int target to cpu 0
     * (Should really query which processor we're running on and use that)
     */
    for (i = 0; i < nirqs; i += 4) {
        gic_dist->targets[i >> 2] = TARGET_CPU0_ALLINT;
    }

    /* level-triggered, 1-N */
    for (i = 64; i < nirqs; i += 32) {
        gic_dist->config[i >> 5] = 0x55555555;
    }

    /* group 0 for secure; group 1 for non-secure */
    for (i = 0; i < nirqs; i += 32) {
        if (config_set(CONFIG_ARM_HYPERVISOR_SUPPORT)) {
            gic_dist->security[i >> 5] = 0xffffffff;
        } else {
            gic_dist->security[i >> 5] = 0;
        }
    }
    /* enable the int controller */
    gic_dist->enable = 1;
}

BOOT_CODE static void
cpu_iface_init(void)
{
    uint32_t i;

    /* For non-Exynos4, the registers are banked per CPU, need to clear them */
    gic_dist->enable_clr[0] = IRQ_SET_ALL;
    gic_dist->pending_clr[0] = IRQ_SET_ALL;

    /* put everything in group 0; group 1 if in hyp mode */
    if (config_set(CONFIG_ARM_HYPERVISOR_SUPPORT)) {
        gic_dist->security[0] = 0xffffffff;
        gic_dist->priority[0] = 0x80808080;
    } else {
        gic_dist->security[0] = 0;
        gic_dist->priority[0] = 0x0;
    }

    /* clear any software generated interrupts */
    for (i = 0; i < 16; i += 4) {
        gic_dist->sgi_pending_clr[i >> 2] = IRQ_SET_ALL;
    }

    gic_cpuiface->icontrol = 0;
    /* the write to priority mask is ignored if the kernel is
     * in non-secure mode and the priority mask is already configured
     * by secure mode software. the elfloader should config the
     * interrupt routing properly to ensure that the hyp-mode kernel
     * can get interrupts
     */
    gic_cpuiface->pri_msk_c = 0x000000f0;
    gic_cpuiface->pb_c = 0x00000003;

    i = gic_cpuiface->int_ack;
    while ((i & IRQ_MASK) != IRQ_NONE) {
        gic_cpuiface->eoi = i;
        i = gic_cpuiface->int_ack;
    }
    gic_cpuiface->icontrol = 1;
}


#else /* CONFIG_PLAT_ZYNQMP */

BOOT_CODE static void
dist_init(void)
{
}

BOOT_CODE static void
dist_init_real(void)
{
    word_t i;
    int nirqs = 32 * ((gic_dist->ic_type & 0x1f) + 1);
    int ncpus =  1 + ((gic_dist->ic_type & 0x0e0) >> 5);
    int cpunum = MYgetCurrentCPUIndex();
    uint32_t cpunum32;

    cpunum32 = gic_dist->targets[0]&0xff;
    if(cpunum32 != (1<<cpunum))printf("dist_init id missmatch: %d and %d\n", cpunum, (int)cpunum32);
    if(ncpus != CONFIG_MAX_NUM_NODES)printf("dist_init core count missmatch: %d and %d\n", ncpus, CONFIG_MAX_NUM_NODES);

    for (i = 0; i < nirqs; i += 32) {
        /* DISBABLE before configuring */
        gic_dist->enable_clr[i >> 5] = IRQ_SET_ALL;
    }

    /* reset interrupts priority */
    for (i = 32; i < nirqs; i += 4) {
        gic_dist->priority[i >> 2] = 0x80808080;
    }

   /*
     * reset int target to current core
     */
    for (i = 0; i < nirqs; i += 4) {
        gic_dist->targets[i >> 2] = TARGET_CPU_ALLINT(BIT(cpunum));
    }

    /* level-triggered, 1-N */
    for (i = 64; i < nirqs; i += 32) {
        gic_dist->config[i >> 5] = 0x55555555;
    }

    /* group 0 for non-secure; group 1 for secure */
    for (i = 0; i < nirqs; i += 32) {
        gic_dist->security[i >> 5] = 0xffffffff;
    }

    /* clear any software generated interrupts */
    for (i = 0; i < 16; i += 4) {
        gic_dist->sgi_pending_clr[i >> 2] = IRQ_SET_ALL;
    }

    for (i = 0; i < nirqs; i += 32) {
        /* clear pending */
        gic_dist->pending_clr[i >> 5] = IRQ_SET_ALL;
    }
    for (i = 0; i < nirqs; i += 32) {
        /* enable */
        gic_dist->enable_set[i >> 5] = IRQ_SET_ALL;
    }

}

BOOT_CODE static void
cpu_iface_init(void)
{
    uint32_t i;

    /* disable the int controller */
    gic_dist->enable = 0;
    dist_init_real();

    gic_cpuiface->icontrol = 0;
    /* the write to priority mask is ignored if the kernel is
     * in non-secure mode and the priority mask is already configured
     * by secure mode software. the elfloader should config the
     * interrupt routing properly to ensure that the hyp-mode kernel
     * can get interrupts
     */
    gic_cpuiface->pri_msk_c = 0x000000ff;
    gic_cpuiface->pb_c = 0x00000003;

    i = gic_cpuiface->int_ack;
    while ((i & IRQ_MASK) != IRQ_NONE) {
        gic_cpuiface->eoi = i;
        i = gic_cpuiface->int_ack;
    }

    gic_cpuiface->icontrol = 3;

    gic_dist->enable = 3;

}
#endif

BOOT_CODE void
initIRQController(void)
{
    dist_init();
}

BOOT_CODE void cpu_initLocalIRQController(void)
{
    cpu_iface_init();
}

