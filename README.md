# RISC-V Emulator
An in-development RISV-V Emulator which simulates a simple 5 stage Risc-V Pipeline with hazarding detection and mitigation. Note that the current implementation only supports the following instructions. Currently, the supported instruction set only
extends to a small subset of the RISC-V Basic ISA, but more will be added soon. 

## Running the provided simulation:
The provided demo details an example of a program that adds two numbers and writes the numbers to an address in memory. 

Running the simulation entails compiling the `main.cpp` program in the `samples` directory with the option `-std==c++20`. 
Next you write a program using RISC-V assembly, then call the compiled binary from main with the file containing the list. 
Parameters in the file can be altered as one wishes. 

## Planned Features

* Expanded Instruction Set
* Load hazards
* Configuration Options
* Branch Prediction Emulaton.
* Data Visualization of Virtualized CPU Performance.


