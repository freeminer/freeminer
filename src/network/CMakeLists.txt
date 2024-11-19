set(common_network_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/fm_lan.cpp
	# TODO merge:
	${CMAKE_CURRENT_SOURCE_DIR}/multi/connection.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/enet/connection.cpp
	#${CMAKE_CURRENT_SOURCE_DIR}/fm_connection_sctp.cpp
	#${CMAKE_CURRENT_SOURCE_DIR}/fm_connection_websocket_sctp.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/ws/wssocket.cpp
	#${CMAKE_CURRENT_SOURCE_DIR}/ws/connection.cpp
	#${CMAKE_CURRENT_SOURCE_DIR}/ws/connectionthreads.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/ws/impl.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/ws/threads.cpp

	${CMAKE_CURRENT_SOURCE_DIR}/address.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/connection.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/mtp/impl.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/mtp/threads.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/networkpacket.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/networkprotocol.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/socket.cpp
	PARENT_SCOPE
)

set(server_network_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/serveropcodes.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/serverpackethandler.cpp
	PARENT_SCOPE
)

if (BUILD_CLIENT)
	set(client_network_SRCS
		${CMAKE_CURRENT_SOURCE_DIR}/clientopcodes.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/clientpackethandler.cpp
		PARENT_SCOPE
	)
endif()
