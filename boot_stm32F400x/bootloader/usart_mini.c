#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#ifdef STM32F10X_LD_VL
#include "stm32f10x.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#endif

#ifdef STM32F4XX
#include "stm32f4xx.h"
#include "stm32f4xx_usart.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_gpio.h"
#endif

#include "usart_mini.h"

#define USART_BOD 500000

void usart_init()
{
#ifdef STM32F10X_LD_VL
#ifdef NEED_SMALL
	RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
	RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;

	USART1->BRR = SystemCoreClock / USART_BOD;
	USART1->CR1 |= USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_PCE | USART_CR1_M;

	GPIOA->CRH	&= ~GPIO_CRH_CNF9; 		                 // Clear CNF bit 9
	GPIOA->CRH	|= (GPIO_CRH_CNF9_1 | GPIO_CRH_MODE9_0); // Set CNF bit 9 to 10 - AFIO Push-Pull

	GPIOA->CRH	&= ~(GPIO_CRH_CNF10 | GPIO_CRH_MODE10);	    // Clear CNF bit 9 // Set MODE bit 9 to Mode 01 = 10MHz
	GPIOA->CRH	|= GPIO_CRH_CNF10_0;	// Set CNF bit 9 to 01 = HiZ
#else
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitTypeDef GPIO_Init_RX =
    {
            .GPIO_Pin   = GPIO_Pin_10,
            .GPIO_Mode  = GPIO_Mode_IN_FLOATING,
            .GPIO_Speed = GPIO_Speed_50MHz
    };
    GPIO_Init(GPIOA, &GPIO_Init_RX);

    GPIO_InitTypeDef GPIO_Init_TX =
    {
    	    .GPIO_Pin   = GPIO_Pin_9,
    	    .GPIO_Speed = GPIO_Speed_50MHz,
    	    .GPIO_Mode  = GPIO_Mode_AF_PP
    };
    GPIO_Init(GPIOA, &GPIO_Init_TX);

    USART_InitTypeDef USART_InitStructure = {
            .USART_BaudRate            = USART_BOD,
            .USART_WordLength          = USART_WordLength_9b,
            .USART_StopBits            = USART_StopBits_1,
            .USART_Parity              = USART_Parity_Even,
            .USART_HardwareFlowControl = USART_HardwareFlowControl_None,
            .USART_Mode                = USART_Mode_Rx | USART_Mode_Tx
    };

    USART_Init(USART1, &USART_InitStructure);
    USART_Cmd(USART1, ENABLE);
#endif
#endif

#ifdef STM32F4XX
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

	GPIO_PinAFConfig(GPIOA, GPIO_PinSource9,  GPIO_AF_USART1);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_USART1);


    GPIO_InitTypeDef GPIO_InitStructure =
    {
    		.GPIO_OType = GPIO_OType_PP,
    		.GPIO_PuPd  = GPIO_PuPd_UP,
    		.GPIO_Mode  = GPIO_Mode_AF,
    		.GPIO_Pin   = GPIO_Pin_9 | GPIO_Pin_10
    };
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    USART_InitTypeDef USART_InitStructure = {
            .USART_BaudRate            = USART_BOD,
            .USART_WordLength          = USART_WordLength_9b,
            .USART_StopBits            = USART_StopBits_1,
            .USART_Parity              = USART_Parity_Even,
            .USART_HardwareFlowControl = USART_HardwareFlowControl_None,
            .USART_Mode                = USART_Mode_Rx | USART_Mode_Tx
    };

    USART_Init(USART1, &USART_InitStructure);
    USART_Cmd(USART1, ENABLE);
#endif
}

bool wait(uint8_t *rx_data)
{
	uint16_t timeout = 0xFFFF;
	while ((timeout--) && (!recive_ready()));

	if (recive_ready())
	{
		*rx_data = recive();
		return true;
	}
	else
		return false;
}

void send(uint8_t tx_data)
{
	while ((USART1->SR & USART_FLAG_TC) == RESET);
    USART1->DR = tx_data;
}

void __send_block(uint8_t *data, uint32_t size)
{
	while (size--)
		send(*(data++));
}

void send_str(const char *str)
{
	while (*str)
		send(*(str++));
}

