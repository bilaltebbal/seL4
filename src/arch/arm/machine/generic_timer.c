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


#include <plat/machine/timer.h>
#include <arch/machine/generic_timer.h>

BOOT_CODE void initGenericTimer(void)
{
    if (config_set(CONFIG_DEBUG_BUILD)) {
        /* check the frequency is correct */
        uint32_t gpt_cnt_tval = 0;
        SYSTEM_READ_WORD(CNTFRQ, gpt_cnt_tval);

#ifdef ENABLE_SMP_SUPPORT
        if (gpt_cnt_tval != 0 && gpt_cnt_tval != TIMER_CLOCK_HZ && getCurrentCPUIndex() == 0) {
            printf("Warning:  gpt_cnt_tval %u, expected %u\n", gpt_cnt_tval,
                   (uint32_t) TIMER_CLOCK_HZ);
        }
#else
        if (gpt_cnt_tval != 0 && gpt_cnt_tval != TIMER_CLOCK_HZ) {
            printf("Warning:  gpt_cnt_tval %u, expected %u\n", gpt_cnt_tval,
                   (uint32_t) TIMER_CLOCK_HZ);
        }
#endif
    }

    resetTimer();
}
