zaphodkvm - software KVM switch for multiple X servers
Copyright (C) 2014 by geekamole, released under the GNU GPLv3 or later (see COPYING)

libsuinput used and distributed under terms of the GNU GPLv3 or later, and obtained from
   http://tjjr.fi/sw/libsuinput/

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

----------------------------------------------------------------------------------------

To install: simply

make

then copy the binary "zaphodkvm" to somewhere in your path. It should be run as root. I run it
early in the boot process which seems to resolve some race conditions with X and the VTs.
It runs in the foreground but it should be put in the background and disowned so that it runs
as a daemon while the system is running.

Since this program uses the uinput kernel module, be sure to enable it as built-in or module.
This program also uses the system command "fgconsole," which on Gentoo is in the sys-apps/kbd package.

zaphodkvm implements a software-based KVM switch (really just the KM part) for a multiseat-X
Linux configuration. With only one user controlling all the physical input devices, it might
be more appropriately called a single-seat-multiple-X-server configuration.

zaphodkvm listens to all connected USB mice and keyboards, and supports hotplugging them via
libudev. It generates four virtual input devices, two "mice" and two "keyboards," and redirects
all input events from the real hardware to one or the other of the virtual mice/keyboard sets.
Double-tapping scroll lock switches the active virtual mice/keyboard set. If your mice and
keyboards aren't USB devices, you will need to change the udev filters in zaphodkvm.cpp. The
filters are there to keep the uinput devices from being detected as physical input devices.

When uinput devices are created, they show up as a node with the form /dev/input/eventX. Unlike
a physical HID device, udev does not automatically create a persistent symlink in /dev/input/by-id/.
I had to add some rules to my /etc/udev/rules.d/00-custom-persistent.rules file:

ATTRS{name}=="zaphodkvm_kbd0", DEVPATH=="*event*", SYMLINK+="input/by-id/zaphodkvm_kbd0"
ATTRS{name}=="zaphodkvm_kbd1", DEVPATH=="*event*", SYMLINK+="input/by-id/zaphodkvm_kbd1"
ATTRS{name}=="zaphodkvm_mouse0", DEVPATH=="*event*", SYMLINK+="input/by-id/zaphodkvm_mouse0"
ATTRS{name}=="zaphodkvm_mouse1", DEVPATH=="*event*", SYMLINK+="input/by-id/zaphodkvm_mouse1"

Then either reboot or do a "udevadm control --reload-rules" to activate the new rules. Once the 
persistent symlinks are present, configure X servers to use the symlinks as their only input devices.

I use simple, standard mice with the normal left/right buttons and clickwheel. I also use standard
keyboards. Fancy mice or keyboards might require you to add additional calls to suinput_enable_event()
in zaphodkvm.cpp, to turn on the additional buttons. I also discard all the raw scancodes from the
physical /dev/input/eventX devices, but those might be useful to your application.

Since the virtual terminals 1-6 tend to latch onto the uinput device nodes when they are created,
I initially had issues with duplicate keystrokes when the X servers weren't running or when I switched
VTs with Ctrl-Alt-FX. Now, zaphodkvm checks to see if VT7 is the active virtual terminal, and only
passes on input events when it is. The X server on VT8 uses -sharevts and DontVTSwitch so it doesn't
affect fgconsole/VT detection.

I set this up for a two-X-server system, but it could be easily extended to an arbitrary number of X servers.
