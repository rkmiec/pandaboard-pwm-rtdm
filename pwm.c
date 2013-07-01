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
static pwm_data_t pwm_data_ptr;
//irq handler
static rtdm_irq_t irqt;
static int val;
static int debug_tab[] = {0,0,0,0,0,0,0};

#define TIMER_MAX 0xFFFFFFFF

// do some kernel module documentation
MODULE_AUTHOR("Robert Kmiec <robertkmiec1989@gmail.com>");
MODULE_DESCRIPTION("Hardware PWM using GPIO");
MODULE_LICENSE("GPL");

static int rtdm_open_nrt(struct rtdm_dev_context *context,
		rtdm_user_info_t *user_info, int oflags)
{
	// start the timer
	omap_dm_timer_start(timer_ptr);

	// done!
	rtdm_printk(KERN_DEBUG "pwm module: Device opened and GP Timer started\n");

	return 0;
}

static int rtdm_close_nrt(struct rtdm_dev_context *context,
		rtdm_user_info_t *user_info)
{
	// stop the timer
	omap_dm_timer_stop(timer_ptr);

	//set gpio to low
	gpio_set_value(pwm_data_ptr.pin, 0);

	// done!
	rtdm_printk(KERN_DEBUG "pwm module: Device closed and GP Timer stopped\n");

	return 0;
}

//TODO: add mutex
static int rtdm_ioctl_rt(struct rtdm_dev_context *context,
		rtdm_user_info_t *user_info, unsigned int request, void __user *arg)
{
	context_data_t *data = (context_data_t*) context->dev_private;
	if (rtdm_in_rt_context())
		rtdm_printk("pwm ioctl: rt_context\n");
	else rtdm_printk("pwm ioctl: nrt context 0x%x\n",request);
	switch (request) {
		case SET_FREQUENCY:
			return 0;
			break;
		case SET_DUTYCYCLE:
			data->size = sizeof(data->value);
			if (rtdm_safe_copy_from_user(user_info, &(data->value), arg, 
						data->size)) {
				rtdm_printk(KERN_WARNING "pwm: ioctl: error %p\n",arg);
				return -1;
			}
			set_pwm_dutycycle(1,data->value*2);
			//omap_dm_timer_set_match(timer_ptr,1,data->value*64);
			return data->size;
			break;
		case SET_DIRECTION:
			return 0;
			break;
		default: 
			return -1;
	}
}

static struct rtdm_device device = { 
	.struct_version = RTDM_DEVICE_STRUCT_VER,
	.device_flags = RTDM_NAMED_DEVICE | RTDM_EXCLUSIVE,
	//rozmiar struktury z danymi
	.context_size = sizeof(context_data_t),
	.device_name = "pwmgpio",
	.open_nrt = rtdm_open_nrt,
	//.open_rt = rtdm_open_nrt,
	.ops = { 
		.ioctl_rt = rtdm_ioctl_rt,
		.ioctl_nrt = rtdm_ioctl_rt,
		.close_nrt = rtdm_close_nrt,
		.close_rt = rtdm_close_nrt,
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
	// reset the timer interrupt status
	val = omap_dm_timer_read_status(timer_ptr);
	val &= 0b111;
	debug_tab[val]+=1;
	omap_dm_timer_write_status(timer_ptr,OMAP_TIMER_INT_OVERFLOW|
			OMAP_TIMER_INT_MATCH);
	omap_dm_timer_read_status(timer_ptr); //you need to do this read
	//omap_dm_timer_write_counter(timer_ptr,0);	

 	// toggle pin
/*	if(gpio_get_value(pwm_data_ptr.pin) == 0 ) {
		gpio_set_value(pwm_data_ptr.pin,1);
	} else {
		gpio_set_value(pwm_data_ptr.pin,0);
	}*/
	if (val == OMAP_TIMER_INT_OVERFLOW)
		gpio_set_value(pwm_data_ptr.pin, 0);
	else
		gpio_set_value(pwm_data_ptr.pin, 1);
}


//the interrupt handler
int timer_irq_handler(rtdm_irq_t *irq_handle)
{
	timer_handler();
	// tell the nucleus it's handled
	return RTDM_IRQ_HANDLED;
}


// set the pwm frequency
static int set_pwm_freq(int freq) {
	// set preload, and autoreload of the timer
	//
	uint32_t period = pwm_data_ptr.timer_rate / (5*freq);
	uint32_t load = TIMER_MAX+1 - period;
	omap_dm_timer_set_load(timer_ptr, 1, load);
	//store the new frequency
	pwm_data_ptr.frequency = freq;
	pwm_data_ptr.load = load;
	
	return 0;
}

// set the pwm duty cycle
static int set_pwm_dutycycle(uint32_t pin,int dutycycle)
{
	uint32_t val = 	TIMER_MAX+1 - dutycycle;
	omap_dm_timer_set_match(timer_ptr,1,val);
	pwm_data_ptr.dutycycle = val;

	return 0;
}

// setup a GPIO pin for use
static int pwm_setup_pin(uint32_t gpio_number)
{
	int err;

	// see if that pin is available to use
	if (gpio_is_valid(gpio_number)) {
		rtdm_printk("pwm module: setting up gpio pin %i...",gpio_number);
		// allocate the GPIO pin
		err = gpio_request(gpio_number,"pwmIRQ");
		//error check
		if(err) {
			rtdm_printk("pwm module: failed to request GPIO %i\n",gpio_number);
			return -1;
		}

		// set as output
		err = gpio_direction_output(gpio_number,0);

		//error check
		if(err) {
			rtdm_printk("pwm module: failed to set GPIO to ouput\n");
			return -1;
		}

		//add gpio data to struct
		pwm_data_ptr.pin = gpio_number;
	} else {
		rtdm_printk("pwm module: requested GPIO is not valid\n");
		// return failure
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

	// set prescalar to 1:1
	omap_dm_timer_set_prescaler(timer_ptr, 3);

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
	pwm_data_ptr.timer_rate = gt_rate;

	// set preload, and autoreload
	// we set it to a default of 1kHz
	set_pwm_freq(1000);

	// setup timer to trigger IRQ on the overflow
	omap_dm_timer_set_int_enable(timer_ptr, OMAP_TIMER_INT_OVERFLOW|OMAP_TIMER_INT_MATCH);
	
	// setup a GPIO
	pwm_setup_pin(GPIO_OUTPUT_PORT);
	
	pwm_data_ptr.pin = GPIO_OUTPUT_PORT;

	set_pwm_dutycycle(1,150);

	rtdm_printk(KERN_DEBUG 
			"pwm module: GP Timer initialized (%lu Hz, IRQ %d)\n",
			(long unsigned)gt_rate, timer_irq);

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
	gpio_free(pwm_data_ptr.pin);

	//unregister device
	rtdm_dev_unregister(&device,1000);

	int i;
	for (i = 0; i<6; i+=1) {
		rtdm_printk("%d ",debug_tab[i]);
	}
	rtdm_printk("\n");
}

// entry and exit points
module_init(pwm_start);
module_exit(pwm_end);
