/*===============================================================================
 Copyright (C) Andrzej Adamczyk (at https://blackdev.org/). All rights reserved.
===============================================================================*/

void kernel_vfs_file_close( struct KERNEL_VFS_STRUCTURE *socket ) {
	// can we close file?
	if( socket -> pid != kernel_task_pid() ) return;	// no! TODO: something nasty

	// release socket
	if( socket -> lock ) socket -> lock--;
}

struct KERNEL_VFS_STRUCTURE *kernel_vfs_file_open( uint8_t *path, uint64_t length ) {
	// properties of found file
	struct LIB_VFS_STRUCTURE *vfs;
	if( ! (vfs = kernel_vfs_path( path, length, FALSE )) ) return EMPTY;	// file not found

	// open socket
	struct KERNEL_VFS_STRUCTURE *socket = (struct KERNEL_VFS_STRUCTURE *) &kernel -> vfs_base_address[ kernel_vfs_socket_add( (uint64_t) vfs ) ];

	// file located on definied storage
	socket -> storage = kernel -> storage_root;

	// file identificator
	socket -> knot = (uint64_t) vfs;

	// socket opened by process with ID
	socket -> pid = kernel -> task_pid();

	// file found
	return socket;
}

void kernel_vfs_file_properties( struct KERNEL_VFS_STRUCTURE *socket, struct KERNEL_VFS_STRUCTURE_PROPERTIES *properties ) {
	// properties of file
	struct LIB_VFS_STRUCTURE *file = (struct LIB_VFS_STRUCTURE *) socket -> knot;

	// retrun file size in Bytes
	properties -> byte = file -> byte;

	// return file name
	properties -> name_length = file -> name_length;
	for( uint64_t i = 0; i < file -> name_length; i++ ) properties -> name[ i ] = file -> name[ i ];
}

void kernel_vfs_file_read( struct KERNEL_VFS_STRUCTURE *socket, uint8_t *target, uint64_t seek, uint64_t byte ) {
	// properties of file
	struct LIB_VFS_STRUCTURE *file = (struct LIB_VFS_STRUCTURE *) socket -> knot;

	// invalid read request?
	if( seek + byte > file -> byte ) return;	// yes, ignore

	// copy content of file to destination
	uint8_t *source = (uint8_t *) file -> offset + seek;
	for( uint64_t i = 0; i < byte; i++ ) target[ i ] = source[ i ];
}

struct KERNEL_VFS_STRUCTURE *kernel_vfs_file_touch( uint8_t *path, uint64_t length, uint8_t type ) {
	// remove all "white" characters from path
	length = lib_string_trim( path, length );

	// remove all SLASH characters from end of path
	while( path[ length - 1 ] == '/' ) length--;

	// properties of directory from path
	struct LIB_VFS_STRUCTURE *directory;
	if( ! (directory = kernel_vfs_path( path, length, TRUE )) ) return EMPTY;	// path not resolvable

	// content of directory
	struct LIB_VFS_STRUCTURE *file = (struct LIB_VFS_STRUCTURE *) directory -> offset;

	// search for 

	// cannot resolve path before file
	return EMPTY;
}

void kernel_vfs_file_write( struct KERNEL_VFS_STRUCTURE *socket, uint8_t *source, uint64_t seek, uint64_t byte ) {
	// lock exclusive access
	MACRO_LOCK( socket -> semaphore );

	// properties of file
	struct LIB_VFS_STRUCTURE *file = (struct LIB_VFS_STRUCTURE *) socket -> knot;

	// invalid write request?
	if( seek + byte > MACRO_PAGE_ALIGN_UP( file -> byte ) ) {
		// prepare new file content area
		uint8_t *new = (uint8_t *) kernel -> memory_alloc( MACRO_PAGE_ALIGN_UP( seek + byte ) >> STD_SHIFT_PAGE );
		uint8_t *old = (uint8_t *) file -> offset;

		// copy current content to new location only if current file content will not be truncated
		if( ! seek ) for( uint64_t i = 0; i < file -> byte; i++ ) new[ i ] = old[ i ];

		// release old file content
		kernel -> memory_release( file -> offset, MACRO_PAGE_ALIGN_UP( file -> byte ) >> STD_SHIFT_PAGE );

		// update file properties with new content location
		file -> offset = (uintptr_t) new;
	}

	// INFO: writing to file from seek == EMPTY, means the same as create new content

	// copy content of memory to file
	uint8_t *target = (uint8_t *) file -> offset + seek;
	for( uint64_t i = 0; i < byte; i++ ) target[ i ] = source[ i ];

	// truncate file size?
	if( ! seek ) {
		// remove obsolete blocks of file
		if( MACRO_PAGE_ALIGN_UP( file -> byte ) > MACRO_PAGE_ALIGN_UP( byte ) ) {
			// locate amount of file blocks to truncate
			uint64_t blocks = (MACRO_PAGE_ALIGN_UP( file -> byte ) - MACRO_PAGE_ALIGN_UP( byte )) >> STD_SHIFT_PAGE;

			// remove unused blocks from file
			kernel -> memory_release( file -> offset + (MACRO_PAGE_ALIGN_UP( file -> byte ) - (blocks << STD_SHIFT_PAGE)), blocks );
		}

		// new file size
		file -> byte = byte;
	} else
		// file size will change after overwrite?
		if( seek + byte > file -> byte ) file -> byte = seek + byte;	// yes
	
	// unlock access
	MACRO_UNLOCK( socket -> semaphore );
}

uint8_t	kernel_vfs_identify( uintptr_t base_address, uint64_t limit_byte ) {
	// file properties in chunks
	uint32_t *vfs = (uint32_t *) base_address;

	// at end of file, magic value exist?
	if( vfs[ (limit_byte >> STD_SHIFT_4) - 1 ] == LIB_VFS_magic ) return TRUE;	// yes

	// no
	return FALSE;
}

struct LIB_VFS_STRUCTURE *kernel_vfs_path( uint8_t *path, uint64_t length, uint8_t penultimate ) {
	// remove all "white" characters from path
	length = lib_string_trim( path, length );

	// properties of current file
	struct LIB_VFS_STRUCTURE *file;

	// start from current file?
	if( *path != STD_ASCII_SLASH ) {
		// properties of task
		struct KERNEL_TASK_STRUCTURE *task = kernel_task_active();
	
		// choose task current file
		file = (struct LIB_VFS_STRUCTURE *) task -> directory;
	} else
		// start from default file
		file = (struct LIB_VFS_STRUCTURE *) kernel -> storage_base_address[ kernel -> storage_root ].device_block;

	// remove all SLASH characters from end of path
	while( path[ length - 1 ] == '/' ) length--;

	// if path is empty
	if( ! length )
		// acquired VFS root file
		return file;

	// properties of current directory
	struct LIB_VFS_STRUCTURE *directory = file;

	// parse path
	while( TRUE ) {
		// start from current file
		file = (struct LIB_VFS_STRUCTURE *) file -> offset;

		// remove leading '/', if exist
		while( *path == '/' ) { path++; length--; }

		// select file name from path
		uint64_t file_length = lib_string_word_end( path, length, '/' );

		// search file inside current file
		do { if( file_length == file -> name_length && lib_string_compare( path, (uint8_t *) file -> name, file_length ) ) break;
		} while( (++file) -> name_length );

		// file not found?
		if( ! file -> name_length ) {
			// but requested only previous directory
			if( penultimate ) return directory;
			else return EMPTY;	// no
		}

		// last file from path and requested one?
		if( length == file_length ) {
			// if requested only previous directory
			if( penultimate ) return directory;

			// follow symbolic links (if possible)
			while( file -> type & STD_FILE_TYPE_link ) file = (struct LIB_VFS_STRUCTURE *) file -> offset;

			// acquired file
			return file;
		}

		// preserve current directory
		directory = file;

		// follow symbolic links (if possible)
		while( file -> type & STD_FILE_TYPE_link ) { directory = file; file = (struct LIB_VFS_STRUCTURE *) file -> offset; }

		// remove parsed file from path
		path += file_length; length -= file_length;
	}

	// file not found
	return EMPTY;
}

uint64_t kernel_vfs_socket_add( uint64_t knot ) {
	// lock exclusive access
	MACRO_LOCK( kernel -> vfs_semaphore );

	// available entry, doesn't exist by default
	uint64_t available = -1;

	// search thru all sockets
	for( uint64_t i = 0; i < KERNEL_VFS_limit; i++ ) {
		// if available for use, remember it
		if( ! kernel -> vfs_base_address[ i ].lock ) available = i;

		// file already opened?
		if( kernel -> vfs_base_address[ i ].knot == knot ) {
			// set entry for use
			available = i;

			// done
			break;
		}
	}

	// increase lock level of socket
	if( available != -1 ) kernel -> vfs_base_address[ available ].lock++;
	else available = EMPTY;	// not found

	// unlock access
	MACRO_UNLOCK( kernel -> vfs_semaphore );

	// all sockets reserved
	return available;
}