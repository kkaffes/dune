#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sched.h>

#include "libdune/dune.h"
#include "libdune/cpu-x86.h"

#define NUM_THREADS 1
#define TEST_VECTOR 0xF2

volatile bool t2_ready = false;
volatile bool received_posted_ipi[NUM_THREADS];

static void test_handler(struct dune_tf *tf) {
	printf("posted_ipi: received posted IPI on core %d\n", sched_getcpu());
	received_posted_ipi[sched_getcpu()] = true;
	dune_apic_eoi();
}

void *t_start(void *arg) {
	volatile int ret = dune_enter();
	if (ret) {
		printf("posted_ipi: failed to enter dune in thread 2\n");
		return NULL;
	}
	printf("Thread running on core %d\n", sched_getcpu());
	printf("Thread %d has APIC ID %d\n", sched_getcpu(), dune_apic_id());
	apic_init_rt_entry();
	dune_register_intr_handler(TEST_VECTOR, test_handler);
	asm volatile("mfence" ::: "memory");
	*(volatile bool *)arg = true;
	printf("two: %p\n", (void *)POSTED_INTR_DESCS_BASE);
        printf("hey: %c\n", *(char *)POSTED_INTR_DESCS_BASE);
	while (!received_posted_ipi[sched_getcpu()]);
	return NULL;
}

int main(int argc, char *argv[])
{
	volatile int ret;

	printf("posted_ipi: not running dune yet\n");

	ret = dune_init_and_enter();
	if (ret) {
		printf("failed to initialize dune\n");
		return ret;
	}
	printf("posted_ipi: now printing from dune mode on core %d\n", sched_getcpu());

	setup_apic();
	apic_init_rt_entry();	

	pthread_t pthreads[NUM_THREADS];
	volatile bool ready[NUM_THREADS];
	int i;
	for (i = 0; i < NUM_THREADS; i++) {
        	pthread_attr_t attr;
        	cpu_set_t cpus;
        	pthread_attr_init(&attr);
        	CPU_ZERO(&cpus);
        	CPU_SET(i, &cpus);
        	pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpus);
        	pthread_create(&pthreads[i], &attr, t_start, (void *)&ready[i]);
	}

	for (i = 0; i < NUM_THREADS; i++) {
		while (!ready[i]);
	}
	asm volatile("mfence" ::: "memory");
	
	printf("posted_ipi: about to send posted IPIs to %d cores\n", NUM_THREADS);
	for (i = 0; i < NUM_THREADS; i++) {
		apic_send_posted_ipi(TEST_VECTOR, i);
	}

	for (i = 0; i < NUM_THREADS; i++) {
		pthread_join(pthreads[i], NULL);
	}

	return 0;
}
