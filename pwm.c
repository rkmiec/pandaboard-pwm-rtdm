#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include <plat/dmtimer.h>
#include <linux/types.h>

#include <rtdm/rtdm_driver.h>

#include "pwm.h"

//pointer to data struct
static pwm_data_t pwm_data;
//irq handler
static rtdm_irq_t irqt;
static int val;
static int debug_tab[] = {0,0};
rtdm_mutex_t write_mutex;

#define TIMER_MAX 0xFFFFFFFF

// do some kernel module documentation
MODULE_AUTHOR("Robert Kmiec <robertkmiec1989@gmail.com>");
MODULE_DESCRIPTION("Hardware PWM using GPIO");
MODULE_LICENSE("GPL");

static int rtdm_open_nrt(struct rtdm_dev_context *context,
		rtdm_user_info_t *user_info, int oflags)
{
	omap_dm_timer_start(timer_ptr);
	rtdm_printk(KERN_DEBUG "pwm module: Device opened and GP Timer started\n");

	return 0;
}

static int rtdm_close_nrt(struct rtdm_dev_context *context,
		rtdm_user_info_t *user_info)
{
	omap_dm_timer_stop(timer_ptr);
	gpio_set_value(pwm_data.pin, OFF_VALUE);
	rtdm_printk(KERN_DEBUG "pwm module: Device closed and GP Timer stopped\n");

	return 0;
}

static int rtdm_ioctl_rt(struct rtdm_dev_context *context,
		rtdm_user_info_t *user_info, unsigned int request, void __user *arg)
{
	rtdm_mutex_lock(&write_mutex);
	context_data_t *data = (context_data_t*) context->dev_private;
	data->size = sizeof(data->value);
	if (rtdm_safe_copy_from_user(user_info, &(data->value), arg, 
				data->size)) {
		rtdm_printk(KERN_WARNING "pwm: ioctl: error %p\n",arg);
		return -1;
	}
	if (rtdm_in_rt_context()){
		debug_tab[0]+=1;
		rtdm_printk("pwm ioctl: rt_context\n");
	} else {
		debug_tab[1]+=1;
		rtdm_printk("pwm ioctl: nrt context 0x%x\n",request);
	}
	switch (request) {
		case SET_PERIOD:
			set_pwm_period(data->value);
			break;
		case SET_DUTYCYCLE:
			set_pwm_dutycycle(data->value);
			break;
		case SET_DIRECTION:
			set_motor_direction(data->value);
			break;
		default: 
			rtdm_mutex_unlock(&write_mutex);
			return -1;
	}
	return 0;
}

static struct rtdm_device device = { 
	.struct_version = RTDM_DEVICE_STRUCT_VER,
	.device_flags = RTDM_NAMED_DEVICE | RTDM_EXCLUSIVE,
	//context structure size
	.context_size = sizeof(context_data_t),
	.device_name = "pwmgpio",
	.open_nrt = rtdm_open_nrt,
	.ops = { 
		.ioctl_rt = rtdm_ioctl_rt,
		//Still needed, even if not real-time (like soft-rt):
		.ioctl_nrt = rtdm_ioctl_rt,
		.close_nrt = rtdm_close_nrt,
	},  
	.device_class = RTDM_CLASS_EXPERIMENTAL,
	.device_sub_class = 4711,
	.profile_version = 1,
	.driver_name = "pwmgpio",
	.driver_version = RTDM_DRIVER_VER(0,1,2),
	.peripheral_name = "pwmgpio",
	.provider_name = "rkmiec",
	.proc_name = device.device_name,
};

static void timer_handler(void)
{
	val = omap_dm_timer_read_status(timer_ptr);
	val &= 0b111;
	// reset the timer interrupt status
	omap_dm_timer_write_status(timer_ptr,OMAP_TIMER_INT_OVERFLOW|
			OMAP_TIMER_INT_MATCH);
	//you need to do this read
	omap_dm_timer_read_status(timer_ptr);

/*	if(gpio_get_value(pwm_data.pin) == 0 ) {
		gpio_set_value(pwm_data.pin,1);
	} else {
		gpio_set_value(pwm_data.pin,0);
	}*/
	if (val == OMAP_TIMER_INT_OVERFLOW)
		gpio_set_value(pwm_data.pin, 0);
	else
		gpio_set_value(pwm_data.pin, 1);
}

//the interrupt handler
int timer_irq_handler(rtdm_irq_t *irq_handle)
{
	timer_handler();
	// tell the nucleus it's handled
	return RTDM_IRQ_HANDLED;
}

//set the pwm full cycle period in 10microseconds.
//for example: set_pwm_period(100) = 1 millisecond period
static int set_pwm_period(int period)
{
	// set preload, and autoreload of the timer
	uint32_t load = TIMER_MAX+1 - (24*period);
	omap_dm_timer_set_load(timer_ptr, 1, load);
	//store the new load value
	pwm_data.load = 24*period;
	//keep previous dutycycle
	set_pwm_dutycycle(pwm_data.dutycycle);
	
	return 0;
}

// set the pwm duty cycle (permils of period)
static int set_pwm_dutycycle(int dutycycle)
{
	uint32_t val;
	dutycycle = (dutycycle > 1000) 	? 1000	: dutycycle;
	dutycycle = (dutycycle < 	0) 	? 0 	: dutycycle;
	val = pwm_data.load * dutycycle / 1000 ;
	val = TIMER_MAX + 1 - val;
	omap_dm_timer_set_match(timer_ptr, 1, val);
	pwm_data.dutycycle = dutycycle;

	return 0;
}

//set motor rotation direction
static void set_motor_direction(int new_pin)
{
	if (new_pin == 1){
		pwm_data.pin = GPIO_OUTPUT_PORT_R;
		gpio_set_value(GPIO_OUTPUT_PORT_L, OFF_VALUE);
	} else {
		pwm_data.pin = GPIO_OUTPUT_PORT_L;
		gpio_set_value(GPIO_OUTPUT_PORT_R, OFF_VALUE);
	}
}

// setup a GPIO pin for use
static int pwm_setup_pin(uint32_t gpio_number)
{
	int err;

	// see if that pin is available to use
	if (gpio_is_valid(gpio_number)){
		rtdm_printk("pwm module: setting up gpio pin %i...",gpio_number);
		// allocate the GPIO pin
		err = gpio_request(gpio_number,"pwmIRQ");

		if(err){
			rtdm_printk("pwm module: failed to request GPIO %i\n",gpio_number);
			return -1;
		}

		err = gpio_direction_output(gpio_number,0);
		
		if(err){
			rtdm_printk("pwm module: failed to set GPIO to ouput\n");
			return -1;
		}
	} else {
		rtdm_printk("pwm module: requested GPIO is not valid\n");
		return -1;
	}

	// return success
	rtdm_printk("DONE\n");
	return 0;
}

static int __init pwm_start(void)
{
	int ret = 0;
  	struct clk *timer_fclk;
	uint32_t gt_rate;

	rtdm_printk(KERN_INFO "Loading PWM Module... \n");

	// request any timer
	timer_ptr = omap_dm_timer_request();
	if(timer_ptr == NULL){
		// no timers available
		rtdm_printk(KERN_WARNING
				"pwm module: No more gp timers available, bailing out\n");
		return -1;
	}

	// set the clock source to the system clock
	ret = omap_dm_timer_set_source(timer_ptr, OMAP_TIMER_SRC_SYS_CLK);
	if(ret) {
		rtdm_printk("pwm module: could not set source\n");
		return -1;
	}

	//timer prescaler settings (look at pwm.h for more settings)
	omap_dm_timer_set_prescaler(timer_ptr, TIMER_PRESC_1_16);

	// figure out what IRQ our timer triggers
	timer_irq = omap_dm_timer_get_irq(timer_ptr);

	// install our IRQ handler for our timer
	ret = rtdm_irq_request(&irqt, timer_irq, timer_irq_handler, 
			0,"pwm",NULL);
	rtdm_irq_enable(&irqt);
	if(ret){
		rtdm_printk("pwm module: rtdm_request_irq failed (on irq %d), bailing out\n", timer_irq);
		return ret;
	}

	// get clock rate in Hz and add it to struct
	timer_fclk = omap_dm_timer_get_fclk(timer_ptr);
	gt_rate = clk_get_rate(timer_fclk);
	pwm_data.timer_rate = gt_rate;

	//set pwm fulfillment
	set_pwm_dutycycle(500);
	//set preload, and autoreload
	set_pwm_period(100);

	// setup timer to trigger IRQ on the overflow
	omap_dm_timer_set_int_enable(timer_ptr, OMAP_TIMER_INT_OVERFLOW|OMAP_TIMER_INT_MATCH);
	
	// setup a GPIO
	pwm_setup_pin(GPIO_OUTPUT_PORT_R);
	pwm_setup_pin(GPIO_OUTPUT_PORT_L);
	
	pwm_data.pin = GPIO_OUTPUT_PORT_R;

	rtdm_printk(KERN_DEBUG 
			"pwm module: GP Timer initialized (%lu Hz, IRQ %d)\n",
			(long unsigned)gt_rate, timer_irq);
	rtdm_mutex_init(&write_mutex);

	// return success
	rtdm_dev_register(&device);
	return 0;
}

static void __exit pwm_end(void)
{
	rtdm_printk(KERN_INFO "Exiting PWM Module. \n");

	//disable interrupts
	rtdm_irq_disable(&irqt);

 	// release the timer
  	omap_dm_timer_free(timer_ptr);

	// release GPIO
	gpio_free(GPIO_OUTPUT_PORT_R);
	gpio_free(GPIO_OUTPUT_PORT_L);

	//unregister device
	rtdm_dev_unregister(&device,1000);

	rtdm_printk(KERN_INFO "rt calls: %d, nrt calls: %d \n",
			debug_tab[0], debug_tab[1]);
}

// entry and exit points
module_init(pwm_start);
module_exit(pwm_end);
