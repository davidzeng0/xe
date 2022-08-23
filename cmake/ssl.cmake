set(cafile_paths
	/etc/ssl/certs/ca-certificates.crt
	/etc/pki/tls/certs/ca-bundle.crt
	/usr/share/ssl/certs/ca-bundle.crt
	/usr/local/share/certs/ca-root-nss.crt
	/etc/ssl/cert.pem
)

set(capath_paths "/etc/ssl/certs")

set(XE_SSL_CAFILE "null")
set(XE_SSL_CAPATH "null")

foreach(cafile_path ${cafile_paths})
	if(EXISTS ${cafile_path})
		set(XE_SSL_CAFILE "\"${cafile_path}\"")
		break()
	endif()
endforeach()

if(EXISTS ${capath_paths})
	set(XE_SSL_CAPATH "\"${capath_paths}\"")
endif()

configure_file("cmake/ssl.h.in" "${CMAKE_CURRENT_SOURCE_DIR}/include/xconfig/ssl.h")