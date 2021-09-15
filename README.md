# mpwm - Multi Pointer Window Manager

Based on [https://dwm.suckless.org/](dwm)

mpwm is an extremely fast, small, and dynamic window manager which uses
xinput2 to enable the use of multi pointer (MPX) for X.

## Requirements

In order to build mpwm you need the Xlib header files.

## Installation

Edit config.mk to match your local setup (mpwm is installed into
the /usr/local namespace by default).

Afterwards enter the following command to build and install mpwm (if
necessary as root):

```text
make clean install
```

## Running mpwm

Add the following line to your .xinitrc to start mpwm using startx:

```text
exec mpwm
```

In order to connect mpwm to a specific display, make sure that
the DISPLAY environment variable is set correctly, e.g.:

```text
DISPLAY=foo.bar:1 exec mpwm
```

(This will start mpwm on display :1 of the host foo.bar.)

In order to display status info in the bar, you can do something
like this in your .xinitrc:

```text
while xsetroot -name "`date` `uptime | sed 's/.*,//'`"
do
    sleep 1
done &
exec mpwm
```

## Configuration

The configuration of mpwm is done by creating a custom config.h
and (re)compiling the source code.
