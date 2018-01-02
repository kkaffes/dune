/* These functions are used to send and receive posted IPIs with an x2APIC.
*/

#define _GNU_SOURCE

#include <malloc.h>
#include <sched.h>

#include "dune.h"
#include "cpu-x86.h"

//the value to write to the EOI register when an interrupt handler has finished
#define EOI_ACK 0x0

//TODO: Get total number of CPUs in system
static int apic_routing[100];
static int num_rt_entries = 100;

uint32_t dune_apic_id() {
	long long apic_id;
	rdmsrl(0x802, apic_id);
	return (uint32_t)apic_id;
}

void setup_apic() {
	memset(apic_routing, -1, sizeof(int) * num_rt_entries);
	asm("mfence" ::: "memory");
}

void apic_init_rt_entry() {
	int core_id = sched_getcpu();
	apic_routing[core_id] = dune_apic_id();
	asm("mfence" ::: "memory");
}

uint32_t apic_id_for_cpu(uint32_t cpu) {
	if (cpu >= num_rt_entries) return -1;
	return apic_routing[cpu];
}

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
void dune_apic_send_ipi(uint8_t vector, uint32_t destination_apic_id) {
	uint64_t to_write = 0x0;
	//write the vector number to the least significant 8 bits
	to_write |= vector;
	//write the destination to the most significant 32 bits
	to_write |= ((uint64_t) destination_apic_id) << 32;

	//[to_write] is initialized to 0, so the remaining bits are all 0
	printf("Destination %u, ICR value %lx\n", destination_apic_id, to_write);
	//now write [to_write] to the MSR for the ICR
	wrmsrl(0x830, to_write);
	printf("DONE\n");
}

/* apic_eoi
 * Writes to the End-of-Interrupt register for the local APIC for the core
 * that executes this function to indicate that the interrupt handler has
 * finished.
*/
void dune_apic_eoi() {
	//TODO: 32 or 64 bits? Does it matter?
	wrmsrl(MSR_X2APIC_EOI, EOI_ACK);
}
