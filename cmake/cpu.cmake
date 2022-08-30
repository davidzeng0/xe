if(NOT DEFINED XE_PAGESIZE)
	try_run(RUN_RESULT COMPILE_RESULT ${CMAKE_CURRENT_BINARY_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/pagesize.cc" COMPILE_OUTPUT_VARIABLE COMPILE_ERROR RUN_OUTPUT_VARIABLE XE_PAGESIZE)

	if(NOT COMPILE_RESULT)
		message(SEND_ERROR "Failed to detect page size")
		message(SEND_ERROR ${COMPILE_ERROR})
	endif()

	if(RUN_RESULT)
		message(SEND_ERROR "Failed to detect page size, exit code: ${XE_RUN_CACHESIZE}")
	endif()
endif()

if(NOT DEFINED XE_CACHESIZE)
	try_run(RUN_RESULT COMPILE_RESULT ${CMAKE_CURRENT_BINARY_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cachesize.cc" COMPILE_OUTPUT_VARIABLE COMPILE_ERROR RUN_OUTPUT_VARIABLE XE_CACHESIZE)

	if(NOT COMPILE_RESULT)
		message(SEND_ERROR "Failed to detect cache size")
		message(SEND_ERROR ${COMPILE_ERROR})
	endif()

	if(RUN_RESULT)
		message(SEND_ERROR "Failed to detect cache size, exit code: ${XE_RUN_CACHESIZE}")
	endif()
endif()

configure_file("cmake/cpu.h.in" "${CMAKE_CURRENT_SOURCE_DIR}/include/xconfig/cpu.h")