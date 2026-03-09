#include "runner.h"

#include <stdio.h>
#include <string.h>

int main(void) {
    struct Context ctx;
	memset(&ctx, 0, sizeof(ctx));

	while(runner(&ctx) == 1){
		printf("Runner going back to sleep\n");
	}

    return 0;
}
