/*
 * The MySensors Arduino library handles the wireless radio link and protocol
 * between your home built sensors/actuators and HA controller of choice.
 * The sensors forms a self healing radio network with optional repeaters. Each
 * repeater and gateway builds a routing tables in EEPROM which keeps track of the
 * network topology allowing messages to be routed to nodes.
 *
 * Created by Henrik Ekblad <henrik.ekblad@mysensors.org>
 * Copyright (C) 2013-2020 Sensnology AB
 * Full contributor list: https://github.com/mysensors/MySensors/graphs/contributors
 *
 * Documentation: http://www.mysensors.org
 * Support Forum: http://forum.mysensors.org
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

#include "MyHwSTM32F1.h"

#include <STM32Sleep.h>
#include <RTClock.h>
#include <boards.h>
#include "boards_private.h"


/*
* Pinout STM32F103C8 dev board:
* http://wiki.stm32duino.com/images/a/ae/Bluepillpinout.gif
*
* Wiring RFM69 radio / SPI1
* --------------------------------------------------
* CLK	PA5
* MISO	PA6
* MOSI	PA7
* CSN	PA4
* CE	NA
* IRQ	PA3 (default)
*
* Wiring RF24 radio / SPI1
* --------------------------------------------------
* CLK	PA5
* MISO	PA6
* MOSI	PA7
* CSN	PA4
* CE	PB0 (default)
* IRQ	NA
*
*/
bool hwInit(void)
{
#if !defined(MY_DISABLED_SERIAL)
	MY_SERIALDEVICE.begin(MY_BAUD_RATE);
#if defined(MY_GATEWAY_SERIAL)
	while (!MY_SERIALDEVICE) {}
#endif
#endif
	if (EEPROM.init() == EEPROM_OK) {
		uint16 cnt;
		EEPROM.count(&cnt);
		if(cnt>=EEPROM.maxcount()) {
			// tmp, WIP: format eeprom if full
			EEPROM.format();
		}
		return true;
	}
	return false;
}

void hwReadConfigBlock(void *buf, void *addr, size_t length)
{
	uint8_t *dst = static_cast<uint8_t *>(buf);
	int pos = reinterpret_cast<int>(addr);
	while (length-- > 0) {
		*dst++ = EEPROM.read(pos++);
	}
}

void hwWriteConfigBlock(void *buf, void *addr, size_t length)
{
	uint8_t *src = static_cast<uint8_t *>(buf);
	int pos = reinterpret_cast<int>(addr);
	while (length-- > 0) {
		EEPROM.write(pos++, *src++);
	}
}

uint8_t hwReadConfig(const int addr)
{
	uint8_t value;
	hwReadConfigBlock(&value, reinterpret_cast<void *>(addr), 1);
	return value;
}

void hwWriteConfig(const int addr, uint8_t value)
{
	hwWriteConfigBlock(&value, reinterpret_cast<void *>(addr), 1);
}


static void setup_clocks(void) {
    // Turn on HSI. We'll switch to and run off of this while we're
    // setting up the main PLL.
    rcc_turn_on_clk(RCC_CLK_HSI);

    // Turn off and reset the clock subsystems we'll be using, as well
    // as the clock security subsystem (CSS). Note that resetting CFGR
    // to its default value of 0 implies a switch to HSI for SYSCLK.
    RCC_BASE->CFGR = 0x00000000;
    rcc_disable_css();
    rcc_turn_off_clk(RCC_CLK_PLL);
    rcc_turn_off_clk(RCC_CLK_HSE);
    wirish::priv::board_reset_pll();
    // Clear clock readiness interrupt flags and turn off clock
    // readiness interrupts.
    RCC_BASE->CIR = 0x00000000;
#if !USE_HSI_CLOCK
    // Enable HSE, and wait until it's ready.
    rcc_turn_on_clk(RCC_CLK_HSE);
    while (!rcc_is_clk_ready(RCC_CLK_HSE))
        ;
#endif
    // Configure AHBx, APBx, etc. prescalers and the main PLL.
    wirish::priv::board_setup_clock_prescalers();
    rcc_configure_pll(&wirish::priv::w_board_pll_cfg);

    // Enable the PLL, and wait until it's ready.
    rcc_turn_on_clk(RCC_CLK_PLL);
    while(!rcc_is_clk_ready(RCC_CLK_PLL))
        ;

    // Finally, switch to the now-ready PLL as the main clock source.
    rcc_switch_sysclk(RCC_CLKSRC_PLL);
}

#ifndef STM32_SLEEPMODE
#define STM32_SLEEPMODE STOP
#endif
RTClock hw_rt(RTCSEL_LSE);
int8_t hwSleep(uint32_t ms)
{
	if (ms >= 1000)
	{
		systick_disable();
		gpio_pin_mode gpioABack[16], gpioBBack[16], gpioCBack[16];
		for (char i = 0; i < 16; ++i) {
			gpioABack[i] = gpio_get_mode(GPIOA, i);
			gpioBBack[i] = gpio_get_mode(GPIOB, i);
			gpioCBack[i] = gpio_get_mode(GPIOC, i);
		}
		setGPIOModeToAllPins(GPIO_INPUT_ANALOG);
		
		sleepAndWakeUp(STM32_SLEEPMODE, &hw_rt, (uint32_t)(ms/1000UL));
		setup_clocks();
		systick_enable();
		for (char i = 0; i < 16; ++i) {
			gpio_set_mode(GPIOA, i, gpioABack[i]);
			gpio_set_mode(GPIOB, i, gpioBBack[i]);
			gpio_set_mode(GPIOC, i, gpioCBack[i]);
		}
		delay(10);
		return MY_WAKE_UP_BY_TIMER;
	}

	// TODO: short sleep not supported!
	return MY_SLEEP_NOT_POSSIBLE;
}

int8_t hwSleep(const uint8_t interrupt, const uint8_t mode, uint32_t ms)
{
	// TODO: Not supported!
	(void)interrupt;
	(void)mode;
	(void)ms;
	return MY_SLEEP_NOT_POSSIBLE;
}

int8_t hwSleep(const uint8_t interrupt1, const uint8_t mode1, const uint8_t interrupt2,
               const uint8_t mode2,
               uint32_t ms)
{
	// TODO: Not supported!
	(void)interrupt1;
	(void)mode1;
	(void)interrupt2;
	(void)mode2;
	(void)ms;
	return MY_SLEEP_NOT_POSSIBLE;
}


void hwRandomNumberInit(void)
{
	// use internal temperature sensor as noise source
	adc_reg_map *regs = ADC1->regs;
	regs->CR2 |= ADC_CR2_TSVREFE;
	regs->SMPR1 |= ADC_SMPR1_SMP16;

	uint32_t seed = 0;
	uint16_t currentValue = 0;
	uint16_t newValue = 0;

	for (uint8_t i = 0; i < 32; i++) {
		const uint32_t timeout = hwMillis() + 20;
		while (timeout >= hwMillis()) {
			newValue = adc_read(ADC1, 16);
			if (newValue != currentValue) {
				currentValue = newValue;
				break;
			}
		}
		seed ^= ( (newValue + hwMillis()) & 7) << i;
	}
	randomSeed(seed);
	regs->CR2 &= ~ADC_CR2_TSVREFE; // disable VREFINT and temp sensor
}

bool hwUniqueID(unique_id_t *uniqueID)
{
	(void)memcpy((uint8_t *)uniqueID, (uint32_t *)0x1FFFF7E0, 16); // FlashID + ChipID
	return true;
}

uint16_t hwCPUVoltage(void)
{
	adc_reg_map *regs = ADC1->regs;
	regs->CR2 |= ADC_CR2_TSVREFE; // enable VREFINT and temp sensor
	regs->SMPR1 =  ADC_SMPR1_SMP17; // sample rate for VREFINT ADC channel
	adc_calibrate(ADC1);

	const uint16_t vdd = adc_read(ADC1, 17);
	regs->CR2 &= ~ADC_CR2_TSVREFE; // disable VREFINT and temp sensor
	return (uint16_t)(1200u * 4096u / vdd);
}

uint16_t hwCPUFrequency(void)
{
	return F_CPU/100000UL;
}

int8_t hwCPUTemperature(void)
{
	adc_reg_map *regs = ADC1->regs;
	regs->CR2 |= ADC_CR2_TSVREFE; // enable VREFINT and Temperature sensor
	regs->SMPR1 |= ADC_SMPR1_SMP16 | ADC_SMPR1_SMP17;
	adc_calibrate(ADC1);

	//const uint16_t adc_temp = adc_read(ADC1, 16);
	//const uint16_t vref = 1200 * 4096 / adc_read(ADC1, 17);
	// calibrated at 25°C, ADC output = 1430mV, avg slope = 4.3mV / °C, increasing temp ~ lower voltage
	const int8_t temp = static_cast<int8_t>((1430.0 - (adc_read(ADC1, 16) * 1200 / adc_read(ADC1,
	                                        17))) / 4.3 + 25.0);
	regs->CR2 &= ~ADC_CR2_TSVREFE; // disable VREFINT and temp sensor
	return (temp - MY_STM32F1_TEMPERATURE_OFFSET) / MY_STM32F1_TEMPERATURE_GAIN;
}

uint16_t hwFreeMem(void)
{
	//Not yet implemented
	return FUNCTION_NOT_SUPPORTED;
}
