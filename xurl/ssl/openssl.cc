#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sys/socket.h>
#include "xconfig/ssl.h"
#include "xstd/fla.h"
#include "xutil/log.h"
#include "xe/error.h"
#include "../ssl_common.h"

using namespace xurl;

static BIO_METHOD* method;

static int sock_write_ex(BIO* bio, const char* data, size_t len, size_t* written){
	int fd, flags;
	ssize_t ret;

	BIO_get_fd(bio, &fd);

	flags = (int)(long)BIO_get_data(bio);
	len = xe_min<size_t>(len, xe_max_value<int>());
	ret = send(fd, data, len, flags);

	BIO_clear_retry_flags(bio);

	if(ret <= 0){
		if(BIO_sock_should_retry(ret))
			BIO_set_retry_write(bio);
		*written = 0;

		return ret;
	}

	*written = ret;

	return 1;
}

static int sock_write(BIO* bio, const char* buf, int len){
	size_t written;
	int res = sock_write_ex(bio, buf, len, &written);

	if(res <= 0)
		return res;
	return written;
}

static int sock_read_ex(BIO* bio, char* buf, size_t len, size_t* read){
	if(!buf)
		return 0;
	int fd, flags;
	ssize_t ret;

	BIO_get_fd(bio, &fd);

	flags = (int)(long)BIO_get_data(bio);
	len = xe_min<size_t>(len, xe_max_value<int>());
	ret = recv(fd, buf, len, flags);

	BIO_clear_retry_flags(bio);

	if(ret <= 0){
		if(BIO_sock_should_retry(ret))
			BIO_set_retry_read(bio);
		else if(ret == 0)
			BIO_set_flags(bio, BIO_FLAGS_IN_EOF);
		*read = 0;

		return ret;
	}

	*read = ret;

	return 1;
}

static int sock_read(BIO* bio, char* buf, int len){
	size_t read;
	int res = sock_read_ex(bio, buf, len, &read);

	if(res <= 0)
		return res;
	return read;
}

int xe_ssl_ctx::init(){
	SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());

	if(!ctx)
		return XE_ENOMEM;
	SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, null);

	data = ctx;

	return 0;
}

int xe_ssl_ctx::load_default_verify_locations(){
	return load_verify_locations(XE_SSL_CAFILE, XE_SSL_CAPATH);
}

int xe_ssl_ctx::load_verify_locations(xe_cstr cafile, xe_cstr capath){
	SSL_CTX* ctx = (SSL_CTX*)data;

#if OPENSSL_VERSION_MAJOR >= 3
	if(cafile && !SSL_CTX_load_verify_file(ctx, cafile))
		return XE_SSL_BADCERTS;
	if(capath && !SSL_CTX_load_verify_dir(ctx, capath))
		return XE_SSL_BADCERTS;
#else
	if(!SSL_CTX_load_verify_locations(ctx, cafile, capath))
		return XE_SSL_BADCERTS;
#endif
	xe_log_trace(this, "loaded cafile: %s", cafile);
	xe_log_trace(this, "loaded capath: %s", capath);

	return 0;
}

void xe_ssl_ctx::close(){
	SSL_CTX_free((SSL_CTX*)data);
}

static void ssl_msg_callback(int direction, int version, int content_type, xe_cptr buf, size_t len, SSL* ssl, xe_ptr user){
	xe_ssl_msg_callback(direction, version, content_type, buf, len, user);
}

int xe_ssl::init(const xe_ssl_ctx& ctx){
	SSL* ssl = SSL_new((SSL_CTX*)ctx.data);

	if(!ssl)
		return XE_ENOMEM;
	data = ssl;

#ifdef XE_DEBUG
	SSL_set_msg_callback(ssl, ssl_msg_callback);
	SSL_set_msg_callback_arg(ssl, this);
#else
	(void)ssl_msg_callback;
#endif

	return 0;
}

void xe_ssl::close(){
	SSL_free((SSL*)data);
}

int xe_ssl::preconnect(int fd){
	SSL* ssl = (SSL*)data;
	BIO* bio = BIO_new(method);

	if(!bio)
		return XE_ENOMEM;
	BIO_set_fd(bio, fd, BIO_NOCLOSE);
	SSL_set_bio(ssl, bio, bio);
	SSL_set_connect_state(ssl);

	return 0;
}

int xe_ssl::verify_host(const xe_string& host){
	SSL* ssl = (SSL*)data;

	SSL_set_verify(ssl, SSL_VERIFY_PEER, null);
	SSL_set_hostflags(ssl, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);

	if(!SSL_set_tlsext_host_name(ssl, host.c_str()))
		return XE_ENOMEM;
	if(!SSL_set1_host(ssl, host.c_str()))
		return XE_ENOMEM;
	return 0;
}

int xe_ssl::set_alpn(const xe_string_view& protocols){
	SSL* ssl = (SSL*)data;

	if(SSL_set_alpn_protos(ssl, (byte*)protocols.data(), protocols.size()))
		return XE_ENOMEM;
	return 0;
}

static void ssl_connected(xe_ssl& wrapper, SSL* ssl){
#ifndef XE_DEBUG
	return;
#endif
	xe_fla<char, 2048> buffer;
	X509* cert;
	char* out;
	BIO* mem;
	stack_st_GENERAL_NAME* alts;
	int alt_len;

	mem = BIO_new(BIO_s_mem());

	if(!mem)
		return;
	xe_log_verbose(&wrapper, "connected using %s / %s", SSL_get_version(ssl), SSL_get_cipher(ssl));

	cert = SSL_get1_peer_certificate(ssl);

	if(!cert){
		xe_log_verbose(&wrapper, "no peer certificate");

		return;
	}

	xe_log_verbose(&wrapper, "certificate:");

	out = X509_NAME_oneline(X509_get_subject_name(cert), buffer.data(), buffer.size());

	if(out)
		xe_log_verbose(&wrapper, "    subject: %s", out);
	out = X509_NAME_oneline(X509_get_issuer_name(cert), buffer.data(), buffer.size());

	if(out)
		xe_log_verbose(&wrapper, "    issuer : %s", out);
	if(ASN1_TIME_print(mem, X509_get0_notBefore(cert))){
		BIO_get_mem_data(mem, &out);
		xe_log_verbose(&wrapper, "    start date : %s", out);
		BIO_reset(mem);
	}

	if(ASN1_TIME_print(mem, X509_get0_notAfter(cert))){
		BIO_get_mem_data(mem, &out);
		xe_log_verbose(&wrapper, "    expire date: %s", out);
		BIO_reset(mem);
	}

	alts = (stack_st_GENERAL_NAME*)X509_get_ext_d2i(cert, NID_subject_alt_name, null, null);
	alt_len = sk_GENERAL_NAME_num(alts);

	for(int i = 0; i < alt_len; i++){
		xe_log_verbose(&wrapper, "    alt name: %s",
			ASN1_STRING_get0_data(sk_GENERAL_NAME_value(alts, i) -> d.ia5)
		);
	}

	X509_free(cert);
	BIO_free(mem);
}

int xe_ssl::connect(int flags){
	SSL* ssl = (SSL*)data;
	int err;

	ERR_clear_error();
	BIO_set_data(SSL_get_wbio(ssl), (xe_ptr)(long)flags);

	err = SSL_connect(ssl);

	if(err == 1){
		if(SSL_get_verify_result(ssl) != X509_V_OK)
			return XE_SSL_PEER_VERIFICATION_FAILED;
		ssl_connected(*this, ssl);

		return 0;
	}

	switch(SSL_get_error(ssl, err)){
		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_WRITE:
			return XE_EAGAIN;
		case SSL_ERROR_SYSCALL:
			err = xe_errno();

			return err ?: XE_RECV_ERROR;
	}

	return XE_SSL;
}

int xe_ssl::get_alpn_protocol(xe_string_view& proto){
	SSL* ssl = (SSL*)data;

	const byte* name;
	uint size;

	SSL_get0_alpn_selected(ssl, &name, &size);

	if(!name)
		return XE_SSL_NO_ALPN;
	proto = xe_string_view((char*)name, size);

	return 0;
}

int xe_ssl::recv(xe_ptr buffer, size_t len, int flags){
	SSL* ssl = (SSL*)data;
	int recv;

	ERR_clear_error();
	BIO_set_data(SSL_get_rbio(ssl), (xe_ptr)(long)flags);

	recv = SSL_read(ssl, buffer, xe_min<size_t>(len, xe_max_value<int>()));

	if(recv >= 0)
		return recv;
	switch(SSL_get_error(ssl, recv)){
		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_WRITE:
			return XE_EAGAIN;
		case SSL_ERROR_SYSCALL:
			return xe_errno();
	}

	return XE_SSL;
}

int xe_ssl::send(xe_cptr buffer, size_t len, int flags){
	SSL* ssl = (SSL*)data;
	int sent;

	ERR_clear_error();
	BIO_set_data(SSL_get_wbio(ssl), (xe_ptr)(long)flags);

	sent = SSL_write(ssl, buffer, xe_min<size_t>(len, xe_max_value<int>()));

	if(sent >= 0)
		return sent;
	switch(SSL_get_error(ssl, sent)){
		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_WRITE:
			return XE_EAGAIN;
		case SSL_ERROR_SYSCALL:
			return xe_errno();
	}

	return XE_SSL;
}

int xe_crypto::sha1(const byte* in, size_t inlen, byte* out, size_t outlen){
	EVP_MD_CTX* ctx = EVP_MD_CTX_new();
	uint len;
	bool success;

	if(outlen < SHA_DIGEST_LENGTH)
		return XE_EINVAL;
	if(!ctx)
		return XE_ENOMEM;
	success = EVP_DigestInit(ctx, EVP_sha1()) &&
		EVP_DigestUpdate(ctx, in, inlen) &&
		EVP_DigestFinal(ctx, out, &len);
	EVP_MD_CTX_free(ctx);

	return success ? 0 : XE_FATAL;
}

int xurl::xe_ssl_init(){
	const BIO_METHOD* socket;

	if(!OPENSSL_init_ssl(0, null))
		return XE_SSL;
	method = BIO_meth_new(BIO_TYPE_SOCKET, "socket");

	if(!method){
		xe_ssl_cleanup();

		return XE_ENOMEM;
	}

	socket = BIO_s_socket();

	BIO_meth_set_write(method, sock_write);
	BIO_meth_set_write_ex(method, sock_write_ex);
	BIO_meth_set_read(method, sock_read);
	BIO_meth_set_read_ex(method, sock_read_ex);
	BIO_meth_set_gets(method, BIO_meth_get_gets(socket));
	BIO_meth_set_puts(method, BIO_meth_get_puts(socket));
	BIO_meth_set_ctrl(method, BIO_meth_get_ctrl(socket));
	BIO_meth_set_create(method, BIO_meth_get_create(socket));
	BIO_meth_set_destroy(method, BIO_meth_get_destroy(socket));
	BIO_meth_set_callback_ctrl(method, BIO_meth_get_callback_ctrl(socket));

	return 0;
}

void xurl::xe_ssl_cleanup(){
	if(method)
		BIO_meth_free(method);
	EVP_cleanup();
	ERR_free_strings();
}