#include <unistd.h>
#include <xe/loop.h>
#include <xe/io/file.h>
#include "coroutine.h"

static task run(xe_loop& loop){
	xe_file file(loop);
	byte data[16384];
	long offset = 0;
	int result;

	co_await file.open("../example/sample.txt", O_RDONLY);

	while(true){
		result = co_await file.read(data, sizeof(data), offset);

		if(result <= 0)
			break;
		offset += result;

		co_await loop.run(xe_op::write(STDOUT_FILENO, data, result, 0));
	}

	file.close();
}

int main(){
	xe_loop loop;

	/* init */
	loop.init(8);

	run(loop);

	loop.run();

	/* cleanup */
	loop.close();

	return 0;
}