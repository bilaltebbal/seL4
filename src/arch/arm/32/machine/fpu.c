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


#include <mode/machine.h>
#include <arch/machine/fpu.h>
#include <mode/model/statedata.h>
#include <config.h>
#include <util.h>

/* We cache the following value to avoid reading the coprocessor when isFpuEnable()
 * is called. enableFpu() and disableFpu(), the value is set to cache/reflect the
 * actual HW FPU enable/disable state.
 */
bool_t isFPUEnabledCached[CONFIG_MAX_NUM_NODES];

/*
 * The following function checks if the subarchitecture support asynchronous exceptions
 */
BOOT_CODE static inline bool_t supportsAsyncExceptions(void)
{
    word_t fpexc;

    /* Set FPEXC.EX=1 */
    MRC(FPEXC, fpexc);
    fpexc |= BIT(FPEXC_EX_BIT);
    MCR(FPEXC, fpexc);

    /* Read back the FPEXC register*/
    MRC(FPEXC, fpexc);

    return !!(fpexc & BIT(FPEXC_EX_BIT));
}

#ifdef CONFIG_HAVE_FPU
/* This variable is set at boot/init time to true if the FPU supports 32 registers (d0-d31).
 * otherwise it only supports 16 registers (d0-d15).
 * We cache this value in the following variable to avoid reading the coprocessor
 * on every FPU context switch, since it shouldn't change for one platform on run-time.
 */
bool_t isFPUD32SupportedCached;

BOOT_CODE static inline bool_t isFPUD32Supported(void)
{
    word_t mvfr0;
    asm volatile (".word 0xeef73a10 \n"  /* vmrs    r3, mvfr0 */
                  "mov %0, r3       \n"
                  : "=r" (mvfr0)
                  :
                  : "r3");
    return ((mvfr0 & 0xf) == 2);
}

/* Initialise the FP/SIMD for this machine. */
BOOT_CODE bool_t
fpsimd_init(void)
{
    word_t cpacr;

    MRC(CPACR, cpacr);
    cpacr |= (CPACR_CP_ACCESS_PLX << CPACR_CP_10_SHIFT_POS |
              CPACR_CP_ACCESS_PLX << CPACR_CP_11_SHIFT_POS);
    MCR(CPACR, cpacr);

    isb();

    if (supportsAsyncExceptions()) {
        /* In the future, when we've targets that support asynchronous FPU exceptions, we've to support them */
        printf("Error: seL4 doesn't support FPU subarchitectures that support asynchronous exceptions\n");
        return false;
    }

    isFPUD32SupportedCached = isFPUD32Supported();
    /* Set the FPU to lazy switch mode */
    disableFpu();

    return true;
}
#endif /* CONFIG_HAVE_FPU */

BOOT_CODE bool_t
fpsimd_HWCapTest(void)
{
    word_t cpacr, fpsid;

    /* Change permissions of CP10 and CP11 to read control/status registers */
    MRC(CPACR, cpacr);
    cpacr |= (CPACR_CP_ACCESS_PLX << CPACR_CP_10_SHIFT_POS |
              CPACR_CP_ACCESS_PLX << CPACR_CP_11_SHIFT_POS);
    MCR(CPACR, cpacr);

    isb();

    /* Check of this platform supports HW FP instructions */
    asm volatile (".word 0xeef00a10  \n" /* vmrs    r0, fpsid */
                  "mov %0, r0        \n"
                  : "=r" (fpsid) :
                  : "r0");
    if (fpsid & BIT(FPSID_SW_BIT)) {
        return false;
    }

    word_t fpsid_subarch;

    if (supportsAsyncExceptions()) {
        /* In the future, when we've targets that support asynchronous FPU exceptions, we've to support them */
        if (config_set(CONFIG_HAVE_FPU)) {
            printf("Error: seL4 doesn't support FPU subarchitectures that support asynchronous exceptions\n");
            return false;
        } else {
            // if we aren't using the fpu then we have detected an fpu that we cannot use, but that is fine
            return true;
        }
    }
    /* Check for subarchitectures we support */
    fpsid_subarch = (fpsid >> FPSID_SUBARCH_SHIFT_POS) & 0x7f;

    switch (fpsid_subarch) {
    /* We only support the following subarch values */
    case 0x2:
    case 0x3:
    case 0x4:
        break;
    default: {
        if (config_set(CONFIG_HAVE_FPU)) {
            printf("Error: seL4 doesn't support this VFP subarchitecture\n");
            return false;
        } else {
            // if we aren't using the fpu then we have detected an fpu that we cannot use, but that is fine
            return true;
        }
    }

    }

    if (!config_set(CONFIG_HAVE_FPU)) {
        printf("Info: Not using supported FPU as FPU is disabled in the build configuration\n");
    }
    return true;
}
