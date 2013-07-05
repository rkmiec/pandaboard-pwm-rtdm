#ifndef PWM_H
#define PWN_H

//pwm prescaler settings
#define TIMER_PRESC_1_1     -1                                               
#define TIMER_PRESC_1_2     0
#define TIMER_PRESC_1_4     1
#define TIMER_PRESC_1_8     2
#define TIMER_PRESC_1_16    3
#define TIMER_PRESC_1_32    4
#define TIMER_PRESC_1_64    5
#define TIMER_PRESC_1_128   6
#define TIMER_PRESC_1_256   7  

//pwm output port
#define GPIO_OUTPUT_PORT 39

//ioctl requests
#define SET_PERIOD	 	0x01
#define SET_DUTYCYCLE	0x02
#define SET_DIRECTION	0x04

#define GET_FREQUENCY 	0x11
#define GET_DUTYCYCLE	0x12
#define GET_TIMER_RATE	0x13
#define GET_VALUE		0x14
#define GET_LOAD		0x15
#define GET_DIRECTION	0x16


// opaque pointer to timer object
static struct omap_dm_timer *timer_ptr;

// the IRQ # for our gp timer
static int32_t timer_irq;

// pointer gpio object

// struct for pwm object
typedef struct pwm_data {
	uint32_t dutycycle;
	uint32_t pin;
	uint32_t timer_rate;
	uint32_t load;
} pwm_data_t;

typedef struct context_data {
	uint32_t value;
	size_t size;
} context_data_t;

static int set_pwm_period(int period);
static int set_pwm_dutycycle(int dutycycle);
static int pwm_setup_pin(uint32_t gpio_number);

#endif
