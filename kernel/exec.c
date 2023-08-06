/*===============================================================================
 Copyright (C) Andrzej Adamczyk (at https://blackdev.org/). All rights reserved.
===============================================================================*/

int64_t kernel_exec( uint8_t *name, uint64_t length ) {
	// length of exec name
	uint64_t exec_length = lib_string_word( name, length );

	// retrieve file properties
	uint8_t path[ 12 + LIB_VFS_name_limit + 1 ] = "/system/bin/";
	for( uint64_t i = 0; i < exec_length; i++ ) path[ i + 12 ] = name[ i ];

	// retrieve information about file to execute
	struct STD_FILE_STRUCTURE file = kernel_storage_file( kernel -> storage_root_id, path, 12 + exec_length );

	// if file doesn't exist
	if( ! file.id ) return EMPTY;	// end of routine

	// prepare space for workbench
	uintptr_t workbench = EMPTY; 
	if( ! (workbench = kernel_memory_alloc( MACRO_PAGE_ALIGN_UP( file.size_byte ) >> STD_SHIFT_PAGE )) ) return EMPTY;	// no enough memory

	// load file into workbench space
	kernel_storage_read( kernel -> storage_root_id, file.id, workbench );

	// file contains proper ELF header?
	if( ! lib_elf_identify( workbench ) ) return EMPTY;	// no

	// ELF structure properties
	struct LIB_ELF_STRUCTURE *elf = (struct LIB_ELF_STRUCTURE *) workbench;

	// it's an executable file?
	if( elf -> type != LIB_ELF_TYPE_executable ) return EMPTY;	// no

	// load libraries required by file
	kernel_library( elf );

	// create a new job in task queue
	struct KERNEL_TASK_STRUCTURE *exec = kernel_task_add( name, length );

	// prepare Paging table for new process
	exec -> cr3 = kernel_memory_alloc_page() | KERNEL_PAGE_logical;

	// insert into paging, context stack of new process
	kernel_page_alloc( (uintptr_t *) exec -> cr3, KERNEL_STACK_address, 2, KERNEL_PAGE_FLAG_present | KERNEL_PAGE_FLAG_write | KERNEL_PAGE_FLAG_process );

// debug
lib_terminal_printf( &kernel_terminal, (uint8_t *) "|rela\n" );

	// set initial startup configuration for new process
	struct KERNEL_IDT_STRUCTURE_RETURN *context = (struct KERNEL_IDT_STRUCTURE_RETURN *) (kernel_page_address( (uintptr_t *) exec -> cr3, KERNEL_STACK_pointer - STD_PAGE_byte ) + KERNEL_PAGE_logical + (STD_PAGE_byte - sizeof( struct KERNEL_IDT_STRUCTURE_RETURN )));

	// code descriptor
	context -> cs = offsetof( struct KERNEL_GDT_STRUCTURE, cs_ring3 ) | 0x03;

	// basic processor state flags
	context -> eflags = KERNEL_TASK_EFLAGS_default;

	// stack descriptor
	context -> ss = offsetof( struct KERNEL_GDT_STRUCTURE, ds_ring3 ) | 0x03;

	// the context stack top pointer
	exec -> rsp = KERNEL_STACK_pointer - sizeof( struct KERNEL_IDT_STRUCTURE_RETURN );

	// set the process entry address
	context -> rip = elf -> entry_ptr;

	//---------------

	// length of name with arguments properties
	uint64_t args = (length & ~0x0F) + 0x18;
	uint8_t *process_stack = (uint8_t *) kernel_memory_alloc( MACRO_PAGE_ALIGN_UP( args ) >> STD_SHIFT_PAGE );

	// stack pointer of process
	context -> rsp = KERNEL_TASK_STACK_pointer - args;

	// share to process:

	// length of args in Bytes
	uint64_t *arg_length = (uint64_t *) &process_stack[ MACRO_PAGE_ALIGN_UP( args ) - args ]; *arg_length = length;

	// and string itself
	for( uint64_t i = 0; i < length; i++ ) process_stack[ MACRO_PAGE_ALIGN_UP( args ) - args + 0x08 + i ] = name[ i ];

	// map stack space to process paging array
	kernel_page_map( (uintptr_t *) exec -> cr3, (uintptr_t) process_stack, context -> rsp & STD_PAGE_mask, MACRO_PAGE_ALIGN_UP( args ) >> STD_SHIFT_PAGE, KERNEL_PAGE_FLAG_present | KERNEL_PAGE_FLAG_write | KERNEL_PAGE_FLAG_user | KERNEL_PAGE_FLAG_process );

	// process memory usage
	exec -> page += MACRO_PAGE_ALIGN_UP( args ) >> STD_SHIFT_PAGE;

	// process stack size
	exec -> stack += MACRO_PAGE_ALIGN_UP( args ) >> STD_SHIFT_PAGE;

	//---------------

	// calculate space required by configured executable
	uint64_t exec_page = EMPTY;

	// ELF header properties
	struct LIB_ELF_STRUCTURE_HEADER *elf_h = (struct LIB_ELF_STRUCTURE_HEADER *) ((uint64_t) elf + elf -> headers_offset);

	// prepare the memory space for segments used by the process
	for( uint16_t i = 0; i < elf -> h_entry_count; i++ ) {
		// ignore blank entry or not loadable
 		if( ! elf_h[ i ].type || ! elf_h[ i ].memory_size || elf_h[ i ].type != LIB_ELF_HEADER_TYPE_load ) continue;

		// update executable space size?
		if( exec_page < (MACRO_PAGE_ALIGN_UP( elf_h[ i ].virtual_address + elf_h[ i ].segment_size ) - KERNEL_EXEC_base_address) >> STD_SHIFT_PAGE ) exec_page = (MACRO_PAGE_ALIGN_UP( elf_h[ i ].virtual_address + elf_h[ i ].segment_size ) - KERNEL_EXEC_base_address) >> STD_SHIFT_PAGE;
	}

	// allocate calculated space
	uintptr_t exec_base_address = kernel_memory_alloc( exec_page );

	// load executable segments in place
	for( uint16_t i = 0; i < elf -> h_entry_count; i++ ) {
		// ignore blank entry or not loadable
 		if( ! elf_h[ i ].type || ! elf_h[ i ].memory_size || elf_h[ i ].type != LIB_ELF_HEADER_TYPE_load ) continue;

		// segment destination
		uint8_t *destination = (uint8_t *) ((elf_h[ i ].virtual_address - KERNEL_EXEC_base_address) + exec_base_address);

		// segment source
		uint8_t *source = (uint8_t *) ((uintptr_t) elf + elf_h[ i ].segment_offset);

		// copy segment content into place
		for( uint64_t j = 0; j < elf_h[ i ].segment_size; j++ ) destination[ j ] = source[ j ];
	}

	// map executable space to paging array
	kernel_page_map( (uintptr_t *) exec -> cr3, exec_base_address, KERNEL_EXEC_base_address, exec_page, KERNEL_PAGE_FLAG_present | KERNEL_PAGE_FLAG_write | KERNEL_PAGE_FLAG_user | KERNEL_PAGE_FLAG_process );

	// process memory usage
	exec -> page += exec_page;

	// ELF section properties
	struct LIB_ELF_STRUCTURE_SECTION *elf_s = (struct LIB_ELF_STRUCTURE_SECTION *) ((uintptr_t) elf + elf -> sections_offset);

	// connect required functions new locations / from another library
	kernel_exec_link( elf_s, elf -> s_entry_count, exec_base_address );

	// to be continued
	return EMPTY;
}

void kernel_exec_link( struct LIB_ELF_STRUCTURE_SECTION *elf_s, uint64_t elf_s_count, uintptr_t exec_base_address ) {
	// we need to know where is

	// dynamic relocations
	struct LIB_ELF_STRUCTURE_DYNAMIC_RELOCATION *rela = EMPTY;
	uint64_t rela_entry_count = EMPTY;

	// global offset table
	uint64_t *got = EMPTY;

	// string table
	uint8_t *strtab = EMPTY;

	// and at last dynamic symbols
	struct LIB_ELF_STRUCTURE_DYNAMIC_SYMBOL *dynsym = EMPTY;

	// retrieve information about them
	for( uint64_t i = 0; i < elf_s_count; i++ ) {
		// function names?
		if( elf_s[ i ].type == LIB_ELF_SECTION_TYPE_strtab ) if( ! strtab ) strtab = (uint8_t *) (exec_base_address + elf_s[ i ].virtual_address - KERNEL_EXEC_base_address);
		
		// global offset table?
		if( elf_s[ i ].type == LIB_ELF_SECTION_TYPE_progbits ) if( ! got ) got = (uint64_t *) (exec_base_address + elf_s[ i ].virtual_address - KERNEL_EXEC_base_address) + 0x03;	// first 3 entries are reserved
		
		// dynamic relocations?
		if( elf_s[ i ].type == LIB_ELF_SECTION_TYPE_rela ) {
			// get pointer to structure
			rela = (struct LIB_ELF_STRUCTURE_DYNAMIC_RELOCATION *) (exec_base_address + elf_s[ i ].virtual_address - KERNEL_EXEC_base_address);

			// and number of entries
			rela_entry_count = elf_s[ i ].size_byte / sizeof( struct LIB_ELF_STRUCTURE_DYNAMIC_RELOCATION );
		}
		
		// dynamic symbols?
		if( elf_s[ i ].type == LIB_ELF_SECTION_TYPE_dynsym ) dynsym = (struct LIB_ELF_STRUCTURE_DYNAMIC_SYMBOL *) (exec_base_address + elf_s[ i ].virtual_address - KERNEL_EXEC_base_address);
	}

	// external libraries are not required?
	if( ! rela ) return;	// yes

	// for each entry in dynamic symbols
	for( uint64_t i = 0; i < rela_entry_count; i++ ) {
		// it's a local function?
		if( dynsym[ rela[ i ].index ].address ) {
			// update address of local function
			got[ i ] = dynsym[ rela[ i ].index ].address + exec_base_address;

// debug
lib_terminal_printf( &kernel_terminal, (uint8_t *) "  [changed address of local function '%s' to %X]\n", &strtab[ dynsym[ rela[ i ].index ].name_offset ], got[ i ] );
		} else {
			// retrieve library function address
			got[ i ] = kernel_library_function( (uint8_t *) &strtab[ dynsym[ rela[ i ].index ].name_offset ], lib_string_length( (uint8_t *) &strtab[ dynsym[ rela[ i ].index ].name_offset ] ) );

// debug
lib_terminal_printf( &kernel_terminal, (uint8_t *) "   [acquired function address of '%s' as 0x%X]\n", &strtab[ dynsym[ rela[ i ].index ].name_offset ], got[ i ] );
		}
	}
}