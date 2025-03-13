#include <unistd.h>
#include <xurl/ctx.h>
#include <xurl/request.h>
#include <xe/loop.h>
#include <xe/error.h>
#include <xutil/log.h>

using namespace xurl;

static int response_cb(xe_request& request, xe_http_response& response){
	fprintf(stderr,
		"HTTP/%i.%i %i %s\n",
		response.version / 10, response.version % 10, /* version */
		response.status,
		response.status_text.c_str() /* reason */
	);

	for(auto& pair : response.headers){
		for(auto& value : pair.second){
			fprintf(stderr,
				"%s: %s\n",
				pair.first.c_str(), /* key */
				value.c_str() /* value */
			);
		}
	}

	fprintf(stderr, "\n");

	auto etag = response.headers.find("etag");

	if(etag != response.headers.end()){
		fprintf(stderr, "found etag header: %s\n", etag -> second[0].c_str());
	}

	return 0;
}

static int write_cb(xe_request& request, xe_ptr data, size_t len){
	write(STDOUT_FILENO, data, len);

	return 0;
}

static void done_cb(xe_request& request, int status){
	fprintf(stderr, "\nrequest completed with status: %s\n", xe_strerror(status));
}

int main(){
	using namespace std::chrono_literals;

	xe_loop loop;
	xurl_shared shared;
	xurl_ctx ctx;
	xe_request request;

	/* build in debug mode for verbose strings */
	xe_log_set_level(XE_LOG_DEBUG);

	/* init */
	xurl_init();

	loop.init(256);
	shared.init();
	ctx.init(loop, shared);

	/* open an http request */
	ctx.open(request, "https://www.example.com");

	request.set_connect_timeout(10s); /* timeout connect after 10s */
	request.set_follow_location(true);

	request.set_http_response_cb(response_cb);
	request.set_write_cb(write_cb);
	request.set_done_cb(done_cb);

	/* send it */
	ctx.start(request);

	/* run event loop */
	loop.run();

	/* cleanup */
	shared.close();
	ctx.close();
	loop.close();

	xurl_cleanup();

	return 0;
}