/*===============================================================================
 Copyright (C) Andrzej Adamczyk (at https://blackdev.org/). All rights reserved.
===============================================================================*/

uintptr_t kernel_page_address( uintptr_t *pml4, uintptr_t address ) {
	// locate page table pointers
	uintptr_t *pml3 = (uintptr_t *) (MACRO_PAGE_ALIGN_DOWN( pml4[ (address & ~KERNEL_PAGE_PML5_mask) / (KERNEL_PAGE_PML3_byte)] ) | KERNEL_PAGE_logical);
	uintptr_t *pml2 = (uintptr_t *) (MACRO_PAGE_ALIGN_DOWN( pml3[ ((address & ~KERNEL_PAGE_PML5_mask) % KERNEL_PAGE_PML3_byte) / KERNEL_PAGE_PML2_byte ] ) | KERNEL_PAGE_logical);
	uintptr_t *pml1 = (uintptr_t *) (MACRO_PAGE_ALIGN_DOWN( pml2[ (((address & ~KERNEL_PAGE_PML5_mask) % KERNEL_PAGE_PML3_byte) % KERNEL_PAGE_PML2_byte) / KERNEL_PAGE_PML1_byte ] ) | KERNEL_PAGE_logical);

	// return address of physical page connected to area
	return MACRO_PAGE_ALIGN_DOWN( pml1[ ((((address & ~KERNEL_PAGE_PML5_mask) % KERNEL_PAGE_PML3_byte) % KERNEL_PAGE_PML2_byte) % KERNEL_PAGE_PML1_byte) / STD_PAGE_byte ] );
}

uint8_t kernel_page_alloc( uint64_t *pml4, uint64_t address, uint64_t pages, uint16_t flags ) {
	// start with following array[ entries ]
	uint16_t p4 = (address & ~KERNEL_PAGE_PML5_mask) / KERNEL_PAGE_PML3_byte;
	uint16_t p3 = ((address & ~KERNEL_PAGE_PML5_mask) % KERNEL_PAGE_PML3_byte) / KERNEL_PAGE_PML2_byte;
	uint16_t p2 = (((address & ~KERNEL_PAGE_PML5_mask) % KERNEL_PAGE_PML3_byte) % KERNEL_PAGE_PML2_byte) / KERNEL_PAGE_PML1_byte;
	uint16_t p1 = ((((address & ~KERNEL_PAGE_PML5_mask) % KERNEL_PAGE_PML3_byte) % KERNEL_PAGE_PML2_byte) % KERNEL_PAGE_PML1_byte) / STD_PAGE_byte;

	// start with an entry representing given address in PML4 array
	for( ; p4 < 512; p4++ ) {
		// get PML3 array address (remove flags)
		uint64_t *pml3 = (uint64_t *) (MACRO_PAGE_ALIGN_DOWN( pml4[ p4 ] ) | KERNEL_PAGE_logical);

		// PML3 array doesn't exist?
		if( ! pml4[ p4 ] ) {
			// register new PML3 array inside PML4 entry
			pml4[ p4 ] = kernel_memory_alloc_page();

			// if failed to allocate page for PML3 array
			if( ! pml4[ p4 ] ) return FALSE;

			// properties of PML3 array
			pml3 = (uintptr_t *) (pml4[ p4 ] | KERNEL_PAGE_logical);

			// set flags for PML4 entry
			pml4[ p4 ] |= flags;
		}

		// start with an entry representing given address in PML3 array
		for( ; p3 < 512; p3++ ) {
			// get PML2 array address (remove flags)
			uint64_t *pml2 = (uint64_t *) (MACRO_PAGE_ALIGN_DOWN( pml3[ p3 ] ) | KERNEL_PAGE_logical);
	
			// PML2 array doesn't exist?
			if( ! pml3[ p3 ] ) {
				// register new PML2 array inside PML3 entry
				pml3[ p3 ] = kernel_memory_alloc_page();

				// if failed to allocate page for PML2 array
				if( ! pml3[ p3 ] ) return FALSE;

				// properties of PML2 array
				pml2 = (uintptr_t *) (pml3[ p3 ] | KERNEL_PAGE_logical);

				// set flags for PML3 entry
				pml3[ p3 ] |= flags;
			}

			// start with an entry representing given address in PML2 array
			for( ; p2 < 512; p2++ ) {
				// get PML1 array address (remove flags)
				uint64_t *pml1 = (uint64_t *) (MACRO_PAGE_ALIGN_DOWN( pml2[ p2 ] ) | KERNEL_PAGE_logical);
	
				// PML1 array doesn't exist?
				if( ! pml2[ p2 ] ) {
					// register new PML1 array inside PML2 entry
					pml2[ p2 ] = kernel_memory_alloc_page();

					// if failed to allocate page for PML1 array
					if( ! pml2[ p2 ] ) return FALSE;

					// properties of PML1 array
					pml1 = (uintptr_t *) (pml2[ p2 ] | KERNEL_PAGE_logical);

					// set flags for PML2 entry
					pml2[ p2 ] |= flags;
				}

				// start with an entry representing given address in PML1 array
				for( ; p1 < 512; p1++ ) {
					// empty entry?
					if( ! pml1[ p1 ] ) {
						// yes, allocate page to a logical address
						pml1[ p1 ] = kernel_memory_alloc_page();

						// if failed to allocate page for logical address
						if( ! pml1[ p1 ] ) return FALSE;

						// set flags for PML1 entry
						pml1[ p1 ] |= flags;
					}

					// if all area is described
					if( ! --pages ) return TRUE;
				}

				// first entry of PML1 array
				p1 = 0;
			}

			// first entry of PML2 array
			p2 = 0;
		}

		// first entry of PML3 array
		p3 = 0;
	}

	// invalid area address
	return FALSE;
}

void kernel_page_clean( uintptr_t address, uint64_t n ) {
	// properties of address
	uint64_t *page = (uint64_t *) address;

	// clear every 64 bit value inside N pages
	for( uint64_t i = 0; i < n << STD_SHIFT_512; i++ ) page[ i ] = EMPTY;
}

uint8_t kernel_page_map( uint64_t *pml4, uintptr_t source, uintptr_t target, uint64_t N, uint16_t flags ) {
	// convert source address to canonical
	source &= ~KERNEL_PAGE_logical;

	// start with following table[record]
	uint16_t p4 = (target & ~KERNEL_PAGE_PML5_mask) / KERNEL_PAGE_PML3_byte;
	uint16_t p3 = ((target & ~KERNEL_PAGE_PML5_mask) % KERNEL_PAGE_PML3_byte) / KERNEL_PAGE_PML2_byte;
	uint16_t p2 = (((target & ~KERNEL_PAGE_PML5_mask) % KERNEL_PAGE_PML3_byte) % KERNEL_PAGE_PML2_byte) / KERNEL_PAGE_PML1_byte;
	uint16_t p1 = ((((target & ~KERNEL_PAGE_PML5_mask) % KERNEL_PAGE_PML3_byte) % KERNEL_PAGE_PML2_byte) % KERNEL_PAGE_PML1_byte) / STD_PAGE_byte;

	// start with an entry representing given address in PML4 array
	for( ; p4 < 512; p4++ ) {
		// get PML3 array address (remove flags)
		uint64_t *pml3 = (uint64_t *) (MACRO_PAGE_ALIGN_DOWN( pml4[ p4 ] ) | KERNEL_PAGE_logical);

		// PML3 array doesn't exist?
		if( ! pml4[ p4 ] ) {
			// register new PML3 array inside PML4 entry
			pml4[ p4 ] = kernel_memory_alloc_page();

			// if failed to allocate page for PML3 array
			if( ! pml4[ p4 ] ) return FALSE;

			// properties of PML3 array
			pml3 = (uintptr_t *) (pml4[ p4 ] | KERNEL_PAGE_logical);

			// set flags for PML4 entry
			pml4[ p4 ] |= flags | KERNEL_PAGE_FLAG_write;
		}

		// start with an entry representing given address in PML3 array
		for( ; p3 < 512; p3++ ) {
			// get PML2 array address (remove flags)
			uint64_t *pml2 = (uint64_t *) (MACRO_PAGE_ALIGN_DOWN( pml3[ p3 ] ) | KERNEL_PAGE_logical);
	
			// PML2 array doesn't exist?
			if( ! pml3[ p3 ] ) {
				// register new PML2 array inside PML3 entry
				pml3[ p3 ] = kernel_memory_alloc_page();

				// if failed to allocate page for PML2 array
				if( ! pml3[ p3 ] ) return FALSE;

				// properties of PML2 array
				pml2 = (uintptr_t *) (pml3[ p3 ] | KERNEL_PAGE_logical);

				// set flags for PML3 entry
				pml3[ p3 ] |= flags | KERNEL_PAGE_FLAG_write;
			}

			// start with an entry representing given address in PML2 array
			for( ; p2 < 512; p2++ ) {
				// get PML1 array address (remove flags)
				uint64_t *pml1 = (uint64_t *) (MACRO_PAGE_ALIGN_DOWN( pml2[ p2 ] ) | KERNEL_PAGE_logical);
	
				// PML1 array doesn't exist?
				if( ! pml2[ p2 ] ) {
					// register new PML1 array inside PML2 entry
					pml2[ p2 ] = kernel_memory_alloc_page();

					// if failed to allocate page for PML1 array
					if( ! pml2[ p2 ] ) return FALSE;

					// properties of PML1 array
					pml1 = (uintptr_t *) (pml2[ p2 ] | KERNEL_PAGE_logical);

					// set flags for PML2 entry
					pml2[ p2 ] |= flags | KERNEL_PAGE_FLAG_write;
				}

				// start with an entry representing given address in PML1 array
				for( ; p1 < 512; p1++ ) {
					// copy page from source to target with flags
					pml1[ p1 ] = source | flags;

					// next page address from described physical memory space
					source += STD_PAGE_byte;

					// if all space is described
					if( ! --N ) return TRUE;
				}

				// first record of PML1 array
				p1 = 0;
			}

			// first record of PML2 array
			p2 = 0;
		}

		// first record of PML3 array
		p3 = 0;
	}

	// invalid address space mapping
	return FALSE;
}

void kernel_page_merge( uint64_t *pml4_kernel, uint64_t *pml4_process ) {
	// start with PML4 level of both arrays
	for( uint16_t p4 = 0; p4 < 512; p4++ ) {
		// source entry exists?
		if( pml4_kernel[ p4 ] ) {
			// no target entry?
			if( ! pml4_process[ p4 ] ) {
				// reload space from source array to destination array
				pml4_process[ p4 ] = pml4_kernel[ p4 ];

				// next entry from source array
				continue;
			}

			// PML3
			uint64_t *pml3_kernel = (uint64_t *) (MACRO_PAGE_ALIGN_DOWN( pml4_kernel[ p4 ] ) | KERNEL_PAGE_logical);
			uint64_t *pml3_process = (uint64_t *) (MACRO_PAGE_ALIGN_DOWN( pml4_process[ p4 ] ) | KERNEL_PAGE_logical);
			for( uint16_t p3 = 0; p3 < 512; p3++ ) {
				// source entry exists?
				if( pml3_kernel[ p3 ] ) {
					// no target entry?
					if( ! pml3_process[ p3 ] ) {
						// reload space from source array to destination array
						pml3_process[ p3 ] = pml3_kernel[ p3 ];

						// next entry from source array
						continue;
					}

					// PML2
					uint64_t *pml2_kernel = (uint64_t *) (MACRO_PAGE_ALIGN_DOWN( pml3_kernel[ p3 ] ) | KERNEL_PAGE_logical);
					uint64_t *pml2_process = (uint64_t *) (MACRO_PAGE_ALIGN_DOWN( pml3_process[ p3 ] ) | KERNEL_PAGE_logical);
					for( uint16_t p2 = 0; p2 < 512; p2++ ) {
						// source entry exists?
						if( pml2_kernel[ p2 ] ) {
							// no target entry?
							if( ! pml2_process[ p2 ] ) {
								// reload space from source array to destination array
								pml2_process[ p2 ] = pml2_kernel[ p2 ];

								// next entry from source array
								continue;
							}

							// PML1
							uint64_t *pml1_kernel = (uint64_t *) (MACRO_PAGE_ALIGN_DOWN( pml2_kernel[ p2 ] ) | KERNEL_PAGE_logical);
							uint64_t *pml1_process = (uint64_t *) (MACRO_PAGE_ALIGN_DOWN( pml2_process[ p2 ] ) | KERNEL_PAGE_logical);
							for( uint16_t p1 = 0; p1 < 512; p1++ ) {
								// no target entry?
								if( ! pml1_process[ p1 ] ) {
									// reload space from source array to destination array
									pml1_process[ p1 ] = pml1_kernel[ p1 ];
								}
							}
						}
					}
				}
			}
		}
	}
}

uintptr_t kernel_page_remove( uintptr_t *pml4, uintptr_t address ) {
	// locate page table pointers
	uintptr_t *pml3 = (uintptr_t *) (MACRO_PAGE_ALIGN_DOWN( pml4[ (address & ~KERNEL_PAGE_PML5_mask) / (KERNEL_PAGE_PML3_byte)] ) | KERNEL_PAGE_logical);
	uintptr_t *pml2 = (uintptr_t *) (MACRO_PAGE_ALIGN_DOWN( pml3[ ((address & ~KERNEL_PAGE_PML5_mask) % KERNEL_PAGE_PML3_byte) / KERNEL_PAGE_PML2_byte ] ) | KERNEL_PAGE_logical);
	uintptr_t *pml1 = (uintptr_t *) (MACRO_PAGE_ALIGN_DOWN( pml2[ (((address & ~KERNEL_PAGE_PML5_mask) % KERNEL_PAGE_PML3_byte) % KERNEL_PAGE_PML2_byte) / KERNEL_PAGE_PML1_byte ] ) | KERNEL_PAGE_logical);

	// which page will be removed?
	uintptr_t removed = pml1[ ((((address & ~KERNEL_PAGE_PML5_mask) % KERNEL_PAGE_PML3_byte) % KERNEL_PAGE_PML2_byte) % KERNEL_PAGE_PML1_byte) / STD_PAGE_byte ];

	// is it shared?
	if( removed & KERNEL_PAGE_FLAG_shared ) return EMPTY;	// do not allow

	// remove page from paging
	pml1[ ((((address & ~KERNEL_PAGE_PML5_mask) % KERNEL_PAGE_PML3_byte) % KERNEL_PAGE_PML2_byte) % KERNEL_PAGE_PML1_byte) / STD_PAGE_byte ] = EMPTY;

	// return removed page address
	return MACRO_PAGE_ALIGN_DOWN( removed );
}