## CRL

[![CRL icon](https://github.com/JNechaevsky/ChocoRenderLimits/blob/master/data/doom.png)](https://github.com/JNechaevsky/ChocoRenderLimits)

CRL is a continuation of [Chocorenderlimits](https://doomwiki.org/wiki/Chocorenderlimits) source port, created by [RestlessRodent](https://doomwiki.org/wiki/RestlessRodent). 

Armed with new features, QoL and technical improvements, it provides the ability to assist mappers in creation of Vanilla comaptible maps, as well as checking some specific scenarios via additional ingame modes. The _tilde_ [<kbd>~</kbd>] key opens the in-game menu which can be used to change the settings and features.

CRL is maintained by [Julia Nechaevskaya](mailto:julia.nechaevskaya@live.com).

### Download:

CRL is currently under development.

Old version of Chocorenderlimits ported to SDL2 can be found [here](https://github.com/JNechaevsky/ChocoRenderLimits/releases/tag/1.0).

### Counters

Limit counters are representing values of engine rendering limits. Port will not crash in case of overflow will happen. Instead, counter will start to blink. Short names stands for limit:

* **BRN:** Number of Icon of Sin monster spawned targets. 
* **ANI:** Number of lines with scrolling animations (line effect №48).
* **PLT:** Number of active of in-stasis platforms.
* **SPR:** Number of rendered sprites, but due to the nature of how engine handles them, not all of them might be really visible.
* **SEG:** Number of rendered wall segments.
* **PLN:** Number of rendered floor and ceiling visplanes.
* **OPN:** "Openings". Erm... 🤔
