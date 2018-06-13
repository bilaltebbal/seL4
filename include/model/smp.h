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
 * This software was released under DARPA, public release number XXXXXXXX (to be updated once approved)
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


#ifndef __MODEL_SMP_H_
#define __MODEL_SMP_H_

#include <config.h>
#include <arch/types.h>
#include <arch/model/statedata.h>
#include <model/statedata.h>
#include <mode/machine/hardware.h>

#ifdef ENABLE_SMP_SUPPORT

/* Use this to avoid false sharing between cores for per-core data structures */
#define LOCAL_LINE_SIZE BIT(L1_CACHE_LINE_SIZE_BITS)
#define PAD_TO_NEXT_CACHE_LN(used) char padding[LOCAL_LINE_SIZE - ((used) % LOCAL_LINE_SIZE)]


typedef struct smpStatedata {
    archNodeState_t cpu;
    nodeState_t system;
    PAD_TO_NEXT_CACHE_LN(sizeof(archNodeState_t) + sizeof(nodeState_t));
} smpStatedata_t;

extern smpStatedata_t ksSMP[CONFIG_MAX_NUM_NODES];

void migrateTCB(tcb_t *tcb, word_t new_core);

#endif /* ENABLE_SMP_SUPPORT */

#endif /* __MODEL_SMP_H_ */
