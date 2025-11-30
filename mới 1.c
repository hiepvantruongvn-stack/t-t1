#include "stm32f10x.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

// ----------------------
// Bi?n toàn c?c
// ----------------------
float temperature = 0.0f;   // luu nhi?t d? do du?c
float humidity    = 0.0f;   // luu d? ?m do du?c
int   status      = 0;      // 0=OK, 1=không ph?n h?i, 2=checksum sai

// ----------------------
// Delay b?ng SysTick (ngu?n m?c d?nh AHB/8)
// ----------------------
void Delay_Init(void) {
    SysTick->CTRL = 0;         // t?t SysTick, CLKSOURCE=0 => AHB/8
    SysTick->LOAD = 0xFFFFFF;  // d?t giá tr? d?m t?i da
    SysTick->VAL  = 0;         // reset
}

void Delay_us(uint32_t us) {
    // V?i CLKSOURCE=AHB/8: m?i 1us c?n SystemCoreClock/8 chu k?
    // -> LOAD = (SystemCoreClock/8) * us - 1. Ta làm g?n b?ng chia 8e6.
    SysTick->LOAD = (SystemCoreClock / 8000000U) * us;
    SysTick->VAL  = 0;
    SysTick->CTRL = SysTick_CTRL_ENABLE_Msk;  // b?t SysTick
    while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0) {;}
    SysTick->CTRL = 0;                         // t?t SysTick
}

void Delay_ms(uint32_t ms) {
    while (ms--) Delay_us(1000U);
}

// ----------------------
// UART1 trên PA9 (TX), PA10 (RX)
// ----------------------
void UART1_Init(uint32_t baud) {
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1, ENABLE);

    GPIO_InitTypeDef GPIO_InitStruct;
    // TX - PA9
    GPIO_InitStruct.GPIO_Pin   = GPIO_Pin_9;
    GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStruct);

    // RX - PA10
    GPIO_InitStruct.GPIO_Pin  = GPIO_Pin_10;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStruct);

    USART_InitTypeDef USART_InitStruct;
    USART_InitStruct.USART_BaudRate            = baud;
    USART_InitStruct.USART_WordLength          = USART_WordLength_8b;
    USART_InitStruct.USART_StopBits            = USART_StopBits_1;
    USART_InitStruct.USART_Parity              = USART_Parity_No;
    USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStruct.USART_Mode                = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART1, &USART_InitStruct);
    USART_Cmd(USART1, ENABLE);
}

void UART1_SendChar(char c) {
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET) {;}
    USART_SendData(USART1, c);
}

void UART1_SendString(const char *s) {
    while (*s) UART1_SendChar(*s++);
}

// ----------------------
// DHT11 trên PA0
// ----------------------
#define DHT11_PORT GPIOA
#define DHT11_PIN  GPIO_Pin_0

static void DHT11_SetPinOutput(void) {
    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.GPIO_Pin   = DHT11_PIN;
    GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}

static void DHT11_SetPinInput(void) {
    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.GPIO_Pin  = DHT11_PIN;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING; // c?n có di?n tr? kéo lên ngoài
    GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}

void DHT11_Init(void) {
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    DHT11_SetPinOutput();
    GPIO_SetBits(DHT11_PORT, DHT11_PIN); // kéo cao m?c d?nh
}

// ----------------------
// Ð?C DHT11
// ----------------------
// **** ÐÃ S?A: dùng float* thay vì int* ****
int DHT11_Read(float *temp, float *humi) {
    uint8_t data[5] = {0};
    uint8_t i, j;
    uint32_t timeout;

    // Start signal
    DHT11_SetPinOutput();
    GPIO_ResetBits(DHT11_PORT, DHT11_PIN);
    Delay_ms(18);                        // >=18ms
    GPIO_SetBits(DHT11_PORT, DHT11_PIN);
    Delay_us(40);                        // 20-40us
    DHT11_SetPinInput();

    // Response: low ~80us
    timeout = 100;
    while (GPIO_ReadInputDataBit(DHT11_PORT, DHT11_PIN) && timeout--) {;}
    if (timeout == 0) return 1;          // không ph?n h?i

    // high ~80us
    timeout = 1000;
    while (!GPIO_ReadInputDataBit(DHT11_PORT, DHT11_PIN) && timeout--) {;}
    if (timeout == 0) return 1;

    // low chu?n b? g?i
    timeout = 10000;
    while (GPIO_ReadInputDataBit(DHT11_PORT, DHT11_PIN) && timeout--) {;}
    if (timeout == 0) return 1;

    // Read 5 bytes
    for (j = 0; j < 5; j++) {
        for (i = 0; i < 8; i++) {
            timeout = 10000;
            while (!GPIO_ReadInputDataBit(DHT11_PORT, DHT11_PIN) && timeout--) {;}
            if (timeout == 0) return 1;

            Delay_us(40); // sau 40us: còn cao => bit 1
            if (GPIO_ReadInputDataBit(DHT11_PORT, DHT11_PIN)) {
                data[j] |= (1U << (7 - i));
            }

            timeout = 10000;
            while (GPIO_ReadInputDataBit(DHT11_PORT, DHT11_PIN) && timeout--) {;}
            if (timeout == 0) return 1;
        }
    }

    // Checksum
    if ((uint8_t)(data[0] + data[1] + data[2] + data[3]) != data[4])
        return 2;

    // DHT11: data[0]=RH int, data[1]=RH dec(0), data[2]=T int, data[3]=T dec(0)
    *humi = (float)data[0];
    *temp = (float)data[2];
    return 0;
}

// ----------------------
// Main
// ----------------------
int main(void) {
    Delay_Init();
    DHT11_Init();
    UART1_Init(115200);

    char buf[64];

    while (1) {
        status = DHT11_Read(&temperature, &humidity);

        if (status == 0) {
            // Ð?C OK
            sprintf(buf, "Temp=%.2f Hum=%.2f\r\n", temperature, humidity);
            UART1_SendString(buf);
        } else {
            // L?I: không ph?n h?i ho?c checksum
            temperature = 0.0f;
            humidity    = 0.0f;
            sprintf(buf, "Temp=%.2f Hum=%.2f ERROR(%d)\r\n", temperature, humidity, status);
            UART1_SendString(buf);
        }

        Delay_ms(2000);
    }
}
