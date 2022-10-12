#include <wolfssl/options.h>
#include <wolfssl/ssl.h>
#include <wolfssl/error-ssl.h>
#include "xconfig/ssl.h"
#include "xutil/log.h"
#include "xstd/fla.h"
#include "xe/error.h"
#include "ssl.h"

using namespace xurl;

int xe_ssl_ctx::init(){
	WOLFSSL_CTX* ctx = wolfSSL_CTX_new(wolfTLS_client_method());

	if(!ctx)
		return XE_ENOMEM;
	wolfSSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, null);

	data = ctx;

	return 0;
}

int xe_ssl_ctx::load_default_verify_locations(){
	return load_verify_locations(XE_SSL_CAFILE, XE_SSL_CAPATH);
}

int xe_ssl_ctx::load_verify_locations(xe_cstr cafile, xe_cstr capath){
	WOLFSSL_CTX* ctx = (WOLFSSL_CTX*)data;

	if(wolfSSL_CTX_load_verify_locations(ctx, cafile, capath) != WOLFSSL_SUCCESS)
		return XE_SSL_BADCERTS;
	xe_log_trace(this, "loaded cafile: %s", cafile);
	xe_log_trace(this, "loaded capath: %s", capath);

	return 0;
}

void xe_ssl_ctx::close(){
	wolfSSL_CTX_free((WOLFSSL_CTX*)data);
}

xe_cstr xe_ssl_ctx::class_name(){
	return "xe_ssl_ctx";
}

static xe_cstr ssl_version_string(int version){
	switch(version){
		case SSL2_VERSION:
			return "SSLv2.0";
		case SSL3_VERSION:
			return "SSLv3.0";
		case TLS1_VERSION:
			return "TLSv1.0";
		case TLS1_1_VERSION:
			return "TLSv1.1";
		case TLS1_2_VERSION:
			return "TLSv1.2";
		case TLS1_3_VERSION:
			return "TLSv1.3";
		case DTLS1_VERSION:
			return "DTLS 1.0";
		case DTLS1_2_VERSION:
			return "DTLS 1.2";
	}

	return "Unknown Version";
}

static xe_cstr ssl_alert_level(byte v){
	switch(v){
		case alert_warning:
			return " Warning";
		case alert_fatal:
			return " Fatal";
	}

	return " Unknown";
}

static xe_cstr ssl_alert_string(byte v){
	switch(v){
		case close_notify:
			return " Close Notify";
		case unexpected_message:
			return " Unexpected Message";
		case bad_record_mac:
			return " Bad Record Mac";
		case record_overflow:
			return " Record Overflow";
		case decompression_failure:
			return " Decompression Failure";
		case handshake_failure:
			return " Handshake Failure";
		case bad_certificate:
			return " Bad Certificate";
		case unsupported_certificate:
			return " Unsupported Certificate";
		case certificate_revoked:
			return " Certificate Revoked";
		case certificate_expired:
			return " Certificate Expired";
		case certificate_unknown:
			return " Certificate Unknown";
		case illegal_parameter:
			return " Illegal Parameter";
		case unknown_ca:
			return " Unknown CA";
		case access_denied:
			return " Access Denied";
		case decode_error:
			return " Decode Error";
		case decrypt_error:
			return " Decrypt Error";
		case protocol_version:
			return " Protocol Version";
		case insufficient_security:
			return " Insufficient Security";
		case internal_error:
			return " Internal Error";
		case inappropriate_fallback:
			return " Inappropriate Fallback";
		case user_canceled:
			return " User Canceled";
		case no_renegotiation:
			return " No Renegotiation";
		case missing_extension:
			return " Missing Extension";
		case unsupported_extension:
			return " Unsupported Extension";
		case unrecognized_name:
			return " Unrecognized Name";
		case bad_certificate_status_response:
			return " Bad Certificate Status Response";
		case unknown_psk_identity:
			return " Unknown PSK Identity";
		case certificate_required:
			return " Certificate Required";
		case no_application_protocol:
			return " No Application Protocol";
	}

	return " Unknown";
}

static xe_cstr ssl_handshake_string(byte v){
	switch(v){
		case 0:
			return " Hello Request";
		case 1:
			return " Client Hello";
		case 2:
			return " Server Hello";
		case 4:
			return " Newsession Ticket";
		case 5:
			return " End of Early Data";
		case 8:
			return " Encrypted Extensions";
		case 11:
			return " Certificate";
		case 12:
			return " Server Key Exchange";
		case 13:
			return " Certificate Request";
		case 14:
			return " Server Hello Done";
		case 15:
			return " Certificate Verify";
		case 16:
			return " Client Key Exchange";
		case 20:
			return " Finished";
		case 21:
			return " Certificate URL";
		case 22:
			return " Certificate Status";
		case 23:
			return " Supplemental Data";
		case 24:
			return " Key Update";
		case 67:
			return " Next Proto";
		case 254:
			return " Message Hash";
	}

	return " Unknown";
}

static void ssl_msg_callback(int direction, int version, int content_type, xe_cptr buf, size_t len, WOLFSSL* wssl, xe_ptr user){
	xe_ssl& ssl = *(xe_ssl*)user;
	byte* data = (byte*)buf;
	xe_cstr type, alert_level, details;

	alert_level = "";
	details = "";

	switch(content_type){
		case SSL3_RT_CHANGE_CIPHER_SPEC:
			type = "Change Cipher Spec";

			break;
		case SSL3_RT_ALERT:
			type = "Alert";

			if(len == 2){
				alert_level = ssl_alert_level(data[0]);
				details = ssl_alert_string(data[1]);
			}

			break;
		case SSL3_RT_HANDSHAKE:
			type = "Handshake";

			if(len > 0)
				details = ssl_handshake_string(data[0]);
			break;
		case SSL3_RT_APPLICATION_DATA:
			type = "Application Data";

			break;
		default:
			type = "Unknown Type";

			break;
	}

	xe_log_trace(&ssl, "%s %s %s%s%s (%zu)", direction ? "<<" : ">>", ssl_version_string(version), type, alert_level, details, len);
}

int xe_ssl::init(const xe_ssl_ctx& ctx){
	WOLFSSL* ssl = wolfSSL_new((WOLFSSL_CTX*)ctx.data);

	if(!ssl)
		return XE_ENOMEM;
	data = ssl;

#ifdef XE_DEBUG
	wolfSSL_set_msg_callback(ssl, ssl_msg_callback);
	wolfSSL_set_msg_callback_arg(ssl, this);
#endif

	return 0;
}

void xe_ssl::close(){
	wolfSSL_free((WOLFSSL*)data);
}

void xe_ssl::set_fd(int fd){
	wolfSSL_set_fd((WOLFSSL*)data, fd);
}

int xe_ssl::verify_host(const xe_string_view& host){
	WOLFSSL* ssl = (WOLFSSL*)data;
	xe_string host_copy;

	wolfSSL_set_verify(ssl, SSL_VERIFY_PEER, null);

	if(wolfSSL_UseSNI(ssl, WOLFSSL_SNI_HOST_NAME, host.data(), host.length()) != WOLFSSL_SUCCESS)
		return XE_ENOMEM;
	if(!host_copy.copy(host))
		return XE_ENOMEM;
	if(wolfSSL_check_domain_name(ssl, host_copy.c_str()) != WOLFSSL_SUCCESS)
		return XE_ENOMEM;
	return 0;
}

int xe_ssl::set_alpn(const xe_string_view& protocols){
	WOLFSSL* ssl = (WOLFSSL*)data;

	if(wolfSSL_UseALPN(ssl, (char*)protocols.data(), protocols.length(), WOLFSSL_ALPN_CONTINUE_ON_MISMATCH) != WOLFSSL_SUCCESS)
		return XE_ENOMEM;
	return 0;
}

static void ssl_connected(xe_ssl& wrapper, WOLFSSL* ssl){
#ifndef XE_DEBUG
	return;
#endif
	WOLFSSL_X509* cert;
	xe_fla<char, 2048> buffer;
	char* out;

	xe_log_verbose(&wrapper, "connected using %s / %s", wolfSSL_get_version(ssl), wolfSSL_get_cipher(ssl));

	cert = wolfSSL_get_peer_certificate(ssl);

	if(!cert){
		xe_log_verbose(&wrapper, "no peer certificate");

		return;
	}

	xe_log_verbose(&wrapper, "certificate:");

	out = wolfSSL_X509_NAME_oneline(wolfSSL_X509_get_subject_name(cert), buffer.data(), buffer.size());

	if(out)
		xe_log_verbose(&wrapper, "    subject: %s", out);
	out = wolfSSL_X509_NAME_oneline(wolfSSL_X509_get_issuer_name(cert), buffer.data(), buffer.size());

	if(out)
		xe_log_verbose(&wrapper, "    issuer : %s", out);
	out = wolfSSL_ASN1_TIME_to_string(wolfSSL_X509_get_notBefore(cert), buffer.data(), buffer.size());

	if(out)
		xe_log_verbose(&wrapper, "    start date : %s", out);
	out = wolfSSL_ASN1_TIME_to_string(wolfSSL_X509_get_notAfter(cert), buffer.data(), buffer.size());

	if(out)
		xe_log_verbose(&wrapper, "    expire date: %s", out);
	while((out = wolfSSL_X509_get_next_altname(cert)))
		xe_log_verbose(&wrapper, "    alt name: %s", out);
	wolfSSL_X509_free(cert);
}

int xe_ssl::connect(int flags){
	WOLFSSL* ssl = (WOLFSSL*)data;
	int err;

	wolfSSL_SetIOReadFlags(ssl, flags);
	wolfSSL_SetIOWriteFlags(ssl, flags);

	if(wolfSSL_connect(ssl) == WOLFSSL_SUCCESS){
		ssl_connected(*this, ssl);

		return 0;
	}

	switch(wolfSSL_get_error(ssl, -1)){
		case WOLFSSL_ERROR_WANT_READ:
		case WOLFSSL_ERROR_WANT_WRITE:
			return XE_EAGAIN;
		case DOMAIN_NAME_MISMATCH:
			return XE_SSL_PEER_VERIFICATION_FAILED;
		case ASN_NO_SIGNER_E:
			return XE_SSL_NO_SIGNER;
		case SOCKET_ERROR_E:
			err = xe_errno();

			return err ?: XE_RECV_ERROR;
	}

	return XE_SSL;
}

int xe_ssl::get_alpn_protocol(xe_string_view& proto){
	WOLFSSL* ssl = (WOLFSSL*)data;

	char* name;
	ushort size;

	switch(wolfSSL_ALPN_GetProtocol(ssl, &name, &size)){
		case WOLFSSL_SUCCESS:
			proto = xe_string_view(name, size);

			return 0;
		case WOLFSSL_ALPN_NOT_FOUND:
			return XE_SSL;
		default:
			return XE_SSL;
	}
}

int xe_ssl::recv(xe_ptr buffer, size_t len, int flags){
	WOLFSSL* ssl = (WOLFSSL*)data;
	int recv;

	wolfSSL_SetIOReadFlags(ssl, flags);

	recv = wolfSSL_read(ssl, buffer, xe_min<size_t>(len, xe_max_value<int>()));

	if(recv >= 0)
		return recv;
	switch(wolfSSL_get_error(ssl, -1)){
		case WOLFSSL_ERROR_WANT_READ:
		case WOLFSSL_ERROR_WANT_WRITE:
			return XE_EAGAIN;
		case SOCKET_ERROR_E:
			return xe_errno();
	}

	return XE_SSL;
}

int xe_ssl::send(xe_cptr buffer, size_t len, int flags){
	WOLFSSL* ssl = (WOLFSSL*)data;
	int sent;

	wolfSSL_SetIOWriteFlags(ssl, flags);

	sent = wolfSSL_write(ssl, buffer, xe_min<size_t>(len, xe_max_value<int>()));

	if(sent >= 0)
		return sent;
	switch(wolfSSL_get_error(ssl, -1)){
		case WOLFSSL_ERROR_WANT_READ:
		case WOLFSSL_ERROR_WANT_WRITE:
			return XE_EAGAIN;
		case SOCKET_ERROR_E:
			return xe_errno();
	}

	return XE_SSL;
}

xe_cstr xe_ssl::class_name(){
	return "xe_ssl";
}

int xurl::xe_ssl_init(){
	return wolfSSL_Init() != WOLFSSL_SUCCESS ? XE_SSL : 0;
}

void xurl::xe_ssl_cleanup(){
	wolfSSL_Cleanup();
}