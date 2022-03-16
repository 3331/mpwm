# mpwm - Multi Pointer Window Manager

mpwm is based on [dwm](https://dwm.suckless.org/) and a
fast, small, and dynamic window manager which uses xinput2
to enable the use of multi pointer (MPX) for X.  

Currently there is no bindings for creating/managing device pairs.  

Use the tool `xinput` to manage devices for multi pointer/keyboard adventures for now.  

## Features/patches

* **TODO** Split into more c files, it is currently a mess to navigate the code

* Intuitive (MODKEY + TAB) behaviour
  * Cycle through stack and watch windows move around (MODKEY + TAB)
  * **TODO** Combined with (ControlMask) it will cycle through tags instead of clients
  * Combining client/tag cycling bindings with (ShiftMask) will change cycle direction to back
* Proper multi cursor support with XInput2
  * Multiple cursor/keyboard pairs can have focus on 1 window
  * **TODO** Limit only 1 pointer/keyboard pair to have focus on an application at a time
  * **TODO** Cycle already existing devices to other masters
  * **TODO** Create new master device pair
  * **TODO** Remove master device pair
* Improved fullscreen support with multi monitors
  * Swap entire monitor with next/prev monitor (MODKEY|ShiftMask|ControlMask + .) or (MODKEY|ShiftMask|ControlMask + ,)
  * Move fullscreen applications to other screens with keyboard (MODKEY + .) or (MODKEY + ,)
  * **TODO** Move fullscreen applications to other screens with mouse (MODKEY|ShiftMask + Button1)
  * **TODO** Toggle visibility of fullscreen and floating windows (MODKEY|ShiftMask + f)
  * **TODO** Toggle fullscreen on window (MODKEY|ShiftMask|ControlMask + f)
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
