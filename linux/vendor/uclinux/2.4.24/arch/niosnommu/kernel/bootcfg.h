/* common parameters */

/* CPU frequency */
#define CPU_FREQ  20000000

/* desired uart baud rate */
#define BAUD_RATE 9600

/* RAM window start */
#define RAM_WINDOW 0x40000000

/* memory size of each ram bank (bytes) */
#define RAM_BANK_SIZE 0x80000

/* number of memory banks */
#define RAM_BANKS 1

/* init value for memory configuration register 1, PROM 3ws, 2M*/
#define MEMCFG1_VAL	0x00020033

/* init value for memory configuration register 2, RAM  2ws, 512k */
/* DEPENDS ON RAM_BANK_SIZE!!! */
#define MEMCFG2_VAL 0xc0a


/* AUTOMATICALLY GENERATED */

/* init value for timer pre-scaler */
#define TIMER_SCALER_VAL (CPU_FREQ -1)

/* init value for uart pre-scaler */
#define UART_SCALER_VAL  ((CPU_FREQ / (8 * BAUD_RATE)) - 1)

/* stack pointer for boot application */
#define STACK_INIT	(((RAM_BANK_SIZE * RAM_BANKS) - 16) + RAM_WINDOW)
