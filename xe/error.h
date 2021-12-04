#pragma once
#include <string.h>
#include <errno.h>
#include "types.h"

static constexpr int xe_syserror(int err){
	return -err;
}

static constexpr int xe_unsyserror(int err){
	return -err;
}

enum xe_error{
	XE_FIRST = -32768,
	XE_ETOOMANYHANDLES,

	/* net */
	XE_ERESOLVER,
	XE_RESOLVER_UNKNOWN_HOST,
	XE_RESOLVER_SERVER_FAIL,
	XE_RESOLVER_CLIENT_FAIL,
	XE_RESOLVER_TIMEOUT,
	XE_RESOLVER_CONNREFUSED,
	XE_RESOLVER_BADNAME,
	XE_MALFORMED_URL,
	XE_SSL,
	XE_SSL_BADCERTS,
	XE_SSL_NO_SIGNER,
	XE_SSL_PEER_VERIFICATION_FAILED,
	XE_EOF,
	XE_SEND_ERROR,
	XE_ABORTED,
	XE_HEADERS_TOO_LONG,
	XE_INVALID_RESPONSE,
	/* net */

	XE_LAST,
	XE_ECANCELED = xe_syserror(ECANCELED),
	XE_EINPROGRESS = xe_syserror(EINPROGRESS),
	XE_EALREADY = xe_syserror(EALREADY),
	XE_ECONNREFUSED = xe_syserror(ECONNREFUSED),
	XE_ECONNRESET = xe_syserror(ECONNRESET),
	XE_EOPNOTSUPP = xe_syserror(EOPNOTSUPP),
	XE_ENOSYS = xe_syserror(ENOSYS),
	XE_EINVAL = xe_syserror(EINVAL),
	XE_ENOMEM = xe_syserror(ENOMEM),
	XE_EAGAIN = xe_syserror(EAGAIN),
	XE_NONE = 0
};

static int xe_issyserror(int err){
	return err > XE_LAST;
}

static xe_cstr xe_strerror(int err){
	switch(err){
		case XE_NONE:
			return "OK";
		case XE_ETOOMANYHANDLES:
			return "Too many I/O handles";
		case XE_ERESOLVER:
			return "Could not resolve host";
		case XE_RESOLVER_UNKNOWN_HOST:
			return "Unknown host";
		case XE_RESOLVER_SERVER_FAIL:
			return "DNS server failed";
		case XE_RESOLVER_CLIENT_FAIL:
			return "DNS client failed";
		case XE_RESOLVER_TIMEOUT:
			return "DNS timed out";
		case XE_RESOLVER_CONNREFUSED:
			return "DNS connection refused";
		case XE_RESOLVER_BADNAME:
			return "Bad hostname";
		case XE_MALFORMED_URL:
			return "Malformed URL";
		case XE_SSL:
			return "SSL error";
		case XE_SSL_BADCERTS:
			return "Invalid certificate file(s)";
		case XE_SSL_NO_SIGNER:
			return "CA signer not available for verification";
		case XE_SSL_PEER_VERIFICATION_FAILED:
			return "SSL peer verification failed";
		case XE_EOF:
			return "Stream EOF";
		case XE_SEND_ERROR:
			return "Socket peer not accepting data";
		case XE_ABORTED:
			return "Aborted";
		case XE_HEADERS_TOO_LONG:
			return "Headers too long";
		case XE_INVALID_RESPONSE:
			return "Invalid response";
		default:
			if(err > XE_LAST)
				return strerror(xe_unsyserror(err));
			return "Unknown error";
	}
}