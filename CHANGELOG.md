## CRL (XXXX-XX-XX)

#### Gameplay
* Fixed missing messages and level names in automap.
* Run/walk mode now can be toggled by pressing run button.

#### CRL menu & featers

Revamped in-game menu and added new features:
 * **Detectors**
   * Intercepts overflow. Once limit is reached (128 vanilla + 61 for overflow emulation), "All Ghost effect" will happen. This detector will help to see when it's happened.
   * Medusa. Will blink if level contains Medusa bug.
 * **Counters**
   * Wall segs - represents wall segments (top/middle/bottom or solid) ammount. Once value reaches `256` (a `SOLIDSEGS` static limit), counter will start blinking.
  * **Widgets**
   * K/I/S stats.
   * Level time. 
   * Player coords.

#### Technical impovements
* Updated build system CMake.
* Updated SDL2 libraries to latest versions (SDL 2.26.2 and SDL Mixer 2.6.2).
* Replaced libpng library with zlib (thanks @Dasperal).
* Windows OS only: added `-console` command line parameter, which enables console output window.
* Both `Enter` keys now working for fullscreen toggling.
