/* These functions are used to send and receive posted IPIs with an x2APIC.
*/

#define _GNU_SOURCE

#include <linux/mm.h>
#include <asm/ipi.h>

#include "dune.h"

#define XAPIC_EOI_OFFSET 0xB0
#define APIC_EOI_ACK 0x0

//TODO: Get highest APIC ID in system
static int apic_routing[100];
static int num_rt_entries = 100;

u32 dune_apic_id() {
        long long apic_id;
        rdmsrl(APIC_ID_MSR, apic_id);
        return (u32)apic_id;
}

void setup_apic() {
        memset(apic_routing, -1, sizeof(int) * num_rt_entries);
        asm("mfence" ::: "memory");
}

void apic_init_rt_entry() {
        int core_id = sched_getcpu();
        apic_routing[dune_apic_id()] = core_id;
        asm("mfence" ::: "memory");
}

u32 cpu_id_for_apic(u32 apic) {
        if (apic >= num_rt_entries) return -1;
        return apic_routing[apic];
}

/* apic_write_x
 * Writes to the xAPIC's memory-mapped registrers.
 *
 * [reg] is the offset to write to within the memory region reserved
 * by the xAPIC.
 * [v] is the value to write.
 */
static inline void apic_write_x(u32 reg, u32 v)
{
	volatile u32 *addr = (volatile u32 *)(APIC_BASE + reg);

	asm volatile("movl %0, %P1" : "=r" (v), "=m" (*addr) : "i" (0), "0" (v), "m" (*addr));
}

/* apic_send_ipi_x2
 * Send an IPI to another local APIC. This function only supports x2APIC, not xAPIC.
 *
 * [vector] is the vector of the interrupt to send.
 * [destination_apic_id] is the ID of the local APIC that will receive the IPI.
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

/* apic_send_ipi_x
 * Send an IPI to another local APIC. This function only supports xAPIC, not x2APIC.
 *
 * [vector] is the vector of the interrupt to send.
 * [destination_apic_id] is the ID of the local APIC that will receive the IPI.
 */
static void apic_send_ipi_x(u8 vector, u8 destination_apic_id) {
	__default_send_IPI_dest_field(destination_apic_id, vector, APIC_DEST_PHYSICAL);
}

/* apic_send_ipi
 * Send an IPI to another local APIC. Determines whether the computer is equipped
 * with xAPICs or x2APICs and chooses the correct delivery method.
 *
 * [vector] is the vector of the interrupt to send.
 * [destination_apic_id] is the ID of the local APIC that will receive the IPI.
 */
void apic_send_ipi(u8 vector, u32 destination_apic_id) {
	if (x2apic_enabled()) {
		apic_send_ipi_x2(vector, destination_apic_id);
	} else {
		apic_send_ipi_x(vector, (u8)destination_apic_id);
	}
}

/* apic_write_eoi
 * Acknowledges receipt of an interrupt to the local APIC by writing an acknowledgment to
 * the local APIC's EOI register. Determines whether the computer is equipped with xAPICs 
 * or x2APICs and writes the acknowledgment accordingly.
 *
 * [vector] is the vector of the interrupt to send.
 * [destination_apic_id] is the ID of the local APIC that will receive the IPI.
 */
void apic_write_eoi(void) {
	if (x2apic_enabled()) {
		wrmsrl(APIC_BASE_MSR + (APIC_EOI >> 4), APIC_EOI_ACK);
	} else {
		apic_write_x(XAPIC_EOI_OFFSET, APIC_EOI_ACK);
	}
}
