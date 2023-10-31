/*===============================================================================
 Copyright (C) Andrzej Adamczyk (at https://blackdev.org/). All rights reserved.
===============================================================================*/

int64_t	gui_pid = EMPTY;
int64_t wm_pid = EMPTY;

uint16_t gui_wallpaper_width	= EMPTY;
uint16_t gui_wallpaper_height	= EMPTY;

struct STD_WINDOW_STRUCTURE_DESCRIPTOR *gui_window_wallpaper	= EMPTY;
struct STD_WINDOW_STRUCTURE_DESCRIPTOR *gui_window_cursor	= EMPTY;
struct STD_WINDOW_STRUCTURE_DESCRIPTOR *gui_window_taskbar	= EMPTY;

MACRO_IMPORT_FILE_AS_ARRAY( wallpaper, "./root/system/var/wallpaper.tga" );
MACRO_IMPORT_FILE_AS_ARRAY( cursor, "./root/system/var/cursor.tga" );