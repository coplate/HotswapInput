# HotswapInput
Hot swappable device driver for linux sytsems using evdev input - Focused on Kindle paperwhite 2015



tkbd is a kernel module that will listen for all connected and disconnected keyboards, and clone all of thier events.
In essence, this will allw you to have one keyboard that is always present, wether you plug or unplug other keyboards.

tmouse is a kernel module that will listen for all connected and disconnected mice, and clone all of thier events.
In essence, this will allw you to have one mouse that is always present, wether you plug or unplug other mice. ( and unlike /dev/mice retains the event format of /dev/input/x )

makexconfig is a replacement of the makexconfig from kindle paperwhite 2015 ca. 5.8.0 that recognizes these hot swappable drivers.
This works in conjunction with pointer.c to let you control the actual screen with a mouse pointer.

pointer.c is a program that will continously draw and refresh an area of the e-ink screen around the current mouse pointer.  Allowing you to control a kindle with a broken touch screen.

I cannot find the makefile I used for this, but it shoudl be a pretty standard kindle makefile with X11, see the makefile for kindlelazy
