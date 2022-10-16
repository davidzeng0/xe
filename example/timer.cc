#include <xe/loop.h>
#include <xutil/log.h>

int main(){
	using namespace std::chrono_literals;

	xe_loop loop;
	xe_timer timer;

	/* init */
	loop.init(8); /* 8 sqes and cqes */

	timer.callback = [](xe_loop& loop, xe_timer& timer){
		xe_print("timer callback executed");

		return 0;
	};

	/* set a timer for 1000 ms without repeating */
	loop.timer(timer, 1000ms, 0s, 0);

	xe_print("timer queued");

	loop.run();

	/* cleanup */
	loop.close();

	return 0;
}