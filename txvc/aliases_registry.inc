
/* Generic */
PROFILE_ALIAS("ft232h", "FT232H-based USB to JTAG cable",
                        "ftdi-generic:"
                        "device=ft232h,vid=0403,pid=6014,channel=A,read_latency_millis=1,"
                        "d4=ignored,d5=ignored,d6=ignored,d7=ignored,")

/* Numato Lab */
PROFILE_ALIAS("mimas_a7", "Numato Lab Mimas Artix 7 FPGA Module",
                        "ftdi-generic:"
                        "device=ft2232h,vid=2a19,pid=1009,channel=B,read_latency_millis=1,"
                        "d4=ignored,d5=ignored,d6=driver_low,d7=driver_low,")
PROFILE_ALIAS("mimas_a7_mini", "Numato Lab Mimas A7 Mini FPGA Development Board",
                        "ftdi-generic:"
                        "device=ft2232h,vid=2a19,pid=100E,channel=B,read_latency_millis=1,"
                        "d4=ignored,d5=ignored,d6=driver_low,d7=driver_low,")
PROFILE_ALIAS("narvi", "Numato Lab Narvi Spartan 7 FPGA Module",
                        "ftdi-generic:"
                        "device=ft2232h,vid=2a19,pid=100D,channel=B,read_latency_millis=1,"
                        "d4=ignored,d5=ignored,d6=driver_low,d7=driver_low,")

