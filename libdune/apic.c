#define _GNU_SOURCE

//#include <linux/mm.h>
//#include <asm/ipi.h>

#define _GNU_SOURCE

#include <malloc.h>
#include <sched.h>

#include "dune.h"
#include "cpu-x86.h"

#define XAPIC_EOI_OFFSET 0xB0
#define APIC_EOI_ACK 0x0

typedef uint8_t u8;
typedef uint32_t u32;

#define NUM_CORES 100
#define POSTED_INTR_VECTOR 0xF2

#define APIC_ID_MSR 0x802
#define APIC_ICR_MSR 0x830
#define APIC_EOI_MSR 0x80B

#define EOI_ACK 0x0

#define APIC_DEST_PHYSICAL 0x00000
#define APIC_DM_FIXED 0x00000
#define APIC_EOI 0xB0
#define APIC_EOI_ACK 0x0
#define APIC_ICR 0x300
#define APIC_ICR2 0x310
#define APIC_ICR_BUSY 0x01000
#define SET_APIC_DEST_FIELD(x) ((x) << 24)

//TODO: Get highest APIC ID in system
static int apic_routing[NUM_CORES];

typedef struct posted_interrupt_desc {
    u32 vectors[8]; /* posted interrupt vectors */
    u32 extra[8]; /* outstanding notification indicator and extra space the VMM can use */
} __attribute__((aligned(64))) posted_interrupt_desc;

//these addresses are loaded via a VM function call in a lazy fashion
static posted_interrupt_desc *posted_interrupt_descriptors[NUM_CORES];

static inline void apic_write(u32 reg, u32 v)
{
	volatile u32 *addr = (volatile u32 *)(APIC_BASE + reg);
	asm volatile("movl %0, %P1" : "=r" (v), "=m" (*addr) : "i" (0), "0" (v), "m" (*addr));
}

static inline uint32_t apic_read(uint32_t reg)
{
	return *((volatile uint32_t *)(APIC_BASE + reg));
}

uint32_t dune_apic_id() {
    uint32_t apic_id = apic_read(0x20);
    apic_id >>= 24;
    apic_id &= 0xF;
    return apic_id;
}

void setup_apic() {
        memset(apic_routing, -1, sizeof(int) * NUM_CORES);
        asm("mfence" ::: "memory");
}

void apic_init_rt_entry() {
        int core_id = sched_getcpu();
        apic_routing[core_id] = dune_apic_id();
        asm("mfence" ::: "memory");
}

uint32_t apic_id_for_cpu(uint32_t cpu, bool *error) {
        if (cpu >= NUM_CORES) {
            if (error) *error = true;
            return 0;
        }
        return apic_routing[cpu];
}

void apic_init_posted_desc_entry() {
	//Implement
}

static inline unsigned int __prepare_ICR(unsigned int shortcut, int vector, unsigned int dest)
{
	return shortcut | dest | APIC_DM_FIXED | vector;
}

static inline int __prepare_ICR2(unsigned int mask)
{
	return SET_APIC_DEST_FIELD(mask);
}

static inline void __xapic_wait_icr_idle(void)
{
	printf("apic read: %x\n", apic_read(APIC_ICR));
	while (apic_read(APIC_ICR) & APIC_ICR_BUSY) {
		asm volatile("pause");
	}
}

static inline void __default_send_IPI_dest_field(unsigned int mask, int vector, unsigned int dest)
{
	__xapic_wait_icr_idle();
	apic_write(APIC_ICR2, __prepare_ICR2(mask));
	apic_write(APIC_ICR, __prepare_ICR(0, vector, dest));
}

static inline void apic_send_ipi(int apicid, int vector)
{
	__default_send_IPI_dest_field(apicid, vector, APIC_DEST_PHYSICAL);
}

void dune_apic_eoi() {
        wrmsrl(APIC_EOI_MSR, EOI_ACK);
}

static inline int
test_and_set_bit(unsigned long nr, volatile void *addr)
{
	unsigned long oldbit;
	unsigned long temp;
	int *m = ((int *) addr) + (nr >> 5);

	__asm__ __volatile__(
#ifdef CONFIG_SMP
	"	mb\n"
#endif
	"1:	ldl_l %0,%4\n"
	"	and %0,%3,%2\n"
	"	bne %2,2f\n"
	"	xor %0,%3,%0\n"
	"	stl_c %0,%1\n"
	"	beq %0,3f\n"
	"2:\n"
#ifdef CONFIG_SMP
	"	mb\n"
#endif
	".subsection 2\n"
	"3:	br 1b\n"
	".previous"
	:"=&r" (temp), "=m" (*m), "=&r" (oldbit)
	:"Ir" (1UL << (nr & 31)), "m" (*m) : "memory");

	return oldbit != 0;
}

void apic_send_posted_ipi(u8 vector, u32 destination_core) {
    /*posted_interrupt_desc *desc;
    desc = posted_interrupt_descriptors[destination_core];
    if (!desc) {
	printf("Error 1\n");
        //the address for the destination core's posted interrupt descriptor
        //is unknown... this is probably because the destination core has not
        //yet called [apic_init_posted_desc_entry]
        return;
    }
 
    //first set the posted-interrupt request
    if (test_and_set_bit(vector, (unsigned long *)desc->vectors)) {
	printf("Error 2\n");
        //bit already set, so the interrupt is already pending (and
        //the outstanding notification bit is 1)
        return;
    }
    
    //set the outstanding notification bit to 1
    if (test_and_set_bit(0, (unsigned long *)desc->extra)) {
	printf("Error 3\n");
        //bit already set, so there is an interrupt(s) already pending
        return;
    }
    */
    //now send the posted interrupt vector to the destination
    bool error = false;
    u32 destination_apic_id = apic_id_for_cpu(destination_core, &error);
    if (error) return;
    printf("APIC BASE: %lx\n", APIC_BASE);
    printf("POSTED_INTR_DESCS_BASE: %lx\n", POSTED_INTR_DESCS_BASE);
    //*(char *)POSTED_INTR_DESCS_BASE = 0;
    apic_send_ipi(destination_apic_id, POSTED_INTR_VECTOR);
    printf("Sent IPI to apic %u\n", destination_apic_id);
}
