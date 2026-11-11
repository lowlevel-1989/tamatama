## Notes

Thanks to BANDAI and WiZ, the creators of Tamagotchi, and to BANDAI NAMCO,
who made these games for Android. Tamatama only runs the games: you need the
original `.apk` files of the versions you own.

Copy them into `ports/tamatama/apk/` and start the port: a menu lets you
choose the version. It recognizes Tamagotchi Classic v1.0.2, Tamagotchi
Classic 2 v1.0, Tamagotchi L.i.f.e. v1.0, v1.01, v1.02, v2.00, v2.0.1 and
v2.0.2, and Tamagotchi Angel v1.00.

Tamatama is open source (MIT): https://github.com/lowlevel-1989/tamatama

## Controls

| Button | Action |
|--|--|
| D-Pad | Move the pointer between the buttons on screen (on paged screens, left and right turn the page) |
| A | Press the button under the pointer; with no pointer on the device screen, its C button |
| Y / B / A | The A, B and C buttons of the Tamagotchi (X is also C) |
| Start | Press the button under the pointer |
| Select | Quit the game and go back to the menu (press it twice) |

In the menu any face button or Start chooses, so it works even when the
buttons of the device come swapped. "Configure controls" in the menu sets
them for the game: it saves `controller.map`, which wins over PortMaster's
mapping. Exit in the menu closes the port.

"Transfer Save" in the menu passes the save of a version to another device
on the same network with Tamatama (a PC, a handheld or a phone): "Receive
Save" on one, "Send Save" on the other. Both show the same 4-character
signature to check. The save that was there is kept as a backup ("Restore
Backup").

## Compile

```
git clone https://github.com/lowlevel-1989/tamatama.git
cd tamatama
make PLATFORM=portmaster
```

It builds `tamatama.zip` for aarch64 and armhf with clang, against Debian
buster (glibc 2.28), and downloads what it needs the first time.
