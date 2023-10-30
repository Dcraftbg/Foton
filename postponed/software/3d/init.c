/*===============================================================================
 Copyright (C) Andrzej Adamczyk (at https://blackdev.org/). All rights reserved.
===============================================================================*/

uint8_t init( void ) {
	// obtain information about kernel framebuffer
	std_framebuffer( (struct STD_SYSCALL_STRUCTURE_FRAMEBUFFER *) &kernel_framebuffer );

	// allocate data container
	uint8_t data[ STD_IPC_SIZE_byte ];

	// prepeare new window request
	struct STD_IPC_STRUCTURE_WINDOW_REQUEST *request = (struct STD_IPC_STRUCTURE_WINDOW_REQUEST *) &data;
	request -> type = STD_IPC_TYPE_internal;
	request -> request_or_answer = STD_WINDOW_REQUEST_create;
	request -> x = 0;
	request -> y = 0;
	request -> width = D3_WIDTH_pixel;
	request -> height = D3_HEIGHT_pixel;

	// send request
	std_ipc_send( kernel_framebuffer.pid, (uint8_t *) request );

	// wait for answer
	while( ! std_ipc_receive( (uint8_t *) &data ) );

	// we are not allowed to create new windows?
	struct STD_IPC_STRUCTURE_WINDOW_ANSWER_DESCRIPTOR *answer = (struct STD_IPC_STRUCTURE_WINDOW_ANSWER_DESCRIPTOR *) &data;
	if( ! answer -> descriptor ) std_exit();	// nothing to do

	// descriptor properties
	descriptor = (struct STD_WINDOW_STRUCTURE_DESCRIPTOR *) answer -> descriptor;

	// initialize RGL library
	rgl = lib_rgl( descriptor -> width, descriptor -> height - LIB_FONT_HEIGHT_pixel, (uint32_t *) ((uintptr_t) descriptor + sizeof( struct STD_WINDOW_STRUCTURE_DESCRIPTOR )) + (descriptor -> width * LIB_FONT_HEIGHT_pixel) );

	// window content ready for display
	descriptor -> flags |= STD_WINDOW_FLAG_visible | STD_WINDOW_FLAG_flush;

	//----------------------------------------------------------------------

	object_load();

	// execute interface as separate thread
	std_thread( (uintptr_t) &interface, (uint8_t *) "demo-interface", 14 );

	// it's ok
	return FALSE;
}