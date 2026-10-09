#include <unistd.h>

#include <i2c.h>
#include <lcd_driver.h>


// 8-bit write add with A0-A2 high, shifted for i2c_master_write
#define LCD_ADDR (0x4E >> 1)

// 100 khz
#define LCD_I2C_CLK 100

// PCF8574 pin bits
#define LCD_RS (1 << 0)
#define LCD_EN (1 << 2)
#define LCD_BACKLIGHT (1 << 3)

// RS values for instruction vs character data
#define LCD_INSTR 0

// masks and shift for 4 data bits onto P4-P7
#define LCD_UPPER_BITS 0xF0
#define LCD_LOWER_BITS 0x0F
#define LCD_DATA_SHIFT 4

// HD44780 instructions
#define LCD_INIT_8BIT 0x3
#define LCD_INIT_4BIT 0x2
#define LCD_FUNCTION_SET 0x28
#define LCD_DISPLAY_ON 0x0F
#define LCD_ENTRY_MODE 0x06
#define LCD_CLEAR 0x01

#define LCD_CLEAR_DELAY 100000

#define LCD_INIT_4BIT_COUNT 4
#define LCD_INIT_8BIT_COUNT 3

// RS value for character data
#define LCD_DATA LCD_RS

// address instruction, address in lower 7 bits
#define LCD_SET_DDRAM 0x80

// address where the second line starts
#define LCD_ROW1_OFFSET 0x40


void lcd_driver_init() {
	// first four instructions from the 4-bit init flow chart (p. 46).
    // the LCD is still in 8-bit, so each is 4-bit packet
    uint8_t init_4bit[LCD_INIT_4BIT_COUNT] = {
        LCD_INIT_8BIT, LCD_INIT_8BIT, LCD_INIT_8BIT, LCD_INIT_4BIT
    };

    // now in 4-bit mode: 2 lines + 5x8 font, display on with blinking
    // cursor, cursor moves right after each character
    uint8_t init_8bit[LCD_INIT_8BIT_COUNT] = {
        LCD_FUNCTION_SET, LCD_DISPLAY_ON, LCD_ENTRY_MODE
    };

    i2c_master_init(LCD_I2C_CLK);

    for (int i = 0; i < LCD_INIT_4BIT_COUNT; i++) {
        uint8_t packet = (init_4bit[i] << LCD_DATA_SHIFT) | LCD_BACKLIGHT | LCD_INSTR;
        uint8_t buf[] = {packet | LCD_EN, packet};

        i2c_master_write(buf, sizeof(buf), LCD_ADDR);
    }

    // each 8-bit instruction goes out as two 4-bit packets, upper bits
    // first, in one I2C transmission
    for (int i = 0; i < LCD_INIT_8BIT_COUNT; i++) {
        uint8_t upper = (init_8bit[i] & LCD_UPPER_BITS) | LCD_BACKLIGHT | LCD_INSTR;
        uint8_t lower = ((init_8bit[i] & LCD_LOWER_BITS) << LCD_DATA_SHIFT)
                        | LCD_BACKLIGHT | LCD_INSTR;
        uint8_t buf[] = {upper | LCD_EN, upper, lower | LCD_EN, lower};

        i2c_master_write(buf, sizeof(buf), LCD_ADDR);
    }

    lcd_clear();

    return;
}

void lcd_print(char *input){
    (void) input;

    for (int i = 0; input[i] != '\0'; i++) {
        // characters are sent with RS=1, two 4-bit packets per character
        uint8_t upper = (input[i] & LCD_UPPER_BITS) | LCD_BACKLIGHT | LCD_DATA;
        uint8_t lower = (((input[i] & LCD_LOWER_BITS) << LCD_DATA_SHIFT)
                        | LCD_BACKLIGHT | LCD_DATA);
        uint8_t buf[] = {upper | LCD_EN, upper, lower | LCD_EN, lower};

        i2c_master_write(buf, sizeof(buf), LCD_ADDR);
    }

    return;
}

void lcd_set_cursor(uint8_t row, uint8_t col){
    (void) row;
    (void) col;

    // line 1 starts at 0x00, line 2 starts at 0x40
    uint8_t addr = (row * LCD_ROW1_OFFSET) + col;
    uint8_t instr = LCD_SET_DDRAM | addr;

    uint8_t upper = (instr & LCD_UPPER_BITS) | LCD_BACKLIGHT | LCD_INSTR;
    uint8_t lower = (((instr & LCD_LOWER_BITS) << LCD_DATA_SHIFT)
                    | LCD_BACKLIGHT | LCD_INSTR);
    uint8_t buf[] = {upper | LCD_EN, upper, lower | LCD_EN, lower};

    i2c_master_write(buf, sizeof(buf), LCD_ADDR);
}

void lcd_clear() {
    uint8_t upper = (LCD_CLEAR & LCD_UPPER_BITS) | LCD_BACKLIGHT | LCD_INSTR;
    uint8_t lower = ((LCD_CLEAR & LCD_LOWER_BITS) << LCD_DATA_SHIFT)
                    | LCD_BACKLIGHT | LCD_INSTR;
    uint8_t buf[] = {upper | LCD_EN, upper, lower | LCD_EN, lower};

    i2c_master_write(buf, sizeof(buf), LCD_ADDR);
    for (volatile int i = 0; i < LCD_CLEAR_DELAY; i++);
}
