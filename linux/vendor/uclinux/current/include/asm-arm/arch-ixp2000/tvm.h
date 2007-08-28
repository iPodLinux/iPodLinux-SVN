/*
 * inclue/asm-arm/arch-ixp2000/tvm.h
 *
 * Register and other defines for ADuC812 device
 *
 * Author: Naeem Afzal <naeem.m.afzal@intel.com>
 *
 * Copyright 2002 Intel Corp.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */
#ifndef TVM_H
#define TVM_H

#define IXP2400_CAP_BASE        CAP_BASE
#define IXP2400_GPIO_BASE       (IXP2400_CAP_BASE  + 0x10000)


/*---------------------------------------------------
 * IXDP2400 GPIO...
 * --------------------------------------------------*/
#define IXP2400_GPIO_BASE       (IXP2400_CAP_BASE  + 0x10000)
#define IXP2400_GPIO_PLR        (IXP2400_GPIO_BASE + 0x0)  /*Read only register to read the level/data on the pins irrespective of input or output*/
#define IXP2400_GPIO_PDSR       (IXP2400_GPIO_BASE + 0x08) /*write a one to this register's bit to configure the corressponding pin as output*/
#define IXP2400_GPIO_POPR   (IXP2400_GPIO_BASE + 0x10)
#define IXP2400_GPIO_PDCR       (IXP2400_GPIO_BASE + 0x0c) /*write a one to this register's bit to configure corressponding bit as input*/
#define IXP2400_GPIO_POSR       (IXP2400_GPIO_BASE + 0x14) /*write to this register's bit to set data high*/
#define IXP2400_GPIO_POCR       (IXP2400_GPIO_BASE + 0x18) /*write to this register's bit to set data low*/


#define LOW 0
#define HIGH 1

/*write this to POSR to drive the clock pin (GPIO pin 7)High and POCR for LOW*/
#define CLOCK   0x80
/*write this to POSR to drive the data pin (gpio pin 6)High and POCR for LOW*/
#define DATA    0x40

/*This GPIO bit 2 has to be configured as output and
 *drive a high to cause a pull up on the SDA line.
 */
#define I2C_INIT_BIT 0x04


/* from vxworks include */
typedef enum volt_id {
        ADUC_3_3V_M_ID  = 1,
        ADUC_2_5V_M_ID,
        ADUC_3_3V_ID,
        ADUC_2_5V_ID,
        ADUC_1_8V_ID,
        ADUC_1_5V_ID,
        ADUC_1_3V_ID,
        ADUC_1_2_5V_ID
} volt_id;

typedef enum temp_id {
        M_NPU_ID        = 1,
        S_NPU_ID
} temp_id;

typedef enum which_limit {
        MONITOR         = 0,
        HIGH_LIMIT,
        LOW_LIMIT
} which_limit;

typedef struct  tvm_read {
        int identifier;
        int limit;
        int value;
} tvm_read_t;

typedef struct  tvm_write {
        int identifier;
        int value;
        int limit;
} tvm_write_t;


/* these resolutions are in uV */
#define ADUC_2_5V_REF_RES       610
#define ADUC_5V_REF_RES         1221

#define ADUC812_DEV_ID_OFF      0x0
#define TEMP_DEV_ID_OFF         0x29
#define INT_OFF                 0x30
#define ADUC812_COMMAND         0x34

#define ADUC812_DEV_ID          0x8C
#define TEMP_DEV_ID              0x31

#define SHUTDOWN_COMMAND        1
#define CLEAR_INT               2

#define VOLT_A_TO_D_BITS        12
#define TEMP_A_TO_D_BITS        8

/* aduc interrupt defines */
#define ADUC_3_3V_M_HIGH                (1 << 0)
#define ADUC_3_3V_M_LOW                 (1 << 1)
#define ADUC_3_3V_HIGH                  (1 << 2)
#define ADUC_3_3V_LOW                   (1 << 3)
#define ADUC_2_5V_M_HIGH                (1 << 4)
#define ADUC_2_5V_M_LOW                 (1 << 5)
#define ADUC_2_5V_HIGH                  (1 << 6)
#define ADUC_2_5V_LOW                   (1 << 7)
#define ADUC_1_8V_HIGH                  (1 << 8)
#define ADUC_1_8V_LOW                   (1 << 9)
#define ADUC_1_5V_HIGH                  (1 << 10)
#define ADUC_1_5V_LOW                   (1 << 11)
#define ADUC_1_3V_HIGH                  (1 << 12)
#define ADUC_1_3V_LOW                   (1 << 13)
#define ADUC_1_2_5V_HIGH                (1 << 14)
#define ADUC_1_2_5V_LOW                 (1 << 15)
#define ADUC_MASTER_TEMP_HIGH   (1 << 16)
#define ADUC_MASTER_TEMP_LOW    (1 << 17)
#define ADUC_SLAVE_TEMP_HIGH    (1 << 18)
#define ADUC_SLAVE_TEMP_LOW             (1 << 19)

/* from vxworks */

#define SWAP32(x)               \
  (((x) << 24) |                \
  (((x) & 0x0000FF00) << 8) |   \
  (((x) & 0x00FF0000) >> 8) |   \
  (((unsigned int)(x)) >> 24))

#define SWAP16(x) ((((x) << 8) | ((x) >> 8)) & 0xFFFF)

#define TVM_MINOR       201 /* for now */

#define	TVM_VOLT_RD		0   
#define	TVM_VOLT_WR		1  
#define	TVM_TEMP_RD		2   
#define	TVM_TEMP_WR		3  

static int tvm_open(struct inode *, struct file *);
static int tvm_release(struct inode *, struct file *);
static int tvm_ioctl(struct inode *, struct file *, unsigned int, unsigned long);
static void tvm_isr(int, void *, struct pt_regs *);
extern int i2c_master_send(struct i2c_client *,const char* ,int);
extern int i2c_master_recv(struct i2c_client *,char* ,int);

static void config_ixmb2400(void);
static int tvm_init(void);
//static int ixmb2400_init(void);
static void ixmb2400_inc(struct i2c_adapter *adapter);
static void ixmb2400_dec(struct i2c_adapter *adapter);
static int volt_limit_set(volt_id identifier, int value, which_limit limit);
static int temp_limit_set(temp_id identifier, int value, which_limit limit);


#endif /* TVM_H */
