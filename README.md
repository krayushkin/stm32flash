This is my version of stm32flash utility
========================================

Changelog
---------

* replace old /sys/ gpio interface with new userspace linux gpio api
* fix error that I get in my setup
* add `-G <gpiochip_name>` option for specifying gpiochip name

Tested only with STM32F103C8, STM32F105RB, STM32F030C8 mcu, but must for other stm32flash supported microcontrollers.
Thanks to original authors of 

Usage
-----

In init section specify gpiochip name with `-G <name>` option. For example:

`stm32flash -G gpiochip0 -i -12,11,-11:12,11,-11 -R -w STM32F105RB.hex /dev/ttyS1`

In example above Line 12 is connected to boot0 pin (boot1 is tied to 1), Line 11 to rst pin.
We automatically reset mcu, enter boot mode, write `STM32F105RB.hex`, exit boot mode and run program.
