#include <rtdm/rtdm.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <native/task.h>
#include <stdio.h>


void sterownik (void *cookie)
{
	uint32_t duty = 1;
	int file = rt_dev_open("pwmgpio",0);
	if (file < 0)
		return;
	for ( ; duty < 1000; duty+=16 ){
		rt_dev_ioctl(file, 0x02, (const void *) &duty);
		rt_task_sleep(500000000UL);
	}
}


int main( )
{
	RT_TASK sterowanie;
	int err;
	mlockall(MCL_CURRENT | MCL_FUTURE);
	err = rt_task_create(&sterowanie, "ster", 0, 99, T_JOINABLE);
	if (err != 0)
		return -1;
	rt_task_start(&sterowanie, &sterownik, NULL);
	rt_task_join(&sterowanie);
	return 0;
}
