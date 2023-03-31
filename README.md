# CRL

[![CRL icon](https://github.com/JNechaevsky/ChocoRenderLimits/blob/master/data/doom.png)](https://github.com/JNechaevsky/ChocoRenderLimits)

CRL is a continuation of [Chocorenderlimits](https://doomwiki.org/wiki/Chocorenderlimits) source port, created by [RestlessRodent](https://doomwiki.org/wiki/RestlessRodent). 

Armed with new features, QoL and technical improvements, it provides the ability to assist mappers in creation of Vanilla compatible maps, as well as checking some specific scenarios via additional ingame modes. The _tilde_ [<kbd>~</kbd>] key opens the in-game menu which can be used to change the settings and features.

CRL is maintained by [Julia Nechaevskaya](mailto:julia.nechaevskaya@live.com).

## Download

* Windows (32-bit): [crl-1.0-win32.zip](https://github.com/JNechaevsky/CRL/releases/download/crl-1.0/crl-1.0-win32.zip)
* Windows (64-bit): [crl-1.0-win64.zip](https://github.com/JNechaevsky/CRL/releases/download/crl-1.0/crl-1.0-win64.zip)

Previous versions can be found on [Releases](https://github.com/JNechaevsky/CRL/releases) page.

## Major features

CRL keeps original Chocorenderlimits idea of being crash-prone source port, and instead of crashing on most of cases, it will print ingame/console warnings.

### Game modes

* **Spectating.** Allows camera to move freely though the level. Use moving keys for horizontal camera movement and mouse wheel for vertical.
* **Freeze.** Freezes game world. Player will not be able to receive damage by touching monsters projectiles and environmental dagame.
* **Notarget.** Monsters will not react to player neither after see them, not after hearing a player's sound.

### Counters

There are two type of counters: playstate and render. First one represeng map-specific limits, while second one representing rendering/drawing limits. Port will not crash in case of overflow will happen. Instead, counter will start to blink. Short names stands for:

#### Playstate counters

* **BRN:** Number of Icon of Sin monster spawner targets. The more vanilla limit of `32` is overflowed, the more chance of critical error increases, especially if multiple spawners are placed on map. This counter is appearing if at least one spawner target is placed on map.
* **ANI:** Number of lines with scrolling animations (line effect №48). Vanilla will crash with "Too many scrolling wall linedefs" message if map contains `65` or more such lines.
* **BTN:** Number buttons pressed in simultaneously. Vanilla will crash with "P_StartButton: no button slots left!" message if more than `16` plats will be pressed in one second.
* **PLT:** Number of active of in-stasis platforms. Vanilla will crash with "P_AddActivePlat: no more plats!" message if more than `30` plats will be active.

#### Render counters

* **SPR:** Number of rendered sprites, but due to the nature of how engine handles them, not all of them might be really visible.
* **SEG:** Number of rendered wall segments. Overflowing vanilla limit of `256` will resulting a HoM effect.
* **PLN:** Number of rendered floor and ceiling visplanes. Overflowing vanilla limit of `128` may result both HoM effect, as well as crashing.
* **OPN:** "Openings". Erm... 🤔
