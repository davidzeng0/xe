#include <wolfssl/options.h>
#include <wolfssl/ssl.h>
#include <wolfssl/error-ssl.h>
#include "xconfig/ssl.h"
#include "xutil/log.h"
#include "xstd/fla.h"
#include "xe/error.h"
#include "../ssl_common.h"

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

static void ssl_msg_callback(int direction, int version, int content_type, xe_cptr buf, size_t len, WOLFSSL* ssl, xe_ptr user){
	xe_ssl_msg_callback(direction, version, content_type, buf, len, user);
}

int xe_ssl::init(const xe_ssl_ctx& ctx){
	WOLFSSL* ssl = wolfSSL_new((WOLFSSL_CTX*)ctx.data);

	if(!ssl)
		return XE_ENOMEM;
	data = ssl;

#ifdef XE_DEBUG
	wolfSSL_set_msg_callback(ssl, ssl_msg_callback);
	wolfSSL_set_msg_callback_arg(ssl, this);
#else
	(void)ssl_msg_callback;
#endif

	return 0;
}

void xe_ssl::close(){
	wolfSSL_free((WOLFSSL*)data);
}

int xe_ssl::preconnect(int fd){
	wolfSSL_set_fd((WOLFSSL*)data, fd);

	return 0;
}

int xe_ssl::verify_host(const xe_string& host){
	WOLFSSL* ssl = (WOLFSSL*)data;

	wolfSSL_set_verify(ssl, SSL_VERIFY_PEER, null);

	if(wolfSSL_UseSNI(ssl, WOLFSSL_SNI_HOST_NAME, host.c_str(), host.length()) != WOLFSSL_SUCCESS)
		return XE_ENOMEM;
	if(wolfSSL_check_domain_name(ssl, host.c_str()) != WOLFSSL_SUCCESS)
		return XE_ENOMEM;
	return 0;
}

int xe_ssl::set_alpn(const xe_string_view& protocols){
	WOLFSSL* ssl = (WOLFSSL*)data;

	if(wolfSSL_UseALPN(ssl, (char*)protocols.data(), protocols.length(), WOLFSSL_ALPN_CONTINUE_ON_MISMATCH)
		!= WOLFSSL_SUCCESS)
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
			return XE_SSL_NO_ALPN;
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

int xe_crypto::sha1(const byte* in, size_t inlen, byte* out, size_t outlen){
	wc_Sha sha;

	if(outlen < SHA_DIGEST_SIZE)
		return XE_EINVAL;
	if(wc_InitSha(&sha))
		return XE_FATAL;
	if(wc_ShaUpdate(&sha, in, inlen))
		return XE_FATAL;
	if(wc_ShaFinal(&sha, out))
		return XE_FATAL;
	wc_ShaFree(&sha);

	return 0;
}

int xurl::xe_ssl_init(){
	return wolfSSL_Init() == WOLFSSL_SUCCESS ? 0 : XE_SSL;
}

void xurl::xe_ssl_cleanup(){
	wolfSSL_Cleanup();
}