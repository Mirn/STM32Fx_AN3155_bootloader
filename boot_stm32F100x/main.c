#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "stm32f10x.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_flash.h"

#include "misc.h"
#include "core_cm3.h"

#include "usart_mini.h"
#include "rnd_gen.h"
#include "bootloader.h"

int main(void)
{
	usart_init();
	bootloader_init(true);

    while(1)
    {
    	if (bootloader_activate(300000))
    		bootloader_commands();
    	//else
    		//send('!');

        bootloader_go(BOOTLOADER_TO);
    }
}
