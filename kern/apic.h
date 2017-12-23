static struct apic_map {
	unsigned core;
	uint32_t local_apic_id;
} apic_map[NR_CPUS];

