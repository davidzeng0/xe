if(NOT DEFINED XE_PAGESIZE)
	try_run(XE_RUN_PAGESIZE XE_COMPILE_PAGESIZE ${CMAKE_CURRENT_BINARY_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/pagesize.cc" RUN_OUTPUT_VARIABLE XE_PAGESIZE)

	if(NOT XE_COMPILE_PAGESIZE)
		message(SEND_ERROR "Failed to detect page size")
	endif()
endif()

if(NOT DEFINED XE_CACHESIZE)
	try_run(XE_RUN_CACHESIZE XE_COMPILE_CACHESIZE ${CMAKE_CURRENT_BINARY_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cachesize.cc" RUN_OUTPUT_VARIABLE XE_CACHESIZE)

	if(NOT XE_COMPILE_CACHESIZE)
		message(SEND_ERROR "Failed to detect cache size")
	endif()
endif()

configure_file("cmake/cpu.h.in" "${CMAKE_CURRENT_SOURCE_DIR}/include/xe/cpu.h")