# mpwm - Multi Pointer Window Manager

mpwm is based on [dwm](https://dwm.suckless.org/) and a
fast, small, and dynamic window manager which uses xinput2
to enable the use of multi pointer (MPX) for X.  

## Features/patches

* Intuitive (MODKEY + TAB) behaviour
  * Cycle through stack and watch windows move around
  * Combined with (ControlMask) it will cycle through tags instead of clients **TODO**
  * Combined either tag/client cycling with (ShiftMask) and it will cycle backwards
* Proper multi cursor support with XInput2
  * Multiple cursor/keyboard pairs can have focus on 1 window
  * Limit only 1 pointer/keyboard pair to have focus on an application at a time **TODO**
  * Create new master device pair **TODO**
  * Cycle already existing devices to other masters **TODO**
  * Remove master device pair **TODO**
* Improved fullscreen support with multi monitors
  * Swap entire monitor with next/prev monitor (MODKEY|ShiftMask|ControlMask + .) or (MODKEY|ShiftMask|ControlMask + ,)
  * Move fullscreen applications to other screens with keyboard (MODKEY + .) or (MODKEY + ,)
  * Move fullscreen applications to other screens with mouse (MODKEY|ShiftMask + Button1) **TODO**
  * Toggle visibility of fullscreen and floating windows (MODKEY|ShiftMask + f) **TODO**
* [centeredmaster](https://dwm.suckless.org/patches/centeredmaster/) (MODKEY + o)
* [rmaster](https://dwm.suckless.org/patches/rmaster/) (MODKEY + r)

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

In order to display status info in the bar, and enable other fancy stuff
such as hyperlinks and opacity, you can do something like this in your .xinitrc:

```text
picom &
eval "$(hsetroot -solid '#000000')"
eval "$(dbus-launch --sh-syntax --exit-with-session)"
while xsetroot -name "`date` `uptime | sed 's/.*,//'`"
do
    sleep 1
done &
exec mpwm
```

## Configuration

The configuration of mpwm is done by creating a custom config.h
and (re)compiling the source code.
