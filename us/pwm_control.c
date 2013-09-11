#include <rtdm/rtdm.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <native/task.h>
#include <native/sem.h>
#include <stdio.h>

#include "../pwm.h"


RT_SEM write_sem;
int val, reg;
int file;

/* Probably it should be done via condition variable services, not sem */
void writer_rt(void * cookie)
{
	for (;;){
		rt_sem_p(&write_sem, TM_INFINITE);
		rt_dev_ioctl(file, reg, (const void *) &val);
	}
}


int main( )
{
	RT_TASK write_loop;
	file = rt_dev_open("pwmgpio",0);
	printf("device number: %d\n", file);
	if (file < 0)
		return -1;
	mlockall(MCL_CURRENT | MCL_FUTURE);

	rt_sem_create(&write_sem, "IOCTL sem", 0, S_PRIO);
	rt_task_create(&write_loop, "Loop IOCTL controller", 0, 99, T_JOINABLE);
	rt_task_start(&write_loop, &writer_rt, NULL);
	printf("Wprowadz zmienna, ktora chcesz edytowac\n");

	printf("%d \t- period\n", SET_PERIOD);
	printf("%d \t- dutycycle\n", SET_DUTYCYCLE);
	printf("%d \t- direction\n", SET_DIRECTION);

	//rt_printf("3 \t- pin (not implemented)\n");

	for (;;){
		printf("Wprowadz zmienna, ktora chcesz edytowac\n");
		scanf("%d",&reg);
		printf("Wprowadz nowa wartosc\n");
		scanf("%d",&val);
		rt_sem_v(&write_sem);
	}
	rt_sem_delete(&write_sem);
	rt_task_destroy(&write_loop);

	return 0;
}
