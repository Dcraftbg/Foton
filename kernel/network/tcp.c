/*===============================================================================
 Copyright (C) Andrzej Adamczyk (at https://blackdev.org/). All rights reserved.
===============================================================================*/

void kernel_network_tcp( struct KERNEL_NETWORK_STRUCTURE_HEADER_ETHERNET *ethernet, uint16_t length ) {
	// properties of IPv4 header
	struct KERNEL_NETWORK_STRUCTURE_HEADER_IPV4 *ipv4 = (struct KERNEL_NETWORK_STRUCTURE_HEADER_IPV4 *) ((uintptr_t) ethernet + sizeof( struct KERNEL_NETWORK_STRUCTURE_HEADER_ETHERNET ));

	// IPv4 header length
	uint16_t ipv4_header_length = (ipv4 -> version_and_header_length & 0x0F) << STD_SHIFT_4;

	// properties of TCP header
	struct KERNEL_NETWORK_STRUCTURE_HEADER_TCP *tcp = (struct KERNEL_NETWORK_STRUCTURE_HEADER_TCP *) ((uintptr_t) ipv4 + ipv4_header_length);

	// search for corresponding socket
	for( uint64_t i = 0; i < KERNEL_NETWORK_SOCKET_limit; i++ ) {
		// properties of socket
		struct KERNEL_NETWORK_STRUCTURE_SOCKET *socket = (struct KERNEL_NETWORK_STRUCTURE_SOCKET *) &kernel -> network_socket_list[ i ];

		// designed port?
		if( tcp -> port_local != MACRO_ENDIANNESS_WORD( socket -> port_local ) ) continue;	// no

		// answer to SYN request?
		if( tcp -> flags == socket -> tcp_flags_required || (tcp -> flags == KERNEL_NETWORK_HEADER_TCP_FLAG_ACK && socket -> tcp_flags_required == (KERNEL_NETWORK_HEADER_TCP_FLAG_SYN | KERNEL_NETWORK_HEADER_TCP_FLAG_ACK)) ) {	// yes
			// change socket status to connected
			socket -> tcp_flags = KERNEL_NETWORK_HEADER_TCP_FLAG_ACK;
			socket -> tcp_flags_required = EMPTY;	// nothing more to do

			// update sequence value
			socket -> tcp_sequence = socket -> tcp_sequence_required;
		
			// preserve connection properties
			socket -> tcp_acknowledgment = MACRO_ENDIANNESS_DWORD( tcp -> sequence ) + 1;

			// update keep-alive timeout to ~1 hours
			socket -> tcp_keep_alive = kernel -> time_unit + KERNEL_NETWORK_HEADER_TCP_KEEP_ALIVE_timeout;

			// do not respond to ACK response
			if( tcp -> flags != KERNEL_NETWORK_HEADER_TCP_FLAG_ACK && socket -> tcp_flags_required != (KERNEL_NETWORK_HEADER_TCP_FLAG_SYN | KERNEL_NETWORK_HEADER_TCP_FLAG_ACK) ) {
				// allocate area for ethernet/tcp frame
				struct KERNEL_NETWORK_STRUCTURE_HEADER_ETHERNET *ethernet = (struct KERNEL_NETWORK_STRUCTURE_HEADER_ETHERNET *) kernel_memory_alloc( TRUE );

				// properties of IPv4 header
				struct KERNEL_NETWORK_STRUCTURE_HEADER_IPV4 *ipv4 = (struct KERNEL_NETWORK_STRUCTURE_HEADER_IPV4 *) ((uintptr_t) ethernet + sizeof( struct KERNEL_NETWORK_STRUCTURE_HEADER_ETHERNET ));

				// default IPv4 header properties
				ipv4 -> version_and_header_length = KERNEL_NETWORK_HEADER_IPV4_VERSION_AND_HEADER_LENGTH_default;

				// IPv4 header length
				uint16_t ipv4_header_length = (ipv4 -> version_and_header_length & 0x0F) << STD_SHIFT_4;

				// properties of TCP header
				struct KERNEL_NETWORK_STRUCTURE_HEADER_TCP *tcp = (struct KERNEL_NETWORK_STRUCTURE_HEADER_TCP *) ((uintptr_t) ipv4 + ipv4_header_length);

				// properties of TCP frame
				tcp -> sequence		= MACRO_ENDIANNESS_DWORD( socket -> tcp_sequence );
				tcp -> acknowledgment	= MACRO_ENDIANNESS_DWORD( socket -> tcp_acknowledgment );
				tcp -> header_length	= KERNEL_NETWORK_HEADER_TCP_HEADER_LENGTH_default;
				tcp -> flags		= KERNEL_NETWORK_HEADER_TCP_FLAG_ACK;	// thank you

				// encapsulate TCP frame and send
				kernel_network_tcp_encapsulate( socket, ethernet, sizeof( struct KERNEL_NETWORK_STRUCTURE_HEADER_TCP ) );
			}
		}

		// answer to ACK (keep-alive) request?
		if( tcp -> flags == socket -> tcp_flags_required ) {	// yes
			// change socket status to ok
			socket -> tcp_flags_required = EMPTY;

			// update keep-alive timeout to ~1 hours
			socket -> tcp_keep_alive = kernel -> time_unit + KERNEL_NETWORK_HEADER_TCP_KEEP_ALIVE_timeout;
		}
	}
}

void kernel_network_tcp_encapsulate( struct KERNEL_NETWORK_STRUCTURE_SOCKET *socket, struct KERNEL_NETWORK_STRUCTURE_HEADER_ETHERNET *ethernet, uint16_t length ) {
	// properties of IPv4 header
	struct KERNEL_NETWORK_STRUCTURE_HEADER_IPV4 *ipv4 = (struct KERNEL_NETWORK_STRUCTURE_HEADER_IPV4 *) ((uintptr_t) ethernet + sizeof( struct KERNEL_NETWORK_STRUCTURE_HEADER_ETHERNET ));

	// IPv4 header length
	uint16_t ipv4_header_length = (ipv4 -> version_and_header_length & 0x0F) << STD_SHIFT_4;

	// properties of TCP header
	struct KERNEL_NETWORK_STRUCTURE_HEADER_TCP *tcp = (struct KERNEL_NETWORK_STRUCTURE_HEADER_TCP *) ((uintptr_t) ipv4 + ipv4_header_length);

	// default properties of TCP header
	tcp -> port_source	= MACRO_ENDIANNESS_WORD( socket -> port_local );
	tcp -> port_local	= MACRO_ENDIANNESS_WORD( socket -> port_target );
	tcp -> window_size	= MACRO_ENDIANNESS_WORD( socket -> tcp_window_size );

	// properties of TCP Pseudo header
	struct KERNEL_NETWORK_STRUCTURE_HEADER_PSEUDO *pseudo = (struct KERNEL_NETWORK_STRUCTURE_HEADER_PSEUDO *) ((uintptr_t) tcp - sizeof( struct KERNEL_NETWORK_STRUCTURE_HEADER_PSEUDO ));
	pseudo -> local = MACRO_ENDIANNESS_DWORD( kernel -> network_interface.ipv4_address );
	pseudo -> target = MACRO_ENDIANNESS_DWORD( socket -> ipv4_target );
	pseudo -> reserved = EMPTY;	// always
	pseudo -> protocol = KERNEL_NETWORK_HEADER_IPV4_PROTOCOL_tcp;
	pseudo -> length = MACRO_ENDIANNESS_WORD( length );

	// calculate checksum
	tcp -> checksum = EMPTY;	// always
	tcp -> checksum = kernel_network_checksum( (uint16_t *) pseudo, sizeof( struct KERNEL_NETWORK_STRUCTURE_HEADER_PSEUDO ) + length );

	// wrap data into a IPv4 frame and send
	kernel_network_ipv4_encapsulate( socket, ethernet, length );
}

void kernel_network_tcp_exit( struct KERNEL_NETWORK_STRUCTURE_SOCKET *socket, uint8_t *data, uint16_t length ) {
	// wait for socket ready
	while( socket -> tcp_flags != KERNEL_NETWORK_HEADER_TCP_FLAG_ACK ) kernel_time_sleep( TRUE );

	// begin connection/synchronization
	socket -> tcp_flags		= KERNEL_NETWORK_HEADER_TCP_FLAG_PSH | KERNEL_NETWORK_HEADER_TCP_FLAG_ACK;
	socket -> tcp_flags_required	= KERNEL_NETWORK_HEADER_TCP_FLAG_ACK;

	// sending X Bytes
	socket -> tcp_sequence_required = socket -> tcp_sequence + length;

	// PSH valid ~1 second
	socket -> tcp_keep_alive = kernel -> time_unit + DRIVER_RTC_Hz;

	// allocate area for ethernet/tcp frame
	struct KERNEL_NETWORK_STRUCTURE_HEADER_ETHERNET *ethernet = (struct KERNEL_NETWORK_STRUCTURE_HEADER_ETHERNET *) kernel_memory_alloc( TRUE );

	// properties of IPv4 header
	struct KERNEL_NETWORK_STRUCTURE_HEADER_IPV4 *ipv4 = (struct KERNEL_NETWORK_STRUCTURE_HEADER_IPV4 *) ((uintptr_t) ethernet + sizeof( struct KERNEL_NETWORK_STRUCTURE_HEADER_ETHERNET ));

	// default IPv4 header properties
	ipv4 -> version_and_header_length = KERNEL_NETWORK_HEADER_IPV4_VERSION_AND_HEADER_LENGTH_default;

	// IPv4 header length
	uint16_t ipv4_header_length = (ipv4 -> version_and_header_length & 0x0F) << STD_SHIFT_4;

	// properties of TCP header
	struct KERNEL_NETWORK_STRUCTURE_HEADER_TCP *tcp = (struct KERNEL_NETWORK_STRUCTURE_HEADER_TCP *) ((uintptr_t) ipv4 + ipv4_header_length);

	// properties of TCP frame
	tcp -> flags		= socket -> tcp_flags;
	tcp -> sequence		= MACRO_ENDIANNESS_DWORD( socket -> tcp_sequence );
	tcp -> acknowledgment	= MACRO_ENDIANNESS_DWORD( --socket -> tcp_acknowledgment );
	tcp -> header_length	= KERNEL_NETWORK_HEADER_TCP_HEADER_LENGTH_default;

	// copy data
	uint8_t *tcp_data = (uint8_t *) ((uintptr_t) tcp + sizeof( struct KERNEL_NETWORK_STRUCTURE_HEADER_TCP ));
	for( uint16_t i = 0; i < length; i++ ) tcp_data[ i ] = data[ i ];

	// encapsulate TCP frame and send
	kernel_network_tcp_encapsulate( socket, ethernet, sizeof( struct KERNEL_NETWORK_STRUCTURE_HEADER_TCP ) + length );
}

void kernel_network_tcp_thread( void ) {
	// hold the door
	while( TRUE ) {
		// search for socket to initialize
		for( uint64_t i = 0; i < KERNEL_NETWORK_SOCKET_limit; i++ ) {
			// properties of socket
			struct KERNEL_NETWORK_STRUCTURE_SOCKET *socket = (struct KERNEL_NETWORK_STRUCTURE_SOCKET *) &kernel -> network_socket_list[ i ];

			// ignore sockets other than TCP
			if( socket -> protocol != STD_NETWORK_PROTOCOL_tcp ) continue;

			// TCP socket waiting for initialization or previous initialization is outdated
			if( socket -> flags & KERNEL_NETWORK_SOCKET_FLAG_init || (socket -> tcp_flags == KERNEL_NETWORK_HEADER_TCP_FLAG_SYN && socket -> tcp_keep_alive < kernel -> time_unit) ) {
				// start connection initialiation
				socket -> flags &= ~KERNEL_NETWORK_SOCKET_FLAG_init;

				// set initial socket configuration of TCP protocol

				// begin connection/synchronization
				socket -> tcp_flags		= KERNEL_NETWORK_HEADER_TCP_FLAG_SYN;
				socket -> tcp_flags_required	= KERNEL_NETWORK_HEADER_TCP_FLAG_SYN | KERNEL_NETWORK_HEADER_TCP_FLAG_ACK;

				// generate new sequence number
				socket -> tcp_sequence		= EMPTY;	// DEBUG
				socket -> tcp_sequence_required	= socket -> tcp_sequence + 1;

				// default window size
				socket -> tcp_window_size = KERNEL_NETWORK_HEADER_TCP_WINDOW_SIZE_default;

				// SYN valid for ~1 second
				socket -> tcp_keep_alive = kernel -> time_unit + DRIVER_RTC_Hz;

				// allocate area for ethernet/tcp frame
				struct KERNEL_NETWORK_STRUCTURE_HEADER_ETHERNET *ethernet = (struct KERNEL_NETWORK_STRUCTURE_HEADER_ETHERNET *) kernel_memory_alloc( TRUE );

				// properties of IPv4 header
				struct KERNEL_NETWORK_STRUCTURE_HEADER_IPV4 *ipv4 = (struct KERNEL_NETWORK_STRUCTURE_HEADER_IPV4 *) ((uintptr_t) ethernet + sizeof( struct KERNEL_NETWORK_STRUCTURE_HEADER_ETHERNET ));

				// default IPv4 header properties
				ipv4 -> version_and_header_length = KERNEL_NETWORK_HEADER_IPV4_VERSION_AND_HEADER_LENGTH_default;

				// IPv4 header length
				uint16_t ipv4_header_length = (ipv4 -> version_and_header_length & 0x0F) << STD_SHIFT_4;

				// properties of TCP header
				struct KERNEL_NETWORK_STRUCTURE_HEADER_TCP *tcp = (struct KERNEL_NETWORK_STRUCTURE_HEADER_TCP *) ((uintptr_t) ipv4 + ipv4_header_length);

				// properties of TCP frame
				tcp -> flags		= socket -> tcp_flags;
				tcp -> sequence		= MACRO_ENDIANNESS_DWORD( socket -> tcp_sequence );
				tcp -> acknowledgment	= EMPTY;	// we do not know it yet
				tcp -> header_length	= KERNEL_NETWORK_HEADER_TCP_HEADER_LENGTH_default;

				// encapsulate TCP frame and send
				kernel_network_tcp_encapsulate( socket, ethernet, sizeof( struct KERNEL_NETWORK_STRUCTURE_HEADER_TCP ) );
			}

			// TCP socket waiting for initialization or previous initialization is outdated
			if( socket -> tcp_flags == KERNEL_NETWORK_HEADER_TCP_FLAG_ACK && socket -> tcp_keep_alive < kernel -> time_unit) {
				// start keep_alive session
				socket -> tcp_flags_required	= KERNEL_NETWORK_HEADER_TCP_FLAG_ACK;

				// request same sequence number
				socket -> tcp_sequence_required	= socket -> tcp_sequence;

				// ACK valid for ~1 second
				socket -> tcp_keep_alive = kernel -> time_unit + DRIVER_RTC_Hz;

				// allocate area for ethernet/tcp frame
				struct KERNEL_NETWORK_STRUCTURE_HEADER_ETHERNET *ethernet = (struct KERNEL_NETWORK_STRUCTURE_HEADER_ETHERNET *) kernel_memory_alloc( TRUE );

				// properties of IPv4 header
				struct KERNEL_NETWORK_STRUCTURE_HEADER_IPV4 *ipv4 = (struct KERNEL_NETWORK_STRUCTURE_HEADER_IPV4 *) ((uintptr_t) ethernet + sizeof( struct KERNEL_NETWORK_STRUCTURE_HEADER_ETHERNET ));

				// default IPv4 header properties
				ipv4 -> version_and_header_length = KERNEL_NETWORK_HEADER_IPV4_VERSION_AND_HEADER_LENGTH_default;

				// IPv4 header length
				uint16_t ipv4_header_length = (ipv4 -> version_and_header_length & 0x0F) << STD_SHIFT_4;

				// properties of TCP header
				struct KERNEL_NETWORK_STRUCTURE_HEADER_TCP *tcp = (struct KERNEL_NETWORK_STRUCTURE_HEADER_TCP *) ((uintptr_t) ipv4 + ipv4_header_length);

				// properties of TCP frame
				tcp -> flags		= socket -> tcp_flags;
				tcp -> sequence		= MACRO_ENDIANNESS_DWORD( (socket -> tcp_sequence - 1) );
				tcp -> acknowledgment	= MACRO_ENDIANNESS_DWORD( (socket -> tcp_acknowledgment - 1) );
				tcp -> header_length	= KERNEL_NETWORK_HEADER_TCP_HEADER_LENGTH_default;

				// encapsulate TCP frame and send
				kernel_network_tcp_encapsulate( socket, ethernet, sizeof( struct KERNEL_NETWORK_STRUCTURE_HEADER_TCP ) );
			}
		}
	}
}