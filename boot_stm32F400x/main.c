#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "stm32f4xx.h"
#include "stm32f4xx_usart.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_flash.h"

#include "misc.h"
#include "core_cm4.h"

#include "usart_mini.h"
#include "rnd_gen.h"
#include "bootloader.h"

int main(void)
{
	SystemCoreClockUpdate();
	usart_init();
	bootloader_init(true);

    while(1)
    {
    	uint32_t cnt = 0;
    	for (cnt = 0; cnt < 16; cnt++)
    		if (bootloader_activate(100000))
    			bootloader_commands();
    		else
    			send('!');

    	bootloader_go(BOOTLOADER_TO);
    }
}
