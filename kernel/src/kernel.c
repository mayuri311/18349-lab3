/**
 * @file kernel.c
 *
 * @brief      Kernel entry point
 *
 * @date
 * @author
 */

#include <arm.h>
#include <kernel.h>

#include <printk.h>
#include <systick.h>
#include <uart.h>

#define BAUD_RATE 115200

#define PRINT_PERIOD_MS 1000


int kernel_main( void ) {
  init_349(); // DO NOT REMOVE THIS LINE

 
  uart_init( BAUD_RATE );
  systick_init();
 
  while ( 1 ) {
    printk( "Interrupt!\n" );
    systick_delay( PRINT_PERIOD_MS );
  }
 

  return 0;
}
