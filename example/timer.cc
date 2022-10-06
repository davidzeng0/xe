#include <string.h>
#include "xutil/mem.h"
#include "xutil/log.h"
#include "xe/loop.h"
#include "xe/error.h"

int timer_callback(xe_loop& loop, xe_timer& timer){
	xe_print("timer callback executed");

	return 0;
}

int main(){
	xe_loop loop;
	xe_loop_options options;
	xe_timer timer;

	int ret;

	options.entries = 8; /* sqes and cqes */

	/* init */
	ret = loop.init_options(options);

	if(ret){
		xe_print("loop init %s", xe_strerror(ret));

		return -1;
	}

	timer.callback = timer_callback;

	ret = loop.timer_ms(timer, 1000, 0, 0); /* set a timer for 1000 ms without repeating */

	if(ret){
		xe_print("loop timer_ms %s", xe_strerror(ret));

		return -1;
	}

	ret = loop.run();

	if(ret){
		xe_print("loop run %s", xe_strerror(ret));

		return -1;
	}

	loop.close();

	return 0;
}