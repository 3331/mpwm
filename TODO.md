# Things to add

## New bugs

* [ ] Unknown bug where changing tag causes main monitor to be duplicated to other monitor
  * Could be related to fullscreen application on other tag that closed without being visible

* [ ] Disable `sendmon` (ctrl + . ,) function when client is fullscreen or make it possible to change monitor with fullscreen applications

## Old bugs

* [ ] Unknown bug where firefox and other windows suddenly resize as if they are alone on a monitor

* [ ] Debug window, show info about:
  * Devices
    * Masters
      * Mices
      * Keyboards
  * Monitors
    * Bar
    * Windows/client

* [ ] Windows that open as floating should be moved to the middle of parent window instead of 0,0 on same monitor
* [ ] Click on floating window should bring it to foreground (probably something to do with restack function)
* [ ] Right clicking on barwin fish/tile/monocle item should cycle -1, and left click should cycle +1 through all modes
* [ ] Split into more c files, honestly a mess to navigate in 1 big file.
* [ ] Fade SchemeSel color for how many devices are focused instead of this manual thing
* [ ] Change border color based on index in the stack
* [ ] Add tail pointer to stack and other lists, could save a lot of resources on just going to tail instead of iterating through everything first
* [ ] Floating windows should go to background if non floating window is in focus, then pop back when non floating window is unfocused (also add unfocus keybinding so you can get back to normal without closing windows)
