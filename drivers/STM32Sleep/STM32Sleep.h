#ifndef STM32SLEEP_H
#define STM32SLEEP_H
#include <libmaple/gpio.h>
#include <RTClock.h>

typedef enum SleepMode {
  STOP,
  STANDBY
} SleepMode;

void sleepAndWakeUp(SleepMode mode, RTClock *rt, uint8_t seconds);
void sleepAndWakeUp(SleepMode mode);
bool wokeUpFromStandby();

void goToSleep(SleepMode mode);

void disableAllPeripheralClocks();

void setGPIOModeToAllPins(gpio_pin_mode mode);

void switchToPLLwithHSE(rcc_pll_multiplier pllMultiplier);
#endif
