# nokd

This is a simple kd/WinDBG stub which can be implemented by anyone, in virtually any environment. 
I wrote this project a few years ago, and figured it would be nice to release to the public, I personally use it 
inside of my type 1 hypervisor to debug windows guests without flagging any anti-debug software, such as malware, or anti-cheats. 
The implementation does not require any of the KD specific variables in ntoskrnl to be set, it will copy the KdDebuggerDataBlock to local
memory and decoded it inline, then pass this to WinDBG as the debugger block, making it very difficult to flag. A few functions are required to
be implemented by yourself, but an example has been provided which was taken from my hypervisor. Not all of the functions need to be implemented
but some functionality will not work properly without them.

# Communication

I've provided a very basic UART driver, which can be used with either a motherboard COM header, or adapted to work with 
various PCI-e boards depending on their chipset. You may have to modify this for it to work on your hardware.

There is also a VMWare RPC driver, which was stripped & taken from [VirtualKD](https://github.com/sysprogs/VirtualKD/), this should
work fine on vmware, with VirtualKD-Redux version 2020.2.

# Implementation

I would recommend compiling as a static library, and including in your project. You will need to implement the 
8 functions, and 2 data variables required by the library, these can be found in [kddef.h](https://github.com/irql/nokd/blob/master/kdpl/inc/kddef.h#L56-L57)
labelled with `KDPLAPI`, an example can be found in the `/examples/` folder.

# Help

Feel free to contact me on discord if you need help implementing it: `ivql`
