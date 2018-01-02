/* These functions are used to send and receive posted IPIs with an x2APIC.
*/

#define _GNU_SOURCE

#include <linux/mm.h>
#include <asm/ipi.h>

#include "dune.h"

/* apic_send_ipi_x2
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
static void apic_send_ipi_x2(u8 vector, u32 destination_apic_id) {
	u32 low;
	low = __prepare_ICR(0, vector, APIC_DEST_PHYSICAL);
	printk(KERN_INFO "ICR value %x\n", low);
	//native_x2apic_icr_write(low, destination_apic_id);

	x2apic_wrmsr_fence();
	wrmsrl(APIC_BASE_MSR + (APIC_ICR >> 4), ((__u64) destination_apic_id) << 32 | low);
	printk(KERN_INFO "DONE\n");
}

static void apic_send_ipi_x(u8 vector, u8 destination_apic_id) {
	__default_send_IPI_dest_field(destination_apic_id, vector, APIC_DEST_PHYSICAL);
}

void apic_send_ipi(u8 vector, u32 destination_apic_id) {
	if (x2apic_enabled()) {
		apic_send_ipi_x2(vector, destination_apic_id);
	} else {
		apic_send_ipi_x(vector, (u8)destination_apic_id);
	}
}

void apic_write_eoi(void) {
	if (x2apic_enabled()) {
		wrmsrl(APIC_BASE_MSR + (APIC_EOI >> 4), APIC_EOI_ACK);
	} else {
		native_apic_msr_eoi_write(0, 0);
		//apic_write(APIC_EOI, APIC_EOI_ACK);
	}
}
