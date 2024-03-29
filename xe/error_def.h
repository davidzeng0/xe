#pragma once

enum xe_error : int{
	XE_FIRST = -1100,

	XE_FATAL,
	XE_STATE,

	XE_RESOLVER,
	XE_UNKNOWN_HOST,
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
	XE_SSL_NO_ALPN,

	XE_RECV_ERROR,
	XE_SEND_ERROR,

	XE_PARTIAL_FILE,
	XE_HEADERS_TOO_LONG,
	XE_INVALID_RESPONSE,
	XE_TOO_MANY_REDIRECTS,
	XE_EXTERNAL_REDIRECT,

	XE_WEBSOCKET_CONNECTION_REFUSED,
	XE_WEBSOCKET_MESSAGE_TOO_LONG,

	XE_LAST,

	XE_EHWPOISON = xe_syserror(EHWPOISON),
	XE_ERFKILL = xe_syserror(ERFKILL),
	XE_EKEYREJECTED = xe_syserror(EKEYREJECTED),
	XE_EKEYREVOKED = xe_syserror(EKEYREVOKED),
	XE_EKEYEXPIRED = xe_syserror(EKEYEXPIRED),
	XE_ENOKEY = xe_syserror(ENOKEY),
	XE_EMEDIUMTYPE = xe_syserror(EMEDIUMTYPE),
	XE_ENOMEDIUM = xe_syserror(ENOMEDIUM),
	XE_EREMOTEIO = xe_syserror(EREMOTEIO),
	XE_EISNAM = xe_syserror(EISNAM),
	XE_ENAVAIL = xe_syserror(ENAVAIL),
	XE_ENOTNAM = xe_syserror(ENOTNAM),
	XE_EUCLEAN = xe_syserror(EUCLEAN),
	XE_ESTRPIPE = xe_syserror(ESTRPIPE),
	XE_ELIBEXEC = xe_syserror(ELIBEXEC),
	XE_ELIBMAX = xe_syserror(ELIBMAX),
	XE_ELIBSCN = xe_syserror(ELIBSCN),
	XE_ELIBBAD = xe_syserror(ELIBBAD),
	XE_ELIBACC = xe_syserror(ELIBACC),
	XE_EREMCHG = xe_syserror(EREMCHG),
	XE_EBADFD = xe_syserror(EBADFD),
	XE_ENOTUNIQ = xe_syserror(ENOTUNIQ),
	XE_EDOTDOT = xe_syserror(EDOTDOT),
	XE_ECOMM = xe_syserror(ECOMM),
	XE_ESRMNT = xe_syserror(ESRMNT),
	XE_EADV = xe_syserror(EADV),
	XE_ENOPKG = xe_syserror(ENOPKG),
	XE_ENONET = xe_syserror(ENONET),
	XE_EBFONT = xe_syserror(EBFONT),
	XE_EBADSLT = xe_syserror(EBADSLT),
	XE_EBADRQC = xe_syserror(EBADRQC),
	XE_ENOANO = xe_syserror(ENOANO),
	XE_EXFULL = xe_syserror(EXFULL),
	XE_EBADR = xe_syserror(EBADR),
	XE_EBADE = xe_syserror(EBADE),
	XE_EL2HLT = xe_syserror(EL2HLT),
	XE_ENOCSI = xe_syserror(ENOCSI),
	XE_EUNATCH = xe_syserror(EUNATCH),
	XE_ELNRNG = xe_syserror(ELNRNG),
	XE_EL3RST = xe_syserror(EL3RST),
	XE_EL3HLT = xe_syserror(EL3HLT),
	XE_EL2NSYNC = xe_syserror(EL2NSYNC),
	XE_ECHRNG = xe_syserror(ECHRNG),
	XE_ERESTART = xe_syserror(ERESTART),
	XE_ENOTRECOVERABLE = xe_syserror(ENOTRECOVERABLE),
	XE_EOWNERDEAD = xe_syserror(EOWNERDEAD),
	XE_ECANCELED = xe_syserror(ECANCELED),
	XE_ETIME = xe_syserror(ETIME),
	XE_EPROTO = xe_syserror(EPROTO),
	XE_EOVERFLOW = xe_syserror(EOVERFLOW),
	XE_ENOSTR = xe_syserror(ENOSTR),
	XE_ENOSR = xe_syserror(ENOSR),
	XE_ENOMSG = xe_syserror(ENOMSG),
	XE_ENOLINK = xe_syserror(ENOLINK),
	XE_ENODATA = xe_syserror(ENODATA),
	XE_EMULTIHOP = xe_syserror(EMULTIHOP),
	XE_EIDRM = xe_syserror(EIDRM),
	XE_EBADMSG = xe_syserror(EBADMSG),
	XE_EILSEQ = xe_syserror(EILSEQ),
	XE_ENOSYS = xe_syserror(ENOSYS),
	XE_ENOLCK = xe_syserror(ENOLCK),
	XE_EREMOTE = xe_syserror(EREMOTE),
	XE_ESTALE = xe_syserror(ESTALE),
	XE_EDQUOT = xe_syserror(EDQUOT),
	XE_EUSERS = xe_syserror(EUSERS),
	XE_ENOTEMPTY = xe_syserror(ENOTEMPTY),
	XE_EHOSTUNREACH = xe_syserror(EHOSTUNREACH),
	XE_EHOSTDOWN = xe_syserror(EHOSTDOWN),
	XE_ENAMETOOLONG = xe_syserror(ENAMETOOLONG),
	XE_ELOOP = xe_syserror(ELOOP),
	XE_ECONNREFUSED = xe_syserror(ECONNREFUSED),
	XE_ETIMEDOUT = xe_syserror(ETIMEDOUT),
	XE_ETOOMANYREFS = xe_syserror(ETOOMANYREFS),
	XE_ESHUTDOWN = xe_syserror(ESHUTDOWN),
	XE_EDESTADDRREQ = xe_syserror(EDESTADDRREQ),
	XE_ENOTCONN = xe_syserror(ENOTCONN),
	XE_EISCONN = xe_syserror(EISCONN),
	XE_ENOBUFS = xe_syserror(ENOBUFS),
	XE_ECONNRESET = xe_syserror(ECONNRESET),
	XE_ECONNABORTED = xe_syserror(ECONNABORTED),
	XE_ENETRESET = xe_syserror(ENETRESET),
	XE_ENETUNREACH = xe_syserror(ENETUNREACH),
	XE_ENETDOWN = xe_syserror(ENETDOWN),
	XE_EADDRNOTAVAIL = xe_syserror(EADDRNOTAVAIL),
	XE_EADDRINUSE = xe_syserror(EADDRINUSE),
	XE_EAFNOSUPPORT = xe_syserror(EAFNOSUPPORT),
	XE_EPFNOSUPPORT = xe_syserror(EPFNOSUPPORT),
	XE_EOPNOTSUPP = xe_syserror(EOPNOTSUPP),
	XE_ESOCKTNOSUPPORT = xe_syserror(ESOCKTNOSUPPORT),
	XE_EPROTONOSUPPORT = xe_syserror(EPROTONOSUPPORT),
	XE_ENOPROTOOPT = xe_syserror(ENOPROTOOPT),
	XE_EPROTOTYPE = xe_syserror(EPROTOTYPE),
	XE_EMSGSIZE = xe_syserror(EMSGSIZE),
	XE_ENOTSOCK = xe_syserror(ENOTSOCK),
	XE_EALREADY = xe_syserror(EALREADY),
	XE_EINPROGRESS = xe_syserror(EINPROGRESS),
	XE_EAGAIN = xe_syserror(EAGAIN),
	XE_ERANGE = xe_syserror(ERANGE),
	XE_EDOM = xe_syserror(EDOM),
	XE_EPIPE = xe_syserror(EPIPE),
	XE_EMLINK = xe_syserror(EMLINK),
	XE_EROFS = xe_syserror(EROFS),
	XE_ESPIPE = xe_syserror(ESPIPE),
	XE_ENOSPC = xe_syserror(ENOSPC),
	XE_EFBIG = xe_syserror(EFBIG),
	XE_ETXTBSY = xe_syserror(ETXTBSY),
	XE_ENOTTY = xe_syserror(ENOTTY),
	XE_ENFILE = xe_syserror(ENFILE),
	XE_EMFILE = xe_syserror(EMFILE),
	XE_EINVAL = xe_syserror(EINVAL),
	XE_EISDIR = xe_syserror(EISDIR),
	XE_ENOTDIR = xe_syserror(ENOTDIR),
	XE_ENODEV = xe_syserror(ENODEV),
	XE_EXDEV = xe_syserror(EXDEV),
	XE_EEXIST = xe_syserror(EEXIST),
	XE_EBUSY = xe_syserror(EBUSY),
	XE_ENOTBLK = xe_syserror(ENOTBLK),
	XE_EFAULT = xe_syserror(EFAULT),
	XE_EACCES = xe_syserror(EACCES),
	XE_ENOMEM = xe_syserror(ENOMEM),
	XE_EDEADLK = xe_syserror(EDEADLK),
	XE_ECHILD = xe_syserror(ECHILD),
	XE_EBADF = xe_syserror(EBADF),
	XE_ENOEXEC = xe_syserror(ENOEXEC),
	XE_E2BIG = xe_syserror(E2BIG),
	XE_ENXIO = xe_syserror(ENXIO),
	XE_EIO = xe_syserror(EIO),
	XE_EINTR = xe_syserror(EINTR),
	XE_ESRCH = xe_syserror(ESRCH),
	XE_ENOENT = xe_syserror(ENOENT),
	XE_EPERM = xe_syserror(EPERM),

	XE_OK = 0
};