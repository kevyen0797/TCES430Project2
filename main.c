//Part 2
//Final Project
//
//Kevin Nguyen, Dino Jazvin, Denis Alekhin

//include files
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include "hw_i2c.h"
#include "hw_memmap.h"
#include "hw_types.h"
#include "hw_gpio.h"
#include "i2c.h"
#include "sysctl.h"
#include "gpio.h"
#include "pin_map.h"

//Defines for DS1307
#define SLAVE_ADDRESS 0x68
#define SEC 0x00
#define MIN 0x01
#define HRS 0x02
#define DAY 0x03
#define DATE 0x04
#define MONTH 0x05
#define YEAR 0x06
#define CNTRL 0x07

//initialize I2C module 0
void InitI2C2(void)
{
	//enable I2C module 2
	SysCtlPeripheralEnable(SYSCTL_PERIPH_I2C2);

	//reset module
	SysCtlPeripheralReset(SYSCTL_PERIPH_I2C2);

	//enable GPIO peripheral that contains I2C 2
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPION);

	// Configure the pin muxing for I2C2 functions on port B2 and B3.
	GPIOPinConfigure(GPIO_PN5_I2C2SCL);
	GPIOPinConfigure(GPIO_PN4_I2C2SDA);

	// Select the I2C function for these pins.
	GPIOPinTypeI2CSCL(GPIO_PORTN_BASE, GPIO_PIN_5);
	GPIOPinTypeI2C(GPIO_PORTN_BASE, GPIO_PIN_4);

	// Enable and initialize the I2C2 master module.
	I2CMasterInitExpClk(I2C2_BASE, SysCtlClockGet(), false);

	//clear I2C FIFOs
	HWREG(I2C2_BASE + I2C_O_FIFOCTL) = 80008000;
}

//sends commands to the I2C slave
void I2CSend(uint8_t slave_addr, uint8_t num_of_args, ...)
{
	//Sets the slave address on the bus
	I2CMasterSlaveAddrSet(I2C2_BASE, slave_addr, false);

	//stores list of variable number of arguments
	va_list vargs;
	va_start(vargs, num_of_args);

	//put data to be sent into FIFO
	I2CMasterDataPut(I2C2_BASE, va_arg(vargs, uint32_t));

	//if there is only one argument, we only need to use the
	//single send I2C function
	if(num_of_args == 1)
	{
		//Send data from the master control unit
		I2CMasterControl(I2C2_BASE, I2C_MASTER_CMD_SINGLE_SEND);
		//Wait until master is done transferring.
		while(I2CMasterBusy(I2C2_BASE));
		va_end(vargs);
	}

	//if more than one argument send multiple bytes
	else
	{
		//Send data from the master control unit
		I2CMasterControl(I2C2_BASE, I2C_MASTER_CMD_BURST_SEND_START);

		//Wait until master is done transferring.
		while(I2CMasterBusy(I2C2_BASE));

		//Use BURST_SEND_CONT to continue sending data
		unsigned char i;
		for(i = 1; i < (num_of_args - 1); i++)
		{
			//put next piece of data into I2C FIFO
			I2CMasterDataPut(I2C2_BASE, va_arg(vargs, uint32_t));

			//send next data that was just placed into FIFO
			I2CMasterControl(I2C2_BASE, I2C_MASTER_CMD_BURST_SEND_CONT);

			// Wait until MCU is done transferring.
			while(I2CMasterBusy(I2C2_BASE));
		}
		//put last piece of data into I2C FIFO
		I2CMasterDataPut(I2C2_BASE, va_arg(vargs, uint32_t));

		//send next data that was just placed into FIFO
		I2CMasterControl(I2C2_BASE, I2C_MASTER_CMD_BURST_SEND_FINISH);

		// Wait until MCU is done transferring.
		while(I2CMasterBusy(I2C2_BASE));

		//"close" variable args list
		va_end(vargs);
	}
}

//read specified register on slave device
uint32_t I2CReceive(uint32_t slave_addr, uint8_t reg)
{
	//Set the slave address on the bus
	I2CMasterSlaveAddrSet(I2C2_BASE, slave_addr, false);

	//Specify register at slave address
	I2CMasterDataPut(I2C2_BASE, reg);

	//send control byte and register address byte to slave device
	I2CMasterControl(I2C2_BASE, I2C_MASTER_CMD_BURST_SEND_START);

	//Wait until the master finishes sending
	while(I2CMasterBusy(I2C2_BASE));

	//Set the slave address to be read
	I2CMasterSlaveAddrSet(I2C2_BASE, slave_addr, true);

	//send control byte and read from the register we specified
	I2CMasterControl(I2C2_BASE, I2C_MASTER_CMD_SINGLE_RECEIVE);

	//wait for MCU to finish transaction
	while(I2CMasterBusy(I2C2_BASE));

	//return data pulled from the specified register
	return I2CMasterDataGet(I2C2_BASE);
}

// convert decimal to BCD
unsigned char decConv(unsigned char time)
{
	return (((time / 10) << 4) | (time));
}

// convert BCD to decimal
unsigned char bcdConv(unsigned char time)
{
	return (((time & 0xF0) >> 4) * 10) + (time & 0x0F);
}

//Set Time
void SetTimeDate(unsigned char sec, unsigned char min, unsigned char hour)
{
	I2CSend(SLAVE_ADDRESS,8,SEC,decConv(sec),decConv(min),decConv(hour));
}

//Read time at the given RTC register
unsigned char GetClock(unsigned char reg)
{
	unsigned char dataIn = I2CReceive(SLAVE_ADDRESS,reg);
	return bcdConv(dataIn);
}

void main(void)
{
	//Configure the system clock
	SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_OSC_INT | SYSCTL_XTAL_16MHZ);

	//initialize I2C module 2
	InitI2C2();

	//Set the time
	SetTimeDate(24,24,6);
	unsigned char sec,min,hour;

	//Continuously read the second, minute, and hour from the RTC
	while(1)
	{
		sec = GetClock(SEC);
		min = GetClock(MIN);
		hour = GetClock(HRS);
		SysCtlDelay(SysCtlClockGet()/10*3);
	}
}
