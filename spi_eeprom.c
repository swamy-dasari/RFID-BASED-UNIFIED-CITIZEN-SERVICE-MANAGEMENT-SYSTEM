/******************************************************************************
 * @file    spi_eeprom.c
 * @brief   Microchip 25LC512 SPI EEPROM driver implementation.
 *          Implements dynamic pin switching to prevent conflicts between 
 *          the LCD data bus and the SPI0 peripheral lines.
 * @author  RFID Pair Programming Team
 * @date    July 2026
 ******************************************************************************/

#include <LPC21xx.h>
#include "types.h"
#include "spi_defines.h"
#include "spi_eeprom_defines.h"
#include "delay.h"

/**
 * @brief  Sends a command instruction to the SPI EEPROM (e.g. WREN, WRDI).
 * @param  cmd EEPROM command code byte.
 */
void Cmd_25LC512(u8 cmd)
{
  IOCLR0=1<<CS;
  SPI0(cmd);//issue WREN/WRDI
  IOSET0=1<<CS;
}

/**
 * @brief  Writes a byte of data to a specific 16-bit EEPROM address.
 *         Includes dynamic pin redirection and write-cycle delay.
 * @param  wBufAddr 16-bit EEPROM destination memory address.
 * @param  dat Data byte to write.
 */
void ByteWrite_25LC512(u16 wBufAddr,u8 dat)
{
  // Switch P0.4 - P0.6 pins dynamically to SPI0 function
  PINSEL0 &= ~0x00003F00;
  PINSEL0 |= 0x00001500;

  Cmd_25LC512(WREN);

  IOCLR0=1<<CS;

  SPI0(WRITE); 

  SPI0(wBufAddr>>8);

  SPI0(wBufAddr);

  SPI0(dat);

  IOSET0=1<<CS;

  delay_ms(10);

  Cmd_25LC512(WRDI);

  // Restore P0.4 - P0.6 pins to standard GPIO output mode for LCD data
  PINSEL0 &= ~0x00003F00;
  IODIR0 |= 0x00000FF0;
}

/**
 * @brief  Reads a byte of data from a specific 16-bit EEPROM address.
 *         Includes dynamic pin redirection to avoid bus conflicts.
 * @param  rBufAddr 16-bit EEPROM source memory address.
 * @return Retrieved data byte from EEPROM.
 */
u8 ByteRead_25LC512(u16 rBufAddr)
{
  u8 dat;

  // Switch P0.4 - P0.6 pins dynamically to SPI0 function
  PINSEL0 &= ~0x00003F00;
  PINSEL0 |= 0x00001500;

  IOCLR0=1<<CS;

  SPI0(READ);   

  SPI0(rBufAddr>>8);

  SPI0(rBufAddr);   

  dat=SPI0(0x00);

  IOSET0=1<<CS;

  // Restore P0.4 - P0.6 pins to standard GPIO output mode for LCD data
  PINSEL0 &= ~0x00003F00;
  IODIR0 |= 0x00000FF0;

  return dat;   
}
