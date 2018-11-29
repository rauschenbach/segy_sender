SET(GNSSOFT_SOURCES
	sender.c
	circbuf.c
	link.c
        log.c
        nmea.c
        start.c
	mythreads.c  
	proto.c
        utils.c
	../plugin.c
)

INCLUDE_DIRECTORIES(.)
INCLUDE_DIRECTORIES(..)

find_package (Threads)
ADD_EXECUTABLE(gnssoft_plugin ${GNSSOFT_SOURCES})
INSTALL(TARGETS gnssoft_plugin RUNTIME DESTINATION ${SEEDLINK_PLUGIN_OUTPUT_DIR})

FILE(GLOB descs "${CMAKE_CURRENT_SOURCE_DIR}/descriptions/*.xml")
INSTALL(FILES ${descs} DESTINATION ${SC3_PACKAGE_APP_DESC_DIR})
target_link_libraries (gnssoft_plugin ${CMAKE_THREAD_LIBS_INIT} -lgps)
