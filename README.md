
# TinyXVC - minimalistic XVC (Xilinx Virtual Cable) server

## Introduction

This program is an implementation of [XVC](https://github.com/Xilinx/XilinxVirtualCable/blob/master/README.md)
protocol, that allows user to connect their custom hardware to the development tools via "virtual cable".

Currently `txvc` supports only one type of backend - FT2232H chip, that acts as intermediate between
PC and JTAG TAP. Users are encouraged to add their own backend implementations.

## How to use

Currently `txvc` supports only Linux hosts. To install needed tools and dependencies run:
```
$ sudo apt install build-essential
$ sudo apt install cmake
$ sudo apt install libftdi1-dev
```
Once sources are checked out it can be built with:
```
$ cd TinyXVC
$ mkdir build && cd $_
$ cmake ../
$ make all
```
To run `txvc` you need to provide a "profile" to initialize backend appropriately for interacting
with your hardware. E.g.:
```
$ ./txvc -p mimas_a7
```
To get info about profiles and other options:
```
$ ./txvc -h
```

