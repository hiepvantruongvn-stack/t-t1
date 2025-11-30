#include "stm32f10x.h"

void Delay_Test(uint32_t time) {
    while(time--);
}


int main() {   

	GPIO_InitTypeDef gpio;  USART_InitTypeDef uart;
    gpio.GPIO_Pin = GPIO_Pin_9;  

    // 1. C?p Clock cho GPIOA và USART1
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1, ENABLE);
    
    // 2. C?u hình PA9 (TX)

    gpio.GPIO_Mode = GPIO_Mode_AF_PP; // Push-Pull Alternate Function
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);
    
    // 3. C?u hình PA10 (RX)
    gpio.GPIO_Pin = GPIO_Pin_10;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);
    
    // 4. C?u hình USART1 (9600 baud)

    uart.USART_BaudRate = 9600;
    uart.USART_WordLength = USART_WordLength_8b;
    uart.USART_StopBits = USART_StopBits_1;
    uart.USART_Parity = USART_Parity_No;
    uart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    uart.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    
    USART_Init(USART1, &uart);
    USART_Cmd(USART1, ENABLE);
    
    while(1) {
        // G?i ký t? 'A'
        USART_SendData(USART1, 'A');
        while(USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET); // Ch? g?i xong
        
        // G?i xu?ng dòng
        USART_SendData(USART1, '\n');
        while(USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);
        
        Delay_Test(1000000); // Delay kho?ng 0.5s
    }
}