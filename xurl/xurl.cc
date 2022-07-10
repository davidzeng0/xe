#include "xurl.h"
#include "ssl.h"

int xurl::xurl_init(){
	return xe_ssl_init();
}

void xurl::xurl_cleanup(){
	xe_ssl_cleanup();
}