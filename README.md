# Fujitsu Touchscreen Driver

For Fujitsu notebooks with serial (not USB!) touchscreens e.g. Lifebook P1610.

## Installation

These instructions illustrate setting up the driver on Ubuntu 15.10. The process
should be similar on other distributions, though packages and package managers
might differ.

Install packages for compiling:

    $ apt-get install build-essential pkg-config xserver-xorg-dev

Download, compile, install:

    $ git clone https://github.com/okias/xf86-input-fujitouch.git
    $ cd xf86-input-fujitouch
    $ ./configure --prefix=/usr
    $ make && make install

The driver should now be available in `/usr/lib/xorg/modules/input`.

## Usage

Now we want to get X11 to use the driver.

Search for the touchscreen's serial port:

    $ cat /var/log/Xorg.0.log | grep ttyS
    [    35.055] (II) config/udev: Adding input device Serial Wacom Tablet FUJ02e6 (/dev/ttyS4)
    [    35.060] (**) Option "Device" "/dev/ttyS4"

Hide/remove the default X11 Wacom driver (which does not work anyway). You can
do so by renaming the module:

    $ cd /usr/lib/xorg/modules/input
    $ mv wacom_drv.so disabled-wacom_drv.so

Add the [config file from Issue #4][issue-4] to X11 to set up the new driver.
Remember to change the serial port:

    $ mkdir /etc/X11/xorg.conf.d
    $ vim /etc/X11/xorg.conf.d/ts.conf

    // Content of /etc/X11/xorg.conf.d/ts.conf:
    Section "InputDevice"
    Identifier "touchscreen"
    Driver "fujitsu"
    Option "Device" "/dev/ttyS4"
    Option "DeviceName" "touchscreen"
    Option "MinX" "82"
    Option "MinY" "146"
    Option "MaxX" "4036"
    Option "MaxY" "3999"
    Option "SendCoreEvents" "On"
    EndSection

    Section "InputDevice"
    Identifier "dummy"
    Driver "void"
    Option "Device" "/dev/input/mice"
    EndSection

    Section "ServerLayout"
    Identifier "Default Layout"
    InputDevice "touchscreen" "CorePointer"
    InputDevice "dummy"
    EndSection

Reboot and see if the touchscreen responds.

If it does not, debug with information from `/var/log/Xorg.0.log`.

[issue-4]: https://github.com/okias/xf86-input-fujitouch/issues/4
