# Things to add

## Other things/features

* Split into more c files, honestly a mess to navigate in 1 big file when we could have separation of monitors and windows and commands
* Allow moving fullscreen windows with mouse

## Bugs

* Unknown bug where firefox and other windows suddenly resize as if they are alone on a monitor
* Windows that open as floating should be moved to the middle of parent window instead of 0,0 on same monitor
* Click on floating window should bring it to foreground (probably something to do with restack function)
* Right clicking on barwin fish/tile/monocle item should cycle -1, and left click should cycle +1 through all modes
* Fade SchemeSel color for how many devices are focused instead of this manual thing
* Change border color based on index in the stack
* Add tail pointer to stack and other lists, could save a lot of resources on just going to tail instead of iterating through everything first
* Add toggle to hide floating and fullscreen windows
