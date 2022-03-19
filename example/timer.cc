#include <string.h>
#include "xe/mem.h"
#include "xe/log.h"
#include "xe/loop.h"

void timer_callback(xe_loop& loop, xe_timer& timer){
	xe_print("timer callback executed");
}

int main(){
	xe_loop loop;
	xe_loop_options options;
	xe_timer timer;

	int ret;

	options.capacity = 8; /* sqes and cqes */

	/* init */
	ret = loop.init_options(options);

	if(ret){
		xe_print("loop_init %s", strerror(-ret));

		return -1;
	}

	timer.callback = timer_callback;

	loop.timer_ms(timer, 1000, false); /* set a timer for 1000 ms without repeating */

	ret = loop.run();

	if(ret){
		xe_print("loop_run %s", strerror(-ret));

		return -1;
	}

	loop.destroy();

	return 0;
}