#include "board.h"
#include "imath.h"
#include "periph/pwm.h"
#include "ztimer/periodic.h"

#define OSC_INTERVAL    (250) /* 0.25 ms */
#define OSC_STEP        (10)
#define OSC_MODE        PWM_LEFT
#define OSC_FREQU       (1000U)
#define OSC_STEPS       (ISIN_MAX - ISIN_MIN)

static bool _cb(void *arg)
{
    unsigned *state = arg;
    unsigned val = isin(*state & ISIN_PERIOD) - ISIN_MIN;

    pwm_set(PWM_DEV(1), 2, val);
    *state += OSC_STEP;

    return true;
}

void blinky_start(void)
{
    static ztimer_periodic_t timer;
    static unsigned state;

    pwm_init(PWM_DEV(1), OSC_MODE, OSC_FREQU, OSC_STEPS);
    ztimer_periodic_init(ZTIMER_USEC, &timer, _cb, &state, OSC_INTERVAL);
    ztimer_periodic_start(&timer);
}
