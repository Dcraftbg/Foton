/*===============================================================================
 Copyright (C) Andrzej Adamczyk (at https://blackdev.org/). All rights reserved.
===============================================================================*/

void wm_fill( void ) {
	// fill every zone with assigned object
	for( uint64_t i = 0; i < wm_zone_limit; i++ ) {
		// object assigned to zone?
		if( ! wm_zone_base_address[ i ].object ) continue;	// no

		// fill zone with selected object
		uint32_t *source = (uint32_t *) ((uintptr_t) wm_zone_base_address[ i ].object -> descriptor + sizeof( struct WM_STRUCTURE_DESCRIPTOR ));
		uint32_t *target = (uint32_t *) ((uintptr_t) wm_object_workbench -> descriptor + sizeof( struct WM_STRUCTURE_DESCRIPTOR ));
		for( uint64_t y = wm_zone_base_address[ i ].y; y < wm_zone_base_address[ i ].height + wm_zone_base_address[ i ].y; y++ )
			for( uint64_t x = wm_zone_base_address[ i ].x; x < wm_zone_base_address[ i ].width + wm_zone_base_address[ i ].x; x++ ) {
				// color properties
				uint32_t color_current = target[ (y * wm_object_workbench -> width) + x ];
				uint32_t color_new = source[ (x - wm_zone_base_address[ i ].object -> x) + (wm_zone_base_address[ i ].object -> width * (y - wm_zone_base_address[ i ].object -> y)) ];

				// perform the operation based on the alpha channel
				switch( color_new >> 24 ) {
					// no alpha channel
					case (uint8_t) 0xFF: {
						target[ (y * wm_object_workbench -> width) + x ] = color_new; continue;
					}

					// transparent color
					case 0x00: { continue; }
				}

				// calculate the color based on the alpha channel
				target[ (y * wm_object_workbench -> width) + x ] = lib_color_blend( color_current, color_new );
			}

		// synchronize workbench with framebuffer
		wm_framebuffer_semaphore = TRUE;
	}

	// all zones filled
	wm_zone_limit = EMPTY;
}