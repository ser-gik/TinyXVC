
# TinyXVC - minimalistic XVC (Xilinx Virtual Cable) server

## Purpose

TinyXVC allows you to connect Xilinx FPGAs to a Vivado or ISE without using
expensive Xilinx Platform Cable.

Instead it provides a ["virtual cable"](https://github.com/Xilinx/XilinxVirtualCable/blob/master/README.md)
on top of an affordable FTDI chip that connects FPGA' JTAG TAP pins to your computer USB. This is the only
additional hardware that one needs to start! Just use any of a myriad of breakout boards or
cables that feature MPSSE-capable FTDI chip (already tested with FT232H, FT2232H):

- connect breakout board/cable to your PC USB
- connect JTAG pins (TCK, TMS, TDI, TDO) of FTDI chip to JTAG TAP of your FPGA (ensure first
        that it tolerates 3.3V IO levels from FTDI, otherwise you may need a shifter)

## How Fast Is It?

On Ubuntu 20.04.1 LTS, Intel® Core™ i7-8550U CPU @ 1.80GHz × 8 host, using 100ns TCK period:

- to program configuration memory for Spartan 6 XC6SLX9 (via iMPACT) - instant (less than 2 sec)
- to program configuration memory for Artix 7 XC77A50T (via Vivado) - ~4.5 sec
- to program QSPI flash [indirectly](https://www.xilinx.com/support/documentation/application_notes/xapp586-spi-flash.pdf) for Artix 7 XC77A50T (via Vivado) - ~125 sec

(Indeed numbers depend mostly on TCK period, dev machine CPU speed is way less important.)

## How To Use?

Once FPGA is connected to a PC via appropriate cable or dongle, start `txvc` with a proper
options. Assuming we use FT232H cable to talk to FPGA, it can be as a simple as:
```
$ txvc -p ft232h
      3341:            txvc: I: Found alias ft232h (FT232H-based USB to JTAG cable),
      3387:            txvc: I: Using profile ftdi-generic:device=ft232h,vid=0403,pid=6014,channel=A,read_latency_millis=1,d4=ignored,d5=ignored,d6=ignored,d7=ignored,
      3422:     ftdiGeneric: I: Using d2xx driver v.1.4.24
      7887:     ftdiGeneric: I: Using device "Single RS232-HS" (serial number: "")
     14185:          server: I: Listening for incoming connections at 127.0.0.1:2542...
```

When it is listening for client to connect - open Vivado' Hardware Manager and via "Open target"
connect to a virtual cable at address that is shown in `txvc` log. If using ISE - launch iMPACT
and via "Cable setup..." choose "Cable Plug-in" with the next configuration:
```
xilinx_xvc host=127.0.0.1:2542 disableversioncheck=true
```
That's it, now try to upload your designs to an FPGA!

To get more info about available options:
```
$ txvc -h
```
## Limitations

Currently `txvc` supports only MPSSE-capable FTDI chips as an intermediate between FPGA and dev
machine, though other "backends" can be added easily.
Supported and tested chips are:

- FT232H
- FT2232H

Other models should also work but were not tested.

## How To Build?

Build was tested on Ubuntu 20.04 but should work on many other distros.

Install tools and dependencies:
```
$ sudo apt install build-essential cmake libftdi1-dev
```
Once sources are checked out, build with next:
```
$ cd TinyXVC
$ mkdir build && cd $_
$ cmake ../ && make all
$ txvc/txvc -h
```

## Troubleshooting

You may need an appropriate udev rules to be able to open USB devices w/o root access. Follow
instructions from `udev/` and reload rules

## Supported Boards/Existing profiles
Boards - Profile

- Numato Lab Mimas A7 FPGA Development Board    - mimas_a7
- Numato Lab Mimas Mini FPGA Development Board  - mimas_a7_mini
- Numato Lab Narvi Sparten 7 FPGA Module        - narvi

Generic cables - Profile

- FT232H cable                                   - ft232h

