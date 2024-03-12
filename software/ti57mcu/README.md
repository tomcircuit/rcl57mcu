# RCL57mcu - TI-57 emulator on an MCU

* Author: Tom LeMense 
* Toolchain: Keil uVision/MDK
* Target: STM32F103TB (CM3, 128K Flash, 20K RAM)
* Hardware: rcl57mcu PCB V2
* Repository: [github.com/tomcircuit/rcl57mcu](https://github.com/tomcircuit/rcl57mcu)
* Project Webpage: [hackaday.io/project/194963](https://hackaday.io/project/194963)

Rcl57mcu was inspired by the work of Jeff Parsons and Paul Novaes, from whose work I was able to study and then eventually develop a microcontroller-based TI-57 emulator. The rcl57mcu embedded software is more a less an embedded 'wrapper' around the "TI57 Engine" by Paul Novaes, which can be found in this repository: [github.com/n3times/rcl57](https://github.com/n3times/rcl57). 

Rcl57mcu is a hardware "drop-in" replacement for TI-57 TMC1500 DIP-28, and brings a few enhancements over the original:
 
- Calculation at 4x the speed of the original TI-57 
	+ display timing and PAUSE are same as original
- Non-volatile storage/retrieval of up to 20 user programs
- A "power save" mode if the keyboard is left idle
- USB micro-B rechargeable 3.7V LiPo battery 
- (tbc) Lower overall energy consumption

In theory, rcl57mcu could be used as speedup/retrofit in TI-55 and TI-42/"MBA" calculators, as they use the same TMC1500 IC. Of course, the correct ROM image would need to be used. I do not have these calculators, so I cannot say as to whether or not this actually works...

As part of the debugging activies, I used the "USART_Utilities" package from Saeid Yazdani, which I found at the following website [www.embedonix.com](http://www.embedonix.com) 

RCL57mcu hardware was designed by me, using the wonderful KiCAD 7.0 toolchain. Design files can be found at the GitHub repository.