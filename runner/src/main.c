#include "fsm.h"

int main() {
    Context ctx = {0};
    ctx.state = state_init;
    int run = 1;

    ctx.state(&ctx, EV_START);

    while(run){
        Event e = wait_next_event(&ctx);
        if (e == EV_SHUTDOWN) return 0;
        ctx.state(&ctx, e);
    }

    return 0;
}
