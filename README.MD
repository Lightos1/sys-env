## Overview

Sys-env can be used to prevent mods (or similar things) from being loaded in SysNAND / EmuNAND (this is configurable). <br>

## Usecases:
The most notable example is themes: <br>
 - Theme patches usually need to match the exact firmware version
   - If SysNand and EmuNand are on different firmware, you end up with a crash
 themes need to match the firmware update, so you will get a crash if you use sys and emunand on different firmware
 - If you only need themes on just one of the two environments, you can simply block them from loading on the other one

What this basically does: during boot, it appends .bak to the end of matching directories so they won’t be loaded. <br>

## Usage:
There is no overlay due to my hatred of writing ui, so you have to configure it via the config. <br>
This can be done very easily though: <br>
Edit ``/config/sys-env/config.ini`` and set it up in the following way: <br>
```
[SysNand]
[program id(s) to be blocked in SysNand]

[EmuNand]
[program id(s) to be blocked in EmuNand]
```

You have two sections: <br>

[SysNand] <br>
[EmuNand] <br>

Order doesn’t matter. <br>
Just add the program ID(s) under whichever environment you want them blocked in. <br>

## Example

For instance this config will block themes on ``SysNand``. <br>
```
[SysNand]
01000000000000001000

[EmuNand]
```

## Thanks to:
 - Arcdelta for motivating me to actually finish this lmao.
