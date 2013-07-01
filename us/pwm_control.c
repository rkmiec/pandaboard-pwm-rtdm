#include <rtdm/rtdm.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <native/task.h>
#include <stdio.h>


int main( )
{
	int val, reg;
	int file = rt_dev_open("pwmgpio",0);
	if (file < 0)
		return -1;
	mlockall(MCL_CURRENT | MCL_FUTURE);

	rt_printf("Wprowadz zmienna, ktora chcesz edytowac\n");

	rt_printf("1 \t- freq\n");
	rt_printf("2 \t- dutycycle\n");
	//rt_printf("3 \t- pin (not implemented)\n");
	
	for (;;){
		scanf("%d",&reg);

		rt_printf("Podaj nowa wartosc\n");
		scanf("%d",&val);

		rt_dev_ioctl(file, reg, (const void *) &val);
	}
		
	return 0;
}
