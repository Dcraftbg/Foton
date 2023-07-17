/*===============================================================================
 Copyright (C) Andrzej Adamczyk (at https://blackdev.org/). All rights reserved.
===============================================================================*/

// terminal properties
struct LIB_TERMINAL_STRUCTURE kinit_terminal;

// our limine requests

static volatile struct limine_framebuffer_request limine_framebuffer_request = {
	.id = LIMINE_FRAMEBUFFER_REQUEST,
	.revision = 0
};

static volatile struct limine_memmap_request limine_memmap_request = {
	.id = LIMINE_MEMMAP_REQUEST,
	.revision = 0
};

static volatile struct limine_rsdp_request limine_rsdp_request = {
	.id = LIMINE_RSDP_REQUEST,
	.revision = 0
};