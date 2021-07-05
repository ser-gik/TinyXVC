## Udev rules collection

Registry of rules for USB devices supported by `txvc`.
If an attempt to open connected USB device fails:

- unplug device
- copy `.rules` file to `/etc/udev/rules.d/`
- run `$ sudo udevadm control --reload `
- plug device

