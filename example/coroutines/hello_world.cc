#include <unistd.h>
#include <xe/loop.h>
#include <xe/error.h>
#include <xutil/log.h>
#include "coroutine.h"

static void handle_error(xe_cstr where, int error){
	if(error < 0){
		xe_print("%s: %s", where, xe_strerror(error));
		exit(EXIT_FAILURE);
	}
}

static task run(xe_loop& loop){
	char msg[] = "Hello World!\n";

	co_await loop.run(xe_op::write(STDOUT_FILENO, msg, sizeof(msg) - 1, 0));
}

int main(){
	xe_loop loop;
	int err;

	/* init */
	err = loop.init(8); /* 8 sqes */

	handle_error("init", err);

	run(loop);

	err = loop.run();

	handle_error("run", err);

	/* close */
	loop.close();

	return 0;
}