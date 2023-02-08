## CRL

[![CRL icon](https://github.com/JNechaevsky/ChocoRenderLimits/blob/master/data/doom.png)](https://github.com/JNechaevsky/ChocoRenderLimits)

CRL is a continuation of [Chocorenderlimits](https://doomwiki.org/wiki/Chocorenderlimits) source port, created by [RestlessRodent](https://doomwiki.org/wiki/RestlessRodent). 

Armed with new features, QoL and technical improvements, it provides the ability to assist mappers in creation of Vanilla compatible maps, as well as checking some specific scenarios via additional ingame modes. The _tilde_ [<kbd>~</kbd>] key opens the in-game menu which can be used to change the settings and features.

CRL is maintained by [Julia Nechaevskaya](mailto:julia.nechaevskaya@live.com).

### Download:

CRL is currently under development.

Old version of Chocorenderlimits ported to SDL2 can be found [here](https://github.com/JNechaevsky/ChocoRenderLimits/releases/tag/1.0).

### Counters

Limit counters are representing values of engine rendering limits. Port will not crash in case of overflow will happen. Instead, counter will start to blink. Short names stands for:

* **BRN:** Number of Icon of Sin monster spawner targets. The more vanilla limit of `32` is overflowed, the more chance of critical error increases, especially if multiple spawners are placed on map. This counter is appearing if at least one spawner target is placed on map.
* **ANI:** Number of lines with scrolling animations (line effect №48). Vanilla will crash with "Too many scrolling wall linedefs" message if map contains `65` or more such lines.
* **PLT:** Number of active of in-stasis platforms. Vanilla will crash with ""P_AddActivePlat: no more plats!" message once more than `30` plats will be active.
* **SPR:** Number of rendered sprites, but due to the nature of how engine handles them, not all of them might be really visible.
* **SEG:** Number of rendered wall segments. Overflowing vanilla limit of `256` _may_ resulting a HoM effect.
* **PLN:** Number of rendered floor and ceiling visplanes. Overflowing vanilla limit of `128` may result both HoM effect, as well as crashing.
* **OPN:** "Openings". Erm... 🤔

### Game modes

* **Spectating.** Allows camera to move freely though the level. Use moving keys for horizontal camera movement and mouse wheel for vertical.
* **Freeze.** Freezes game world. Player will not be able to receive damage by touching monsters projectiles and environmental dagame.
