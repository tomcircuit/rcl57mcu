/**
  ******************************************************************************
  * @file    usart_utilities.h
  * @author  Saeid Yazdani - http://www.embedonix.com
  * @version V1.0.0
  * @date    8-5-2014
  * @brief   USART utility for STM32 line of microcontrollers.
  * @see     usart_utilities.h for complete explanation
  */


#include "usart_utilities.h"

void UU_PutChar(USART_TypeDef* USARTx, uint8_t ch)
{
  while(!(USARTx->SR & USART_SR_TXE));
  USARTx->DR = ch;  
}

void UU_PutString(USART_TypeDef* USARTx, uint8_t * str)
{
  while(*str != 0)
  {
    UU_PutChar(USARTx, *str);
    str++;
  }
}

void UU_PutNumber(USART_TypeDef* USARTx, uint32_t x)
{
  char value[10]; //a temp array to hold results of conversion
  int i = 0; //loop index
  
  do
  {
    value[i++] = (char)(x % 10) + '0'; //convert integer to character
    x /= 10;
  } while(x);
  
  while(i) //send data
  {
    UU_PutChar(USARTx, value[--i]); 
  }
}
