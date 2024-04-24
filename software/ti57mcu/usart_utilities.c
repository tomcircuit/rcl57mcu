/**
  ******************************************************************************
  * @file    usart_utilities.c
  * @author  Saeid Yazdani - http://www.embedonix.com
  * @version V1.0.0
  * @date    8-5-2014
  * @brief   USART utility for STM32 line of microcontrollers.
  * @see     usart_utilities.h for complete explanation
  */


#include "usart_utilities.h"

void UU_PutChar(USART_TypeDef* USARTx, uint8_t ch)
{
    while (!(USARTx->SR & USART_SR_TXE));
    USARTx->DR = ch;
}

void UU_PutString(USART_TypeDef* USARTx, uint8_t* str)
{
    while (*str != 0)
    {
        UU_PutChar(USARTx, *str);
        str++;
    }
}

void UU_PutHexByte(USART_TypeDef* USARTx, uint8_t x)
{
    char value = 0;

    value = x >> 4;
    (value > 9) ? UU_PutChar(USARTx, 'A' + value - 10) : UU_PutChar(USARTx, '0' + value);

    value = x & 0xf;
    (value > 9) ? UU_PutChar(USARTx, 'A' + value - 10) : UU_PutChar(USARTx, '0' + value);
}


void UU_PutNumber(USART_TypeDef* USARTx, uint32_t x)
{
    char value[10]; //a temp array to hold results of conversion
    int i = 0; //loop index

    do
    {
        value[i++] = (char)(x % 10) + '0'; //convert integer to character
        x /= 10;
    }
    while (x);

    while (i) //send data
    {
        UU_PutChar(USARTx, value[--i]);
    }
}

void UU_PutHex(USART_TypeDef* USARTx, uint32_t x)
{
    char value[10]; //a temp array to hold results of conversion
    int i = 0; //loop index
		unsigned char temp = 0;

    do
    {
				temp = x & 0xf;
				value[i++] = (temp > 9) ? 'A' + (temp - 10) : '0' + temp;			
        x = x >> 4;
    }
    while (x);

    while (i) //send data
    {
        UU_PutChar(USARTx, value[--i]);
    }
}

