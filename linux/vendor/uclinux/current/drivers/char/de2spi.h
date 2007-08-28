#ifndef _DE2SPI_H_
#define _DE2SPI_H_

/* Sysclock divider */
#define SPICLK_SYSCLK_4   (0)
#define SPICLK_SYSCLK_8   (1)
#define SPICLK_SYSCLK_16  (2)
#define SPICLK_SYSCLK_32  (3)
#define SPICLK_SYSCLK_64  (4)
#define SPICLK_SYSCLK_128 (5)
#define SPICLK_SYSCLK_256 (6)
#define SPICLK_SYSCLK_512 (7)

int de2spi_attach(int div);
void de2spi_detach(void);
int de2spi_exchange(unsigned short data, int bitcount);

#endif
