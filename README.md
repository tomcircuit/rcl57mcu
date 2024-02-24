# rcl57mcu

## MCU-based retrofit for TI-57 Programmable Calculators

This project came about because I stumbled across a TI-57 calculator with a "bad brain". The TMC1501 calculator-on-a-chip was faulty. This, in turn, led me to read more about this particular calculator and how the CPU inside functions. 

Fortunately, I am 'standing on the shoulders of giants' here - two people that have been of great help to me are Jeff Parsons and Paul Novaes. Both of humored me by answering my emails and helping me understand how the hardware works. 

Jeff's TI-57 pages are found here, and extremely helpful is his collection of original Texas Instruments patents filed around the introduction of this calculator:

https://www.pcjs.org/machines/ti/ti57/patents/

Both Jeff and Paul have created TI-57 emulators. Paul's is an iOS app, and Jeff's is browser-based. Both emulate the TMC1501 CPU innards, which in turn execute the original TI-57 ROM code. So it is a faithful 'emulation'.

Paul was kind enough to put his iOS app code up on GitHub:

https://github.com/n3times/rcl57

The 'guts' of the TMC1500 emulation are all contained within the 'engine' folder, and written in very generic and portable C. I learned an awful lot from studying this code.

## The Quest

I began to hatch a scheme - could I put a modern microcontroller (MCU) to the task of emulating the 1970's TMC1501? Could this hardware emulator be then installed into my defunct TI-57 to make it live again, perhaps even better than before?

See the hackaday page for more info on this project!

Tom LeMense
February 2024
