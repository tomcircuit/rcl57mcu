/**
  ******************************************************************************
  * @file    usart_utilities.h
  * @author  Saeid Yazdani - http://www.embedonix.com
  * @version V1.0.0
  * @date    8-5-2014
  * @brief   USART utility for STM32 line of microcontrollers
  *
  @verbatim
  ==============================================================================
                     ##### How to use this utility #####
  ==============================================================================
    [..]
    Include this header and its source file in your project. Make sure that you
    have "usart.h" which is provided in ST's firmware package of your micro
    so that the include statement in this file does not fail
    [..]
    In your main code, you can simply call to write a string to USART like:        
    To send string:
      SD_PutString(USART2, "EMBEDONIX.COM");
    To send numerical values :
      void SD_PutNumber(USART2, 1234);

    note: 
      USART2 in the above example means that USART2 should be used! If you
      configured another USART, use that instead. The constant are defined in 
      stm32fxxxxx.h
      
    note: 
      it is up to you to sent a new line command, if you want to do so just
      send a '\n' character using UU_PutChar() function

  @endverbatim
  ******************************************************************************
  */


#ifndef __USART_UTILITIES_H
#define __USART_UTILITIES_H

#include "stm32f10x_usart.h"

/**
  * @brief Sends a single character
  * @param USARTx: specifies the USART to use
  */
void UU_PutChar(USART_TypeDef* USARTx, uint8_t ch);

/**
  * @brief Sends a string 
  * @param USARTx: specifies the USART to use
  */
void UU_PutString(USART_TypeDef* USARTx, uint8_t* message);

/**
  * @brief Converts an unsigned 32 integer to characters and then
  *	   sends it character by character. Please note that the maximum
  *	   value is 4,294,967,295 which equals 0xFFFFFFFF. Any number 
  *	   greater than this number will be turncated.
  * @param USARTx: specifies the USART to use
  */
void UU_PutNumber(USART_TypeDef* USARTx, uint32_t x);
#endif