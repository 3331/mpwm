# Things to add

## Other things/features

* Split into more c files, honestly a mess to navigate in 1 big file when we could have separation of something sensible
* Limit only 1 pointer/keyboard pair to have focus on an application at a time
* Toggle visibility of fullscreen and floating windows
* Allow moving fullscreen windows with mouse
* Dynamic SchemeSel color for how many devices are focused instead of this manual thing

## Bugs

* Battle.net and their games steal focus sometimes and its very annoying, is this something wrong with the wm?
* Unknown bug where firefox and other windows suddenly resize as if they are alone on a monitor
* Click on floating window should bring it to foreground (probably something to do with restack function)
* Right clicking on barwin fish/tile/monocle item should cycle -1, and left click should cycle +1 through all modes

## Maybe fixed

* Empty screen disables MODKEY + ,/. (change focus to different monitor) (bug that needs to be triggered maybe?)
* Windows that open as floating should be moved to the middle of parent window instead of 0,0 on same monitor
