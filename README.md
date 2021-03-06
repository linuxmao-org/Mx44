**Anté-scriptum:** this is a recovery from the original code from http://web.comhem.se/luna/ which isn't available anymore at the time being - 6th April 2021 (see [an archive of it](https://web.archive.org/web/20190205234526/http://web.comhem.se/luna/)).
Kudos to Jens M Andreasen  (its original author), it's been released under a GPLv2+ licence, hence it's available for the community to maintain/develop/fork it under the same terms.




Intro
-----

Mx44 is a multichannel polyphonic synthesizer, loosely based on FM-synthesis, with a Klingon approach to oscillators...



Building
--------

```
cd src
make
sudo make install
```

should do the trick. The default is to install the binary in `/usr/local/bin/` and to install the default patch file in `/usr/local/share/Mx44`.



Commandline options
-------------------

To see all available command line options use the `--help` option.
Options are described below.



State / Patches
---------------
Mx44 stores its state and patches in a single file automatically.

Mx44 will attempt to read patches from a file called `.mx44patch` in your home directory. If this fails then it will load the system patches in `/usr/local/share/Mx44/mx44patch`.

Mx44 will automatically write patches to .mx44patch in your home directory when it exits.

There are two new command line options to specify the patch to load from and the patch to save to. If you specify a patch to load from, you must also specify the patch to save to. This is to avoid unintentionally overwriting `~/.mx44patch`.

Although you must specify `--save-to` when specifying `--load-from`, you can specify `--save-to` without specifying `--load-from`.



Voices
------

Previously you could specify the number of voices for Mx44 to use by simply adding a number after the command to run mx44. You must now use the `--voices=` option.



Console Operation
-----------------

Mx44 will run quite happily without a graphical user interface. Just supply the `--console` option on the commandline. Obviously you won't be able to edit the patches in this mode. Mx44 will also run in this mode if the `DISPLAY` environment variable is not defined.



Auto-connecting outputs
-----------------------

Mx44 no longer automatically connects it's outputs to the system playback ports. To revert to this behaviour simply add `--auto-connect` as a command line option and it will.



Implemented GS controllers
--------------------------

```
R # Ctrl

 73 Attack  (modifies the time value of all env stage 1 and 2)
 75 Decay   (modifies the time value of all env stage 3 and 4)
 79 Loop    (modifies the time value of all env stage 5 and 6)
 72 Release (you get the drill ...)

 05 Portamento (routed to intonation, being the closest match)
 94 Celeste (modifies amount of frequency offset)

*07 Channel Volume (yep!)
 10 Pan            (rotates the sound-image)

*01 Modulation (modulation send amount from op with "Wheel" btn ON)

*70 Timbre     (modulation send amount from op with "Wheel" btn OFF)
*71 Variation  (balance between modulation from op 1+3/op 2+4)

*74 Cutoff Freq (resonance ctrl for oscillators connected to envelope)
```

Controllers marked with asterix operates in true RT mode (ie: on a sustained note.) The rest is set up at note-on.



External docs & démos
---------------------

- you can see a [demonstration of this synthesizer made by Brian on linuxsynths.com](http://linuxsynths.com/Mx44PatchesDemos/mx44.html)
- [linuxmao Mx44 page](http://linuxmao.org/mx44) 🇫🇷
- [linuxaudio Mx44 page](https://wiki.linuxaudio.org/apps/all/mx44)
- [archived old upstream page](https://web.archive.org/web/20190205234526/http://web.comhem.se/luna/)
