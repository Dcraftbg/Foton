/*===============================================================================
 Copyright (C) Andrzej Adamczyk (at https://blackdev.org/). All rights reserved.
===============================================================================*/

void kernel_idt_mount( uint8_t id, uint16_t type, uintptr_t address ) {
	// interrupt type
	kernel -> idt_header.base_address[ id ].type = type;

	// address of code descriptor that runs procedure
	kernel -> idt_header.base_address[ id ].gdt_descriptor = sizeof( ( ( struct KERNEL_GDT_STRUCTURE *) 0 ) -> cs_ring0 );

	// address of exception handler
	kernel -> idt_header.base_address[ id ].base_low = (uint16_t) address;
	kernel -> idt_header.base_address[ id ].base_middle = (uint16_t) (address >> 16);
	kernel -> idt_header.base_address[ id ].base_high = (uint32_t) (address >> 32);
}

void kernel_idt_exception( struct KERNEL_IDT_STRUCTURE_EXCEPTION *exception ) {
	// task properties
	struct KERNEL_TASK_STRUCTURE *task = kernel_task_active();

	// show type of exception
	kernel -> log( (uint8_t *) "Exception: " );
	switch( exception -> id ) {
		case 0: { kernel -> log( (uint8_t *) "Divide-by-zero Error" ); break; }
		case 1: { kernel -> log( (uint8_t *) "Debug" ); break; }
		case 3: { kernel -> log( (uint8_t *) "Breakpoint" ); break; }
		case 4: { kernel -> log( (uint8_t *) "Overflow" ); break; }
		case 5: { kernel -> log( (uint8_t *) "Bound Range Exceeded" ); break; }
		case 6: { kernel -> log( (uint8_t *) "Invalid Opcode" ); break; }
		case 7: { kernel -> log( (uint8_t *) "Device Not Available" ); break; }
		case 8: { kernel -> log( (uint8_t *) "Double Fault" ); break; }
		case 9: { kernel -> log( (uint8_t *) "Coprocessor Segment Overrun" ); break; }
		case 10: { kernel -> log( (uint8_t *) "Invalid TSS" ); break; }
		case 11: { kernel -> log( (uint8_t *) "Segment Not Present" ); break; }
		case 12: { kernel -> log( (uint8_t *) "Stack-Segment Fault" ); break; }
		case 13: { kernel -> log( (uint8_t *) "General Protection Fault" ); break; }
		case 14: {
			// exception for process stack space?
			if( MACRO_PAGE_ALIGN_UP( exception -> cr2 ) == KERNEL_TASK_STACK_pointer - (task -> stack << STD_SHIFT_PAGE) ) {
				// describe additional space under process stack
				kernel_page_alloc( (uintptr_t *) task -> cr3, KERNEL_TASK_STACK_pointer - (++task -> stack << STD_SHIFT_PAGE), 1, KERNEL_PAGE_FLAG_present | KERNEL_PAGE_FLAG_write | KERNEL_PAGE_FLAG_user | KERNEL_PAGE_FLAG_process );

				// done
				return;
			}

			// debug
			kernel -> log( (uint8_t *) "Page Fault\nRAX 0x%16X   RBX 0x%16X\nRCX 0x%16X   RDX 0x%16X\nRBP 0x%16X\nR8  0x%16X   R9  0x%16X\nR10 0x%16X   R11 0x%16X\nR12 0x%16X   R13 0x%16X\nR14 0x%16X   R15 0x%16X\n", exception -> rax, exception -> rbx, exception -> rcx, exception -> rdx, exception -> rbp, exception -> r8, exception -> r9, exception -> r10, exception -> r11, exception -> r12, exception -> r13, exception -> r14, exception -> r15 );
			
			// done
			break;
		}
		case 16: { kernel -> log( (uint8_t *) "x87 Floating-Point" ); break; }
		case 17: { kernel -> log( (uint8_t *) "Alignment Check" ); break; }
		case 18: { kernel -> log( (uint8_t *) "Machine Check" ); break; }
		case 19: { kernel -> log( (uint8_t *) "SIMD Floating-Point" ); break; }
		case 20: { kernel -> log( (uint8_t *) "Virtualization" ); break; }
		case 21: { kernel -> log( (uint8_t *) "Control Protection" ); break; }
		case 28: { kernel -> log( (uint8_t *) "Hypervisor Injection" ); break; }
		case 29: { kernel -> log( (uint8_t *) "VMM Communication" ); break; }
		case 30: { kernel -> log( (uint8_t *) "Security" ); break; }
		default: { kernel -> log( (uint8_t *) "{unknown}" ); break; }
	}

	// show task name
	kernel -> log( (uint8_t *) "Task: '%s' near 0x%X)\n", task -> name, exception -> cr2 );

	// hold the door
	while( TRUE ) {}
}

__attribute__ (( preserve_all ))
void kernel_idt_interrupt_default( struct KERNEL_IDT_STRUCTURE_RETURN *interrupt ) {
	// tell APIC of current logical processor that the hardware interrupt is being handled properly
	kernel_lapic_accept();
}
