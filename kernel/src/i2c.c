#include <unistd.h>

#include <gpio.h>
#include <i2c.h>
#include <rcc.h>


/** @brief The i2c register map. */
struct i2c_reg_map {
    volatile uint32_t CR1;   /**< Control Register 1 */
    volatile uint32_t CR2;   /**< Control Register 2 */
    volatile uint32_t OAR1;  /**< Own Address Register 1 */
    volatile uint32_t OAR2;  /**< Own Address Register 2 */
    volatile uint32_t DR;    /**< Data Register */
    volatile uint32_t SR1;   /**< Status Register 1 */
    volatile uint32_t SR2;   /**< Status Register 2 */
    volatile uint32_t CCR;   /**< Clock Control Register */
    volatile uint32_t TRISE; /**< Rise Time Register */
    volatile uint32_t FLTR;  /**< Noise Filter Register */
};

/** @brief Base address for i2c1 */
#define I2C1_BASE  (struct i2c_reg_map *) 0x40005400

/** @brief i2c reg bits and clock settings */
#define I2C_RCC (1 << 21)
#define I2C_PE (1 << 0)
#define I2C_PCLK_MHZ 16


//for start stop
#define I2C_START (1 << 8)
#define I2C_STOP (1 << 9)
#define I2C_SB (1 << 0)
#define I2C_BTF (1 << 2)
#define I2C_TXE (1 << 7)


//for write
#define I2C_ADDR (1 << 1)
#define I2C_AF (1 << 10)
#define I2C_WRITE_BIT 0


void i2c_master_init(uint16_t clk){
    //reg map
    struct i2c_reg_map *i2c = I2C1_BASE;
    struct rcc_reg_map *rcc = RCC_BASE;

    //sda line (D14)
    gpio_init(GPIO_B, 9, MODE_ALT, OUTPUT_OPEN_DRAIN, OUTPUT_SPEED_LOW, PUPD_NONE, ALT4);
    //scl line (D15)
    gpio_init(GPIO_B, 8, MODE_ALT, OUTPUT_OPEN_DRAIN, OUTPUT_SPEED_LOW, PUPD_NONE, ALT4);

    // enable RCC
    rcc->apb1_enr |= I2C_RCC;

    // timing regs only we while disabled
    i2c->CR1 &= ~I2C_PE;

    // clock fq 100 MHz
    i2c->CR2 = I2C_PCLK_MHZ;

    // clk 16000 / (2 * 100) = 80
    i2c->CCR = (I2C_PCLK_MHZ * 1000) / (2 * clk);

    // max rise time: 1000ns / 62.5ns + 1 = 17
    i2c->TRISE = I2C_PCLK_MHZ + 1;
    i2c->CR1 |= I2C_PE;

    return;
}

void i2c_master_start() {
    struct i2c_reg_map *i2c = I2C1_BASE;

    i2c->CR1 |= I2C_START;

    // EV5: wait for start bit. SB is cleared later by this SR1 read
    // then, writing the address to DR in i2c_master_write
    // ev5: SB=1, cleared by reading SR1 followed by writing DR with Address
    while (!(i2c->SR1 & I2C_SB)) {
        continue;
    }

}

void i2c_master_stop() {

    struct i2c_reg_map *i2c = I2C1_BASE;

    // EV8_2: wait until data register and shift register are both empty
    // ev8_2: TxE=1, BTF=1, Program Stop request. TxE and BTF are cleared by hardware by STOP condition
    while (!(i2c->SR1 & I2C_TXE) || !(i2c->SR1 & I2C_BTF)) {
        continue;
    }

    i2c->CR1 |= I2C_STOP;

    // hardware clears STOP once the condition is on the bus
    while (i2c->CR1 & I2C_STOP) {
        continue;
    }

}

int i2c_master_write(uint8_t *buf, uint16_t len, uint8_t slave_addr){
    (void) buf;
    (void) len;
    (void) slave_addr;

    struct i2c_reg_map *i2c = I2C1_BASE;

    // EV5 is waited on after start, also SR1 read
    i2c_master_start();

    // address in the top 7 bits, write bit at the bottom. wr DR
    // completes clearing SB
    i2c->DR = (slave_addr << 1) | I2C_WRITE_BIT;

    // EV6: wait for address to be ack
    while (!(i2c->SR1 & I2C_ADDR)) {
        // participant didn't ACK  address
        if (i2c->SR1 & I2C_AF) {
            i2c->SR1 &= ~I2C_AF;
            i2c->CR1 |= I2C_STOP;
            return -1;
        }
    }

    // ADDR is cleared by re SR1 & SR2
    (void) i2c->SR2;

    // EV8_1 / EV8: load each byte once the data register is empty
    for (uint16_t i = 0; i < len; i++) {
        while (!(i2c->SR1 & I2C_TXE)) {
            // slave didn't ACK last byte
            if (i2c->SR1 & I2C_AF) {
                i2c->SR1 &= ~I2C_AF;
                i2c->CR1 |= I2C_STOP;
                return -1;
            }
        }
        i2c->DR = buf[i];
    }

    // EV8_2: stop waits for TxE and BTF stop
    i2c_master_stop();

    return 0;
}

int i2c_master_read(uint8_t *buf, uint16_t len, uint8_t slave_addr){
    (void) buf;
    (void) len;
    (void) slave_addr;

    return 0;
}
