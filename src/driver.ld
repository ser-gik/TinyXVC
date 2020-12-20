SECTIONS
{
    .txvc_driver :
    {
        /* Collect all driver instances into array and provide symbols to iterate over items */
        PROVIDE_HIDDEN (__txvc_drivers_begin = .);
        KEEP (*(.txvc_driver))
        PROVIDE_HIDDEN (__txvc_drivers_end = .);
    }
}
INSERT AFTER .rodata;
