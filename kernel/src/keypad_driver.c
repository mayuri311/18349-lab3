#include <unistd.h>

#include <gpio.h>
#include <keypad_driver.h>

#define KEYPAD_NUM_ROWS 4
#define KEYPAD_NUM_COLS 3

#define KEYPAD_SETTLE_CYCLES 100

// row pins D2, D4, D6, D9
#define KEYPAD_ROW1_PORT GPIO_A
#define KEYPAD_ROW1_PIN  10
#define KEYPAD_ROW2_PORT GPIO_B
#define KEYPAD_ROW2_PIN  5
#define KEYPAD_ROW3_PORT GPIO_B
#define KEYPAD_ROW3_PIN  10
#define KEYPAD_ROW4_PORT GPIO_C
#define KEYPAD_ROW4_PIN  7

// col pins D10, D11, D12
#define KEYPAD_COL1_PORT GPIO_B
#define KEYPAD_COL1_PIN  6
#define KEYPAD_COL2_PORT GPIO_A
#define KEYPAD_COL2_PIN  7
#define KEYPAD_COL3_PORT GPIO_A
#define KEYPAD_COL3_PIN  6

static const gpio_port row_ports[KEYPAD_NUM_ROWS] = {
    KEYPAD_ROW1_PORT, KEYPAD_ROW2_PORT, KEYPAD_ROW3_PORT, KEYPAD_ROW4_PORT
};
static const unsigned int row_pins[KEYPAD_NUM_ROWS] = {
    KEYPAD_ROW1_PIN, KEYPAD_ROW2_PIN, KEYPAD_ROW3_PIN, KEYPAD_ROW4_PIN
};

static const gpio_port col_ports[KEYPAD_NUM_COLS] = {
    KEYPAD_COL1_PORT, KEYPAD_COL2_PORT, KEYPAD_COL3_PORT
};
static const unsigned int col_pins[KEYPAD_NUM_COLS] = {
    KEYPAD_COL1_PIN, KEYPAD_COL2_PIN, KEYPAD_COL3_PIN
};

//map
static const char keymap[KEYPAD_NUM_ROWS][KEYPAD_NUM_COLS] = {
    {'1', '2', '3'},
    {'4', '5', '6'},
    {'7', '8', '9'},
    {'*', '0', '#'}
};

//cols are open drain, col to drive low
//-1 to release all columns
static void set_col(int col) {
    for (int c = 0; c < KEYPAD_NUM_COLS; c++) {
        if (c == col) {
            gpio_clr(col_ports[c], col_pins[c]);
        } else {
            gpio_set(col_ports[c], col_pins[c]);
        }
    }
}

//rows held high by pu
//1 if no key pressed in this row, 0 if one is
static int read_row(int row) {
    return gpio_read(row_ports[row], row_pins[row]);
}

void keypad_init() {
    // row idle high, key press drags low
    for (int r = 0; r < KEYPAD_NUM_ROWS; r++) {
        gpio_init(row_ports[r], row_pins[r], MODE_INPUT,
                  OUTPUT_PUSH_PULL, OUTPUT_SPEED_LOW, PUPD_PULL_UP, ALT0);
    }

    // idle col floats
    for (int c = 0; c < KEYPAD_NUM_COLS; c++) {
        gpio_init(col_ports[c], col_pins[c], MODE_GP_OUTPUT,
                  OUTPUT_OPEN_DRAIN, OUTPUT_SPEED_LOW, PUPD_NONE, ALT0);
    }

    set_col(-1);
}

char keypad_read() {
    static char last_key = '\0';
    char current_key = '\0';

    for (int c = 0; c < KEYPAD_NUM_COLS && current_key == '\0'; c++) {
        set_col(c);
        for (volatile int d = 0; d < KEYPAD_SETTLE_CYCLES; d++);

        for (int r = 0; r < KEYPAD_NUM_ROWS; r++) {
            if (read_row(r) == 0) {
                current_key = keymap[r][c];
                break;
            }
        }
    }

    set_col(-1);

    //debounce: if the key is the same as last time, return null
    if (current_key == last_key) {
        return '\0';
    }

    last_key = current_key;
    return current_key;
}