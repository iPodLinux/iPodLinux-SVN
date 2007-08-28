#ifndef PLATFORM
#define PLATFORM
#define REG_IO_BASE            0x03FF0000
#define REG_SYSTEN_CONFIG  	   0x00
#define REG_SYSTEN_BUS_CLOCK   0x04

/*i/o control registers offset difinitions*/
#define REG_IO_CTRL0           0x4000
#define REG_IO_CTRL1           0x4004
#define REG_IO_CTRL2           0x4008
#define REG_IO_CTRL3           0x400C

/*memory control registers offset difinitions*/
#define REG_MEM_CTRL0          0x4010
#define REG_MEM_CTRL1          0x4014
#define REG_MEM_CTRL2          0x4018
#define REG_MEM_CTRL3          0x401C
#define REG_MEM_GENERAL        0x4020
#define REG_SDRAM_CTRL0        0x4030 
#define REG_SDRAM_CTRL1        0x4034
#define REG_SDRAM_GENERAL      0x4038
#define REG_SDRAM_BUFFER       0x403C
#define REG_SDRAM_REFRESH      0x4040    

/*WAN control registers offset difinitions*/
#define REG_WAN_DMA_TX         0x6000
#define REG_WAN_DMA_RX         0x6004
#define REG_WAN_DMA_TX_START   0x6008    
#define REG_WAN_DMA_RX_START   0x600C    
#define REG_WAN_TX_LIST        0x6010
#define REG_WAN_RX_LIST        0x6014
#define REG_WAN_MAC_LOW        0x6018
#define REG_WAN_MAC_HIGH       0x601C
#define REG_WAN_MAC_ELOW       0x6080
#define REG_WAN_MAC_EHIGH      0x6084

/*LAN control registers offset difinitions*/
#define REG_LAN_DMA_TX         0x8000
#define REG_LAN_DMA_RX         0x8004
#define REG_LAN_DMA_TX_START   0x8008    
#define REG_LAN_DMA_RX_START   0x800C    
#define REG_LAN_TX_LIST        0x8010
#define REG_LAN_RX_LIST        0x8014
#define REG_LAN_MAC_LOW        0x8018
#define REG_LAN_MAC_HIGH       0x801C
#define REG_LAN_MAC_ELOW       0x8080
#define REG_LAN_MAC_EHIGH      0x8084

/*HPNA control registers offset difinitions*/
#define REG_HPNA_DMA_TX        0xA000
#define REG_HPNA_DMA_RX        0xA004
#define REG_HPNA_DMA_TX_START  0xA008    
#define REG_HPNA_DMA_RX_START  0xA00C    
#define REG_HPNA_TX_LIST       0xA010
#define REG_HPNA_RX_LIST       0xA014
#define REG_HPNA_MAC_LOW       0xA018
#define REG_HPNA_MAC_HIGH      0xA01C
#define REG_HPNA_MAC_ELOW      0xA080
#define REG_HPNA_MAC_EHIGH     0xA084

/*UART control registers offset difinitions*/
#define REG_UART_RX_BUFFER     0xE000
#define REG_UART_TX_HOLDING    0xE004
#define REG_UART_FIFO_CTRL     0xE008
#define REG_UART_LINE_CTRL     0xE00C
#define REG_UART_MODEM_CTRL    0xE010
#define REG_UART_LINE_STATUS   0xE014
#define REG_UART_MODEM_STATUS  0xE018
#define REG_UART_DIVISOR       0xE01C
#define REG_UART_STATUS        0xE020

/*Interrupt controlller registers offset difinitions*/
#define REG_INT_CONTL          0xE200
#define REG_INT_ENABLE         0xE204
#define REG_INT_STATUS         0xE208
#define REG_INT_WAN_PRIORITY   0xE20C
#define REG_INT_HPNA_PRIORITY  0xE210
#define REG_INT_LAN_PRIORITY   0xE214
#define REG_INT_TIMER_PRIORITY 0xE218
#define REG_INT_UART_PRIORITY  0xE21C
#define REG_INT_EXT_PRIORITY   0xE220
#define REG_INT_CHAN_PRIORITY  0xE224
#define REG_INT_BUSERROR_PRO   0xE228
#define REG_INT_MASK_STATUS    0xE22C
#define REG_FIQ_PEND_PRIORITY  0xE230
#define REG_IRQ_PEND_PRIORITY  0xE234

/*timer registers offset difinitions*/
#define REG_TIMER_CTRL         0xE400
#define REG_TIMER1             0xE404
#define REG_TIMER0             0xE408
#define REG_TIMER1_PCOUNT      0xE40C
#define REG_TIMER0_PCOUNT      0xE410

/*GPIO registers offset difinitions*/
#define REG_GPIO_MODE          0xE600
#define REG_GPIO_CTRL          0xE604
#define REG_GPIO_DATA          0xE608

/*SWITCH registers offset difinitions*/
#define REG_SWITCH_CTRL0       0xE800
#define REG_SWITCH_CTRL1       0xE804
#define REG_SWITCH_PORT1       0xE808
#define REG_SWITCH_PORT2       0xE80C
#define REG_SWITCH_PORT3       0xE810
#define REG_SWITCH_PORT4       0xE814
#define REG_SWITCH_PORT5       0xE818
#define REG_SWITCH_AUTO0       0xE81C
#define REG_SWITCH_AUTO1       0xE820
#define REG_SWITCH_LUE_CTRL    0xE824
#define REG_SWITCH_LUE_HIGH    0xE828
#define REG_SWITCH_LUE_LOW     0xE82C
#define REG_SWITCH_ADVANCED    0xE830

/*host communication registers difinitions*/
#define REG_DSCP_HIGH          0xE834
#define REG_DSCP_LOW           0xE838
#define REG_SWITCH_MAC_HIGH    0xE83C
#define REG_SWITCH_MAC_LOW     0xE840

/*miscellaneours registers difinitions*/
#define REG_MANAGE_COUNTER     0xE844
#define REG_MANAGE_DATA        0xE848
#define REG_DEVICE_ID          0xEA00
#define REG_REVISION_ID        0xEA04

#define REG_HPNA_CONTROL       0xEA08
#define REG_WAN_CONTROL        0xEA0C
/*newly added registers*/
#define REG_WAN_POWERMAGR      0xEA10
#define REG_WAN_PHY_CONTROL    0xEA14
#define REG_WAN_PHY_STATUS     0xEA18

#define IO_READ(p)         ((*(volatile unsigned int *)(REG_IO_BASE + p)))
#define IO_WRITE(p, c)     (*(unsigned int *)(REG_IO_BASE + (unsigned int)p) = (c))

#endif
