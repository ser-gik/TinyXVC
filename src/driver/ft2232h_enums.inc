
#ifndef PIN_ROLE
#define PIN_ROLE(name, enumVal, descr)
#endif
PIN_ROLE("tck", PIN_ROLE_JTAG_TCK, "JTAG TCK signal (clock)")
PIN_ROLE("tdi", PIN_ROLE_JTAG_TDI, "JTAG TDI signal (test device input)")
PIN_ROLE("tdo", PIN_ROLE_JTAG_TDO, "JTAG TDO signal (test device output)")
PIN_ROLE("tms", PIN_ROLE_JTAG_TMS, "JTAG TMS signal (test mode select)")
PIN_ROLE("driver_low", PIN_ROLE_OTHER_DRIVER_LOW, "permanent low level driver")
PIN_ROLE("driver_high", PIN_ROLE_OTHER_DRIVER_HIGH, "permanent high level driver")
PIN_ROLE("ignored", PIN_ROLE_OTHER_IGNORED, "ignored pin, configured as input")
#undef PIN_ROLE

#ifndef CLK_EDGE
#define CLK_EDGE(name, enumVal, descr)
#endif
CLK_EDGE("falling", CLK_EDGE_FALLING, "falling/negative clock transition")
CLK_EDGE("rising", CLK_EDGE_RISING, "rising/positive clock transition")
#undef CLK_EDGE

#ifndef PIN_LEVEL
#define PIN_LEVEL(name, enumVal, descr)
#endif
PIN_LEVEL("low", PIN_LEVEL_LOW, "low/zero signal level")
PIN_LEVEL("high", PIN_LEVEL_HIGH, "high/one signal level")
#undef PIN_LEVEL

#ifndef FTDI_INTERFACE
#define FTDI_INTERFACE(name, enumVal, descr)
#endif
FTDI_INTERFACE("A", INTERFACE_A, "FTDI' ADBUS channel")
FTDI_INTERFACE("B", INTERFACE_B, "FTDI' BDBUS channel")
#undef FTDI_INTERFACE

