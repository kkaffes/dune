static struct apic_map {
	unsigned core;
	uint32_t local_apic_id;
} apic_map[NR_CPUS];

void apic_send_ipi(uint8_t vector, uint32_t destination_apic_id);
