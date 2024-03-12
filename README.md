# rcl57mcu

## MCU-based retrofit for TI-57 Programmable Calculators

This project came about because I stumbled across a TI-57 calculator with a "bad brain". The TMC1501 calculator-on-a-chip was faulty. This, in turn, led me to read more about this particular calculator and how the CPU inside functions. 

Fortunately, I am 'standing on the shoulders of giants' here - three people that have been of great help to me are Joerg Woerner, Jeff Parsons and Paul Novaes. All have humored me by answering my emails and helping me understand how the hardware works. 

Joerg's DATAMATH pages are amazing reading, and in particular there is an excellent [overview of the TMC1501 calculator-on-a-chip IC](http://www.datamath.org/Chips/TMC1500.htm) used in the TI-57.

Jeff Parsons' TI-57 pages include an online emulator and lots of history on TI-57 development. Extremely helpful is his [collection and analysis of original Texas Instruments patents](https://www.pcjs.org/machines/ti/ti57/patents/) filed around the introduction of this calculator.

Both Jeff and Paul have created TI-57 emulators. Paul's is an iOS app, and Jeff's is browser-based. Both emulate the TMC1501 CPU innards, which in turn execute the original TI-57 ROM code. So it is a faithful 'emulation'.

Paul was kind enough to put his [iOS app code](https://github.com/n3times/rcl57) up on GitHub

The 'guts' of the TMC1500 emulation are all contained within the 'engine' folder, and written in very generic and portable C. I learned an awful lot from studying this code.

## The Quest

I began to hatch a scheme - could I put a modern microcontroller (MCU) to the task of emulating the 1970's TMC1501? Could this hardware emulator be then installed into my defunct TI-57 to make it live again, perhaps even better than before?

See the hackaday page for more info on this project: 

https://hackaday.io/project/194963-ti-57-programmable-calculator-hardware-retrofit

Tom LeMense
February 2024
