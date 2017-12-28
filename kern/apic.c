/* These functions are used to send and receive posted IPIs with an x2APIC.
*/

#define _GNU_SOURCE

#include <linux/mm.h>

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
void apic_send_ipi(uint8_t vector, uint32_t destination_apic_id) {
	uint64_t to_write = 0x0;
	//write the vector number to the least significant 8 bits
	to_write |= vector;
	//set level to ASSERT
	to_write |= 1 << 14;
	//write the destination to the most significant 32 bits
	to_write |= ((uint64_t) destination_apic_id) << 32;

	//[to_write] is initialized to 0, so the remaining bits are all 0
	//printf("ICR value %lx\n", to_write);
	//now write [to_write] to the MSR for the ICR
	wrmsrl(0x830, to_write);
	//printf("DONE\n");
}
