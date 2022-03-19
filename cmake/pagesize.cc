#include <unistd.h>
#include <stdio.h>

int main(){
	printf("%li", sysconf(_SC_PAGESIZE));

	return 0;
}