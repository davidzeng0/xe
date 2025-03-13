#include "error.h"

int xe_errno(){
	return xe_syserror(errno);
}

xe_cstr xe_strerror(int err){
	switch(err){
		case XE_OK:
			return "OK";
		case XE_FATAL:
			return "Fatal error";
		case XE_STATE:
			return "Invalid state";
		case XE_RESOLVER:
			return "Could not resolve hostname";
		case XE_UNKNOWN_HOST:
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
		case XE_SSL_NO_ALPN:
			return "SSL no ALPN";
		case XE_RECV_ERROR:
			return "Failed to recv data";
		case XE_SEND_ERROR:
			return "Failed to send data";
		case XE_PARTIAL_FILE:
			return "Partial file";
		case XE_HEADERS_TOO_LONG:
			return "Headers too long";
		case XE_INVALID_RESPONSE:
			return "Invalid response";
		case XE_TOO_MANY_REDIRECTS:
			return "Too many redirects";
		case XE_EXTERNAL_REDIRECT:
			return "External redirect";
		case XE_WEBSOCKET_CONNECTION_REFUSED:
			return "WebSocket connection rejected";
		case XE_WEBSOCKET_MESSAGE_TOO_LONG:
			return "WebSocket message too long";

		case XE_EHWPOISON:
			return "Memory page has hardware error";
		case XE_ERFKILL:
			return "Operation not possible due to RF-kill";
		case XE_EKEYREJECTED:
			return "Key was rejected by service";
		case XE_EKEYREVOKED:
			return "Key has been revoked";
		case XE_EKEYEXPIRED:
			return "Key has expired";
		case XE_ENOKEY:
			return "Required key not available";
		case XE_EMEDIUMTYPE:
			return "Wrong medium type";
		case XE_ENOMEDIUM:
			return "No medium found";
		case XE_EREMOTEIO:
			return "Remote I/O error";
		case XE_EISNAM:
			return "Is a named type file";
		case XE_ENAVAIL:
			return "No XENIX semaphores available";
		case XE_ENOTNAM:
			return "Not a XENIX named type file";
		case XE_EUCLEAN:
			return "Structure needs cleaning";
		case XE_ESTRPIPE:
			return "Streams pipe error";
		case XE_ELIBEXEC:
			return "Cannot exec a shared library directly";
		case XE_ELIBMAX:
			return "Attempting to link in too many shared libraries";
		case XE_ELIBSCN:
			return ".lib section in a.out corrupted";
		case XE_ELIBBAD:
			return "Accessing a corrupted shared library";
		case XE_ELIBACC:
			return "Can not access a needed shared library";
		case XE_EREMCHG:
			return "Remote address changed";
		case XE_EBADFD:
			return "File descriptor in bad state";
		case XE_ENOTUNIQ:
			return "Name not unique on network";
		case XE_EDOTDOT:
			return "RFS specific error";
		case XE_ECOMM:
			return "Communication error on send";
		case XE_ESRMNT:
			return "Srmount error";
		case XE_EADV:
			return "Advertise error";
		case XE_ENOPKG:
			return "Package not installed";
		case XE_ENONET:
			return "Machine is not on the network";
		case XE_EBFONT:
			return "Bad font file format";
		case XE_EBADSLT:
			return "Invalid slot";
		case XE_EBADRQC:
			return "Invalid request code";
		case XE_ENOANO:
			return "No anode";
		case XE_EXFULL:
			return "Exchange full";
		case XE_EBADR:
			return "Invalid request descriptor";
		case XE_EBADE:
			return "Invalid exchange";
		case XE_EL2HLT:
			return "Level 2 halted";
		case XE_ENOCSI:
			return "No CSI structure available";
		case XE_EUNATCH:
			return "Protocol driver not attached";
		case XE_ELNRNG:
			return "Link number out of range";
		case XE_EL3RST:
			return "Level 3 reset";
		case XE_EL3HLT:
			return "Level 3 halted";
		case XE_EL2NSYNC:
			return "Level 2 not synchronized";
		case XE_ECHRNG:
			return "Channel number out of range";
		case XE_ERESTART:
			return "Interrupted system call should be restarted";
		case XE_ENOTRECOVERABLE:
			return "State not recoverable";
		case XE_EOWNERDEAD:
			return "Owner died";
		case XE_ECANCELED:
			return "Operation canceled";
		case XE_ETIME:
			return "Timer expired";
		case XE_EPROTO:
			return "Protocol error";
		case XE_EOVERFLOW:
			return "Value too large for defined data type";
		case XE_ENOSTR:
			return "Device not a stream";
		case XE_ENOSR:
			return "Out of streams resources";
		case XE_ENOMSG:
			return "No message of desired type";
		case XE_ENOLINK:
			return "Link has been severed";
		case XE_ENODATA:
			return "No data available";
		case XE_EMULTIHOP:
			return "Multihop attempted";
		case XE_EIDRM:
			return "Identifier removed";
		case XE_EBADMSG:
			return "Bad message";
		case XE_EILSEQ:
			return "Invalid or incomplete multibyte or wide character";
		case XE_ENOSYS:
			return "Not implemented";
		case XE_ENOLCK:
			return "No locks available";
		case XE_EREMOTE:
			return "Object is remote";
		case XE_ESTALE:
			return "Stale file handle";
		case XE_EDQUOT:
			return "Disk quota exceeded";
		case XE_EUSERS:
			return "Too many users";
		case XE_ENOTEMPTY:
			return "Directory not empty";
		case XE_EHOSTUNREACH:
			return "No route to host";
		case XE_EHOSTDOWN:
			return "Host is down";
		case XE_ENAMETOOLONG:
			return "File name too long";
		case XE_ELOOP:
			return "Too many levels of symbolic links";
		case XE_ECONNREFUSED:
			return "Connection refused";
		case XE_ETIMEDOUT:
			return "Connection timed out";
		case XE_ETOOMANYREFS:
			return "Too many references: cannot splice";
		case XE_ESHUTDOWN:
			return "Cannot send after transport endpoint shutdown";
		case XE_EDESTADDRREQ:
			return "Destination address required";
		case XE_ENOTCONN:
			return "Transport endpoint is not connected";
		case XE_EISCONN:
			return "Transport endpoint is already connected";
		case XE_ENOBUFS:
			return "No buffer space available";
		case XE_ECONNRESET:
			return "Connection reset by peer";
		case XE_ECONNABORTED:
			return "Software caused connection abort";
		case XE_ENETRESET:
			return "Network dropped connection on reset";
		case XE_ENETUNREACH:
			return "Network is unreachable";
		case XE_ENETDOWN:
			return "Network is down";
		case XE_EADDRNOTAVAIL:
			return "Cannot assign requested address";
		case XE_EADDRINUSE:
			return "Address already in use";
		case XE_EAFNOSUPPORT:
			return "Address family not supported by protocol";
		case XE_EPFNOSUPPORT:
			return "Protocol family not supported";
		case XE_EOPNOTSUPP:
			return "Operation not supported";
		case XE_ESOCKTNOSUPPORT:
			return "Socket type not supported";
		case XE_EPROTONOSUPPORT:
			return "Protocol not supported";
		case XE_ENOPROTOOPT:
			return "Protocol not available";
		case XE_EPROTOTYPE:
			return "Protocol wrong type for socket";
		case XE_EMSGSIZE:
			return "Message too long";
		case XE_ENOTSOCK:
			return "Socket operation on non-socket";
		case XE_EALREADY:
			return "Operation already in progress";
		case XE_EINPROGRESS:
			return "Operation now in progress";
		case XE_EAGAIN:
			return "Resource temporarily unavailable";
		case XE_ERANGE:
			return "Numerical result out of range";
		case XE_EDOM:
			return "Numerical argument out of domain";
		case XE_EPIPE:
			return "Broken pipe";
		case XE_EMLINK:
			return "Too many links";
		case XE_EROFS:
			return "Read-only file system";
		case XE_ESPIPE:
			return "Illegal seek";
		case XE_ENOSPC:
			return "No space left on device";
		case XE_EFBIG:
			return "File too large";
		case XE_ETXTBSY:
			return "Text file busy";
		case XE_ENOTTY:
			return "Inappropriate ioctl for device";
		case XE_ENFILE:
			return "Too many open files in system";
		case XE_EMFILE:
			return "Too many open files";
		case XE_EINVAL:
			return "Invalid argument";
		case XE_EISDIR:
			return "Is a directory";
		case XE_ENOTDIR:
			return "Not a directory";
		case XE_ENODEV:
			return "No such device";
		case XE_EXDEV:
			return "Invalid cross-device link";
		case XE_EEXIST:
			return "File exists";
		case XE_EBUSY:
			return "Device or resource busy";
		case XE_ENOTBLK:
			return "Block device required";
		case XE_EFAULT:
			return "Bad address";
		case XE_EACCES:
			return "Permission denied";
		case XE_ENOMEM:
			return "Cannot allocate memory";
		case XE_EDEADLK:
			return "Resource deadlock avoided";
		case XE_ECHILD:
			return "No child processes";
		case XE_EBADF:
			return "Bad file descriptor";
		case XE_ENOEXEC:
			return "Exec format error";
		case XE_E2BIG:
			return "Argument list too long";
		case XE_ENXIO:
			return "No such device or address";
		case XE_EIO:
			return "Input/output error";
		case XE_EINTR:
			return "Interrupted system call";
		case XE_ESRCH:
			return "No such process";
		case XE_ENOENT:
			return "No such file or directory";
		case XE_EPERM:
			return "Operation not permitted";
		default:
			return "Unknown error";
	}
}