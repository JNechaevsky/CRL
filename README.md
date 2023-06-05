# CRL

[![CRL icon](https://github.com/JNechaevsky/ChocoRenderLimits/blob/master/data/doom.png)](https://github.com/JNechaevsky/ChocoRenderLimits)

CRL is a continuation of [Chocorenderlimits](https://doomwiki.org/wiki/Chocorenderlimits) source port created by [RestlessRodent](https://doomwiki.org/wiki/RestlessRodent).

Armed with new features, QoL and technical improvements, it provides the ability to assist mappers in creation of Vanilla compatible maps, as well as checking some specific scenarios via additional ingame modes. The grave/tilde **[~]** key opens the in-game menu which can be used to change the settings and features.

Major idea is still same - this is crash-prone source port, which will print ingame warnings instead of crashings. However, no changes were made to drawing code and important bugs like Tutti-Frutti and Medusa are still here, to preserve DOS executable behavior and look. Medusa is no longer critical though, and not crashing Windows executable. For vanilla-compatibility purposes, despite of being crash-prone in complex game scenes, this is not limit-removing source port and it will not be able to load huge maps.

Important bits of code has been ported from [International Doom](https://github.com/jnechaevsky/inter-doom), [Crispy Doom](http://github.com/fabiangreffrath/crispy-doom) and [DOOM Retro](https://github.com/bradharding/doomretro).

CRL is maintained by [Julia Nechaevskaya](mailto:julia.nechaevskaya@live.com).

## Download

Version 1.4 (released: June 5, 2023):
* Windows (32-bit): [crl-1.4-win32.zip](https://github.com/JNechaevsky/CRL/releases/download/crl-1.4/crl-1.4-win32.zip)
* Windows (64-bit): [crl-1.4-win64.zip](https://github.com/JNechaevsky/CRL/releases/download/crl-1.4/crl-1.4-win64.zip)

Previous versions can be found on [Releases](https://github.com/JNechaevsky/CRL/releases) page.<br>
Old versions of original Chocorenderlimits can be found on [Historical](https://github.com/JNechaevsky/CRL/releases/tag/Historical) page.

## Major features

### Game modes

* **Spectator mode.** Allows camera to move freely though the level. Use moving keys for horizontal camera movement and mouse wheel for vertical.
* **Freeze mode.** Freezes game world. Player will not be able to receive damage by touching monsters projectiles and environmental damage.
* **No target mode.** Monsters will not react to player neither after see them, not after hearing a player's sound.
* **No momentum mode.** As stated in Doom source code: "Not really a cheat, just a debug aid."

### Counters

There are two type of counters: playstate and render. Short names stands for:

#### Playstate counters

* **BRN:** Number of Icon of Sin monster spawner targets. The more vanilla limit of 32 is overflowed, the more chance of critical error increases, especially if multiple spawners are placed on map. This counter is appearing if at least one spawner target is placed on map.
* **ANI:** Number of lines with scrolling animations (line effect №48). Vanilla will crash with "Too many scrolling wall linedefs" message if map contains 65 or more such lines.
* **BTN:** Number buttons pressed in simultaneously. Vanilla will crash with "P_StartButton: no button slots left!" message if more than 16 plats will be pressed in one second.
* **PLT:** Number of active of in-stasis platforms. Vanilla will crash with "P_AddActivePlat: no more plats!" message if more than 30 plats will be active.

#### Render counters

* **SPR:** Number of rendered sprites. Due to the nature of how engine handles them, not all of them might be really visible, but still rendered.
* **SSG:** Number of rendered solid segments. Overflowing vanilla limit of 32 _may_ resulting a crash.
* **SEG:** Number of rendered wall segments. Overflowing vanilla limit of 256 will resulting a HoM effect sooner or later.
* **PLN:** Number of rendered floor and ceiling visplanes. Overflowing vanilla limit of 128 may result both HoM effect, as well as crashing.
* **OPN:** Number of rendered openings.
