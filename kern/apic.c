/* These functions are used to send and receive posted IPIs with an x2APIC.
*/

#define _GNU_SOURCE

#include <linux/mm.h>
#include <asm/ipi.h>

#include "dune.h"

/* apic_send_ipi
 * Sends a posted IPI to an x2APIC.
 *
 * [vector] is the IPI vector number.
 * [destination_apic_id] is the ID of the local APIC for the core that
 * should receive the IPI. You can look up the local APIC ID for each core
 * by printing /proc/cpuinfo to the console (e.g. cat /proc/cpuinfo).
 *
 * See Section 10.6.1 in the Intel manual for details about the
 * implementation of this function. Note that this function only supports
 * x2APIC, not xAPIC.
 */
void apic_send_ipi(u8 vector, u32 destination_apic_id) {
	u32 low;
	low = __prepare_ICR(0, vector, APIC_DEST_PHYSICAL);
	printk(KERN_INFO "ICR value %x\n", low);
	//native_x2apic_icr_write(low, destination_apic_id);

	x2apic_wrmsr_fence();
	wrmsrl(APIC_BASE_MSR + (APIC_ICR >> 4), ((__u64) destination_apic_id) << 32 | low);
	printk(KERN_INFO "DONE\n");
}
