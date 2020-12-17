
#ifndef FT_PARAM
#define FT_PARAM(name, configField, converterFunc, validation, descr)
#endif
FT_PARAM("vid", vid, str_to_usb_id, > 0, "USB device vendor ID")
FT_PARAM("pid", pid, str_to_usb_id, > 0, "USB device product ID")
FT_PARAM("channel", channel, str_to_ftdi_interface, >= 0, "FTDI channel to use")
FT_PARAM("tck_idle", tck_idle_level, str_to_pin_level, != PIN_LEVEL_INVALID,
    "Level of the TCK signal between transactions")
FT_PARAM("tdi_change_at", tdi_tms_changing_edge, str_to_clk_edge, != CLK_EDGE_INVALID,
    "TCK edge when TDI/TMS values are updated")
FT_PARAM("tdo_sample_at", tdo_sampling_edge, str_to_clk_edge, != CLK_EDGE_INVALID,
    "TCK edge when TDO value is sampled")
FT_PARAM("d0", d_pins[0], str_to_pin_role, != PIN_ROLE_INVALID, "D0 pin role")
FT_PARAM("d1", d_pins[1], str_to_pin_role, != PIN_ROLE_INVALID, "D1 pin role")
FT_PARAM("d2", d_pins[2], str_to_pin_role, != PIN_ROLE_INVALID, "D2 pin role")
FT_PARAM("d3", d_pins[3], str_to_pin_role, != PIN_ROLE_INVALID, "D3 pin role")
FT_PARAM("d4", d_pins[4], str_to_pin_role, != PIN_ROLE_INVALID, "D4 pin role")
FT_PARAM("d5", d_pins[5], str_to_pin_role, != PIN_ROLE_INVALID, "D5 pin role")
FT_PARAM("d6", d_pins[6], str_to_pin_role, != PIN_ROLE_INVALID, "D6 pin role")
FT_PARAM("d7", d_pins[7], str_to_pin_role, != PIN_ROLE_INVALID, "D7 pin role")
#undef FT_PARAM

