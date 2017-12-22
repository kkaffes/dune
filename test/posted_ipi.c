#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sched.h>

#include "libdune/dune.h"
#include "libdune/cpu-x86.h"

#define THREAD_2_CORE 10
#define THREAD_2_LOCAL_APIC_ID 40
#define TEST_VECTOR 0xf2

volatile bool t2_ready = false;
bool received_posted_ipi = false;

static void test_handler(struct dune_tf *tf)
{
	printf("posted_ipi: received posted IPI!\n");
	received_posted_ipi = true;
	//TODO: Send EOI
	dune_apic_eoi();
}

void *t2_start(void *arg) {
	//TODO: Set IF to 1
	volatile int ret = dune_enter();
	if (ret) {
		printf("posted_ipi: failed to enter dune in thread 2\n");
		return NULL;
	}
        
        printf("APID ID (thread 2): %lu\n", dune_apic_id());	
	
	dune_register_intr_handler(TEST_VECTOR, test_handler);
	//TODO: Is this memory fence necessary?
	asm volatile("mfence" ::: "memory");
	t2_ready = true;
	while (!received_posted_ipi);
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

	printf("posted_ipi: now printing from dune mode\n");

	pthread_t t2;
        pthread_attr_t attr;
        cpu_set_t cpus;
        pthread_attr_init(&attr);
        CPU_ZERO(&cpus);
        CPU_SET(THREAD_2_CORE, &cpus);
        pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpus);
        pthread_create(&t2, &attr, t2_start, NULL);

	while (!t2_ready);
	//TODO: Is this memory fence necessary?
	asm volatile("mfence" ::: "memory");

        printf("APID ID: %lu\n", dune_apic_id());

	//TODO: Send posted IPI
	dune_apic_send_ipi(TEST_VECTOR, 40);

	pthread_join(t2, NULL);

	return 0;
}

