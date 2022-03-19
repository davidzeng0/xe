#include <unistd.h>
#include <stdio.h>

int main(){
	printf("%li", sysconf(_SC_LEVEL3_CACHE_SIZE));

	return 0;
}