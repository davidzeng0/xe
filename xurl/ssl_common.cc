#include "xutil/log.h"
#include "ssl_common.h"

using namespace xurl;

xe_cstr xe_ssl_ctx::class_name(){
	return "xe_ssl_ctx";
}

xe_cstr xe_ssl::class_name(){
	return "xe_ssl";
}

enum xe_ssl_version{
	XE_VERSION_SSL2 = 0x0002,
	XE_VERSION_SSL3 = 0x0300,
	XE_VERSION_TLS1 = 0x0301,
	XE_VERSION_TLS1_1 = 0x0302,
	XE_VERSION_TLS1_2 = 0x0303,
	XE_VERSION_TLS1_3 = 0x0304,
	XE_VERSION_DTLS1 = 0xFEFF,
	XE_VERSION_DTLS1_2 = 0xFEFD
};

enum xe_ssl_alert_type{
	XE_ALERT_WARNING = 0x1,
	XE_ALERT_FATAL = 0x2
};

enum xe_ssl_alerts{
	XE_ALERT_CLOSE_NOTIFY = 0,
	XE_ALERT_UNEXPECTED_MESSAGE = 10,
	XE_ALERT_BAD_RECORD_MAC = 20,
	XE_ALERT_RECORD_OVERFLOW = 22,
	XE_ALERT_DECOMPRESSION_FAILURE = 30,
	XE_ALERT_HANDSHAKE_FAILURE = 40,
	XE_ALERT_NO_CERTIFICATE = 41,
	XE_ALERT_BAD_CERTIFICATE = 42,
	XE_ALERT_UNSUPPORTED_CERTIFICATE = 43,
	XE_ALERT_CERTIFICATE_REVOKED = 44,
	XE_ALERT_CERTIFICATE_EXPIRED = 45,
	XE_ALERT_CERTIFICATE_UNKNOWN = 46,
	XE_ALERT_ILLEGAL_PARAMETER = 47,
	XE_ALERT_UNKNOWN_CA = 48,
	XE_ALERT_ACCESS_DENIED = 49,
	XE_ALERT_DECODE_ERROR = 50,
	XE_ALERT_DECRYPT_ERROR = 51,
	XE_ALERT_WC_PROTOCOL_VERSION = 70,
	XE_ALERT_PROTOCOL_VERSION = 70,
	XE_ALERT_INSUFFICIENT_SECURITY = 71,
	XE_ALERT_INTERNAL_ERROR = 80,
	XE_ALERT_INAPPROPRIATE_FALLBACK = 86,
	XE_ALERT_USER_CANCELED = 90,
	XE_ALERT_NO_RENEGOTIATION = 100,
	XE_ALERT_MISSING_EXTENSION = 109,
	XE_ALERT_UNSUPPORTED_EXTENSION = 110,
	XE_ALERT_UNRECOGNIZED_NAME = 112,
	XE_ALERT_BAD_CERTIFICATE_STATUS_RESPONSE = 113,
	XE_ALERT_UNKNOWN_PSK_IDENTITY = 115,
	XE_ALERT_CERTIFICATE_REQUIRED = 116,
	XE_ALERT_NO_APPLICATION_PROTOCOL = 120
};

enum xe_ssl_handshake_type{
	XE_HANDSHAKE_HELLO_REQUEST = 0,
	XE_HANDSHAKE_CLIENT_HELLO = 1,
	XE_HANDSHAKE_SERVER_HELLO = 2,
	XE_HANDSHAKE_HELLO_VERIFY_REQUEST = 3,
	XE_HANDSHAKE_NEWSESSION_TICKET = 4,
	XE_HANDSHAKE_END_OF_EARLY_DATA = 5,
	XE_HANDSHAKE_ENCRYPTED_EXTENSIONS = 8,
	XE_HANDSHAKE_CERTIFICATE = 11,
	XE_HANDSHAKE_SERVER_KEY_EXCHANGE = 12,
	XE_HANDSHAKE_CERTIFICATE_REQUEST = 13,
	XE_HANDSHAKE_SERVER_HELLO_DONE = 14,
	XE_HANDSHAKE_CERTIFICATE_VERIFY = 15,
	XE_HANDSHAKE_CLIENT_KEY_EXCHANGE = 16,
	XE_HANDSHAKE_FINISHED = 20,
	XE_HANDSHAKE_CERTIFICATE_URL = 21,
	XE_HANDSHAKE_CERTIFICATE_STATUS = 22,
	XE_HANDSHAKE_SUPPLEMENTAL_DATA = 23,
	XE_HANDSHAKE_KEY_UPDATE = 24,
	XE_HANDSHAKE_NEXT_PROTO = 67,
	XE_HANDSHAKE_MESSAGE_HASH = 254
};

enum xe_ssl_message_types{
	XE_SSL_CHANGE_CIPHER_SPEC = 20,
	XE_SSL_ALERT = 21,
	XE_SSL_HANDSHAKE = 22,
	XE_SSL_APPLICATION_DATA = 23,
	XE_SSL_HEADER = 256,
	XE_SSL_INNER_CONTENT = 257
};

static xe_cstr ssl_version_string(int version){
	switch(version){
		case XE_VERSION_SSL2:
			return "SSLv2.0";
		case XE_VERSION_SSL3:
			return "SSLv3.0";
		case XE_VERSION_TLS1:
			return "TLSv1.0";
		case XE_VERSION_TLS1_1:
			return "TLSv1.1";
		case XE_VERSION_TLS1_2:
			return "TLSv1.2";
		case XE_VERSION_TLS1_3:
			return "TLSv1.3";
		case XE_VERSION_DTLS1:
			return "DTLS 1.0";
		case XE_VERSION_DTLS1_2:
			return "DTLS 1.2";
	}

	return "Unknown Version";
}

static xe_cstr ssl_alert_level(byte v){
	switch(v){
		case XE_ALERT_WARNING:
			return " Warning";
		case XE_ALERT_FATAL:
			return " Fatal";
	}

	return " Unknown";
}

static xe_cstr ssl_alert_string(byte v){
	switch(v){
		case XE_ALERT_CLOSE_NOTIFY:
			return " Close Notify";
		case XE_ALERT_UNEXPECTED_MESSAGE:
			return " Unexpected Message";
		case XE_ALERT_BAD_RECORD_MAC:
			return " Bad Record Mac";
		case XE_ALERT_RECORD_OVERFLOW:
			return " Record Overflow";
		case XE_ALERT_DECOMPRESSION_FAILURE:
			return " Decompression Failure";
		case XE_ALERT_HANDSHAKE_FAILURE:
			return " Handshake Failure";
		case XE_ALERT_NO_CERTIFICATE:
			return " No certificate";
		case XE_ALERT_BAD_CERTIFICATE:
			return " Bad Certificate";
		case XE_ALERT_UNSUPPORTED_CERTIFICATE:
			return " Unsupported Certificate";
		case XE_ALERT_CERTIFICATE_REVOKED:
			return " Certificate Revoked";
		case XE_ALERT_CERTIFICATE_EXPIRED:
			return " Certificate Expired";
		case XE_ALERT_CERTIFICATE_UNKNOWN:
			return " Certificate Unknown";
		case XE_ALERT_ILLEGAL_PARAMETER:
			return " Illegal Parameter";
		case XE_ALERT_UNKNOWN_CA:
			return " Unknown CA";
		case XE_ALERT_ACCESS_DENIED:
			return " Access Denied";
		case XE_ALERT_DECODE_ERROR:
			return " Decode Error";
		case XE_ALERT_DECRYPT_ERROR:
			return " Decrypt Error";
		case XE_ALERT_WC_PROTOCOL_VERSION:
			return " Protocol Version";
		case XE_ALERT_INSUFFICIENT_SECURITY:
			return " Insufficient Security";
		case XE_ALERT_INTERNAL_ERROR:
			return " Internal Error";
		case XE_ALERT_INAPPROPRIATE_FALLBACK:
			return " Inappropriate Fallback";
		case XE_ALERT_USER_CANCELED:
			return " User Canceled";
		case XE_ALERT_NO_RENEGOTIATION:
			return " No Renegotiation";
		case XE_ALERT_MISSING_EXTENSION:
			return " Missing Extension";
		case XE_ALERT_UNSUPPORTED_EXTENSION:
			return " Unsupported Extension";
		case XE_ALERT_UNRECOGNIZED_NAME:
			return " Unrecognized Name";
		case XE_ALERT_BAD_CERTIFICATE_STATUS_RESPONSE:
			return " Bad Certificate Status Response";
		case XE_ALERT_UNKNOWN_PSK_IDENTITY:
			return " Unknown PSK Identity";
		case XE_ALERT_CERTIFICATE_REQUIRED:
			return " Certificate Required";
		case XE_ALERT_NO_APPLICATION_PROTOCOL:
			return " No Application Protocol";
	}

	return " Unknown";
}

static xe_cstr ssl_handshake_string(byte v){
	switch(v){
		case XE_HANDSHAKE_HELLO_REQUEST:
			return " Hello Request";
		case XE_HANDSHAKE_CLIENT_HELLO:
			return " Client Hello";
		case XE_HANDSHAKE_SERVER_HELLO:
			return " Server Hello";
		case XE_HANDSHAKE_HELLO_VERIFY_REQUEST:
			return " Hello Verify Request";
		case XE_HANDSHAKE_NEWSESSION_TICKET:
			return " Newsession Ticket";
		case XE_HANDSHAKE_END_OF_EARLY_DATA:
			return " End of Early Data";
		case XE_HANDSHAKE_ENCRYPTED_EXTENSIONS:
			return " Encrypted Extensions";
		case XE_HANDSHAKE_CERTIFICATE:
			return " Certificate";
		case XE_HANDSHAKE_SERVER_KEY_EXCHANGE:
			return " Server Key Exchange";
		case XE_HANDSHAKE_CERTIFICATE_REQUEST:
			return " Certificate Request";
		case XE_HANDSHAKE_SERVER_HELLO_DONE:
			return " Server Hello Done";
		case XE_HANDSHAKE_CERTIFICATE_VERIFY:
			return " Certificate Verify";
		case XE_HANDSHAKE_CLIENT_KEY_EXCHANGE:
			return " Client Key Exchange";
		case XE_HANDSHAKE_FINISHED:
			return " Finished";
		case XE_HANDSHAKE_CERTIFICATE_URL:
			return " Certificate URL";
		case XE_HANDSHAKE_CERTIFICATE_STATUS:
			return " Certificate Status";
		case XE_HANDSHAKE_SUPPLEMENTAL_DATA:
			return " Supplemental Data";
		case XE_HANDSHAKE_KEY_UPDATE:
			return " Key Update";
		case XE_HANDSHAKE_NEXT_PROTO:
			return " Next Proto";
		case XE_HANDSHAKE_MESSAGE_HASH:
			return " Message Hash";
	}

	return " Unknown";
}

void xe_ssl_msg_callback(int direction, int version, int content_type, xe_cptr buf, size_t len, xe_ptr user){
	xe_ssl& ssl = *(xe_ssl*)user;
	byte* data = (byte*)buf;
	xe_cstr type, alert_level, details;

	alert_level = "";
	details = "";

	if(content_type == XE_SSL_INNER_CONTENT)
		return;
	switch(content_type){
		case XE_SSL_CHANGE_CIPHER_SPEC:
			type = "Change Cipher Spec";

			break;
		case XE_SSL_ALERT:
			type = "Alert";

			if(len == 2){
				alert_level = ssl_alert_level(data[0]);
				details = ssl_alert_string(data[1]);
			}

			break;
		case XE_SSL_HANDSHAKE:
			type = "Handshake";

			if(len > 0)
				details = ssl_handshake_string(data[0]);
			break;
		case XE_SSL_APPLICATION_DATA:
			type = "Application Data";

			break;
		case XE_SSL_HEADER:
			type = "Record Header";

			if(len > 0)
				details = ssl_handshake_string(data[0]);
			break;
		default:
			type = "Unknown Type";

			break;
	}


#ifndef XE_DEBUG
	(void)ssl;
	(void)data;
	(void)type;
	(void)alert_level;
	(void)details;
	(void)ssl_version_string;
#endif

	xe_log_trace(&ssl, "%s %s %s%s%s (%zu)", direction ? "<<" : ">>", ssl_version_string(version), type, alert_level, details, len);
}