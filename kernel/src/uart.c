/**
 * @file 
 *
 * @brief      
 *
 * @date       
 *
 * @author     
 */

#include <unistd.h>
#include <rcc.h>
#include <uart.h>
#include <uart_polling.h>
#include <nvic.h>

#define UNUSED __attribute__((unused))

// UART register map
struct uart_reg_map {
    volatile uint32_t SR;   /**< Status Register */
    volatile uint32_t DR;   /**< Data Register */
    volatile uint32_t BRR;  /**< Baud Rate Register */
    volatile uint32_t CR1;  /**< Control Register 1 */
    volatile uint32_t CR2;  /**< Control Register 2 */
    volatile uint32_t CR3;  /**< Control Register 3 */
    volatile uint32_t GTPR; /**< Guard Time and Prescaler Register */
};


// TXE interrupt enable bit in CR1 
#define UART_TXEIE (1 << 7)
// RXNE interrupt enable bit in CR1
#define UART_RXNEIE (1 << 5)
// transmit data register empty bit in SR 
#define UART_SR_TXE (1 << 7)
// transmission complete bit in SR
#define UART_SR_TC (1 << 6)
// read data register not empty bit in SR
#define UART_SR_RXNE (1 << 5)
// mask for the 8 data bits in DR
#define UART_DR_MASK 0xFF

// USART2 IRQ number in the NVIC 
#define USART2_IRQ_NUM 38

// kernel buffer size in bytes
#define UART_BUF_SIZE 16

// fixed-size ring buffer with O(1) push and pop 
typedef struct {
    char data[UART_BUF_SIZE]; 
    uint32_t head;          
    uint32_t tail;       
    uint32_t count;      
} rbuf_t;

// TX kernel buffer, shared between main and the TX handler
static volatile rbuf_t tx_buf;

// RX kernel buffer, shared between main and the RX handler 
static volatile rbuf_t rx_buf;






// disables interrupts and returns the previous PRIMASK
static inline uint32_t irq_save(void) {
    uint32_t primask;
    __asm volatile("mrs %0, primask\n\tcpsid i" : "=r"(primask) :: "memory");
    return primask;
}

// restores PRIMASK saved by irq_save()
static inline void irq_restore(uint32_t primask) {
    __asm volatile("msr primask, %0" :: "r"(primask) : "memory");
}

//empties a ring buffer
static void buf_reset(volatile rbuf_t *buf) {
    buf->head = 0;
    buf->tail = 0;
    buf->count = 0;
}

// advances a ring buffer index
static inline uint32_t buf_next(uint32_t idx) {
    idx++;
    if (idx == UART_BUF_SIZE) {
        idx = 0;
    }
    return idx;
}

// adds a byte to a ring buffer
static int buf_push(volatile rbuf_t *buf, char c) {
    if (buf->count == UART_BUF_SIZE) {
        return -1;
    }
    buf->data[buf->head] = c;
    buf->head = buf_next(buf->head);
    buf->count++;
    return 0;
}

// removes a byte from a ring buffer
static int buf_pop(volatile rbuf_t *buf, char *c) {
    if (buf->count == 0) {
        return -1;
    }
    *c = buf->data[buf->tail];
    buf->tail = buf_next(buf->tail);
    buf->count--;
    return 0;
}

// moves one byte from the TX buffer into the data register
void USART2_TX_IRQHandler(void) {
    struct uart_reg_map *uart = UART2_BASE;
    char c;

    if (buf_pop(&tx_buf, &c) == 0) {
        uart->DR = c; // plain store: never |= on DR 
    }

    // buffer drained: stop TXE interrupts until uart_put_byte
    if (tx_buf.count == 0) {
        uart->CR1 &= ~UART_TXEIE;
    }
}

// moves one byte from the data register into the RX buffer
void USART2_RX_IRQHandler(void) {
    struct uart_reg_map *uart = UART2_BASE;

    // load DR, also clears RXNE
    char c = (char) (uart->DR & UART_DR_MASK);

    buf_push(&rx_buf, c);

    // buffer full
    if (rx_buf.count == UART_BUF_SIZE) {
        uart->CR1 &= ~UART_RXNEIE;
    }
}






void uart_init(int baud){
    struct uart_reg_map *uart = UART2_BASE;

    buf_reset(&tx_buf);
    buf_reset(&rx_buf);

    uart_polling_init(baud);

    // TX buffer is empty 
    // RX buffer has room 
    uart->CR1 = (uart->CR1 & ~UART_TXEIE) | UART_RXNEIE;

    nvic_irq(USART2_IRQ_NUM, IRQ_ENABLE);
}

int uart_put_byte(char c){
    struct uart_reg_map *uart = UART2_BASE;
    int ret;

    uint32_t primask = irq_save();

    ret = buf_push(&tx_buf, c);
    if (ret == 0) {
        // buffer now has data
        uart->CR1 |= UART_TXEIE;
    }

    irq_restore(primask);
    return ret;
}

int uart_get_byte(char *c){
    struct uart_reg_map *uart = UART2_BASE;
    int ret;

    uint32_t primask = irq_save();

    ret = buf_pop(&rx_buf, c);
    if (ret == 0 && rx_buf.count == UART_BUF_SIZE - 1) {
        // buffer was full
        uart->CR1 |= UART_RXNEIE;
    }

    irq_restore(primask);
    return ret;
}

void USART2_IRQHandler(void){
    struct uart_reg_map *uart = UART2_BASE;
    uint32_t sr = uart->SR;
    uint32_t cr1 = uart->CR1;

    if ((sr & UART_SR_RXNE) && (cr1 & UART_RXNEIE)) {
        USART2_RX_IRQHandler();
    }

    if ((sr & UART_SR_TXE) && (cr1 & UART_TXEIE)) {
        USART2_TX_IRQHandler();
    }

    nvic_clear_pending(USART2_IRQ_NUM);
}

void uart_flush(void){
    struct uart_reg_map *uart = UART2_BASE;
    char c;

    uint32_t primask = irq_save();

    uart->CR1 &= ~(UART_TXEIE | UART_RXNEIE);

    while (buf_pop(&tx_buf, &c) == 0) {
        while (!(uart->SR & UART_SR_TXE)) {
            continue;
        }
        uart->DR = c;
    }

    while (!(uart->SR & UART_SR_TC)) {
        continue;
    }

    buf_reset(&rx_buf);

    irq_restore(primask);
}

