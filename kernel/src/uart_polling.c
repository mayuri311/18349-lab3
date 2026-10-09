#include <unistd.h>

#include <gpio.h>
#include <rcc.h>
#include <uart_polling.h>

/** @brief The UART register map. */
struct uart_reg_map {
    volatile uint32_t SR;   /**< Status Register */
    volatile uint32_t DR;   /**<  Data Register */
    volatile uint32_t BRR;  /**<  Baud Rate Register */
    volatile uint32_t CR1;  /**<  Control Register 1 */
    volatile uint32_t CR2;  /**<  Control Register 2 */
    volatile uint32_t CR3;  /**<  Control Register 3 */
    volatile uint32_t GTPR; /**<  Guard Time and Prescaler Register */
};

/** @brief Base address for UART2 */
#define UART2_BASE  (struct uart_reg_map *) 0x40004400


/** @brief Enable  Bit for UART Config register */
#define UART_EN (1 << 13)
#define UART_RCC (1 << 17)
#define UART_TE (1 << 3)
#define UART_RE (1 << 2)
#define UART_FCK 16000000
#define OVER8 0


/**
 * @brief initializes UART to given baud rate with 8-bit word length, 1 stop bit, 0 parity bits
 *
 * @param baud Baud rate
 */
void uart_polling_init (int baud){

    struct uart_reg_map *uart = UART2_BASE;
    struct rcc_reg_map *rcc = RCC_BASE;

    //tx line
    gpio_init(GPIO_A, 2, MODE_ALT, OUTPUT_PUSH_PULL, OUTPUT_SPEED_LOW, PUPD_NONE, ALT7);
    
    //rx line
    gpio_init(GPIO_A, 3, MODE_ALT, OUTPUT_OPEN_DRAIN, OUTPUT_SPEED_LOW, PUPD_NONE, ALT7);

    // enable RCC
    rcc->apb1_enr |= UART_RCC;

    uart->CR1 |= UART_EN;
    uart->CR1 |= UART_TE;
    uart->CR1 |= UART_RE;

    //baud rate calculation
    int brr_div = 8 * (2 - OVER8) * baud;
    uart->BRR = (16 * UART_FCK + brr_div / 2) / brr_div;

}

/**
 * @brief transmits a byte over UART
 *
 * @param c character to be sent
 */
void uart_polling_put_byte (char c){
    struct uart_reg_map *uart = UART2_BASE;

    // wait until data register empty
    while (!(uart->SR & (1 << 7))) {
        continue;
    }
    // clear existing data and write byte
    uart->DR = (c & 0xFF);
    return;
}

/**
 * @brief receives a byte over UART
 */
char uart_polling_get_byte () {
    struct uart_reg_map *uart = UART2_BASE;

    // wait until data is transferred to DR
    while (!(uart->SR & (1 << 5))) {
        continue;
    }

    return uart->DR & 0xFF;
}
