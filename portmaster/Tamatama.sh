#!/bin/bash

XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}

if [ -d "/opt/system/Tools/PortMaster/" ]; then
  controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
  controlfolder="/opt/tools/PortMaster"
elif [ -d "$XDG_DATA_HOME/PortMaster/" ]; then
  controlfolder="$XDG_DATA_HOME/PortMaster"
else
  controlfolder="/roms/ports/PortMaster"
fi

source $controlfolder/control.txt

GAMEDIR=/$directory/ports/tamatama

# Tamatama plays the Android Tamagotchi games whose .apk files are in apk/.
#   tamatama.armhf    everything in one program: the game's .so (32-bit ARM)
#                     runs inside it. The fastest, so it is used whenever the
#                     device can run it, aarch64 ones with 32-bit libraries
#                     included (it answers --probe with 42).
#   tamatama.aarch64  otherwise: the screen and the natives, with the game's
#                     .so in loader-arm, on the CPU if it still runs 32-bit
#                     ARM, or else under qemu-arm.aarch64.
$ESUDO chmod +x "$GAMEDIR/tamatama.armhf"
"$GAMEDIR/tamatama.armhf" --probe > /dev/null 2>&1
if [ $? -eq 42 ]; then
  ARCH="armhf"
  export PORT_32BIT="Y"
else
  ARCH="aarch64"
fi

[ -f "${controlfolder}/mod_${CFW_NAME}.txt" ] && source "${controlfolder}/mod_${CFW_NAME}.txt"

get_controls

cd $GAMEDIR

> "$GAMEDIR/log.txt" && exec > >(tee "$GAMEDIR/log.txt") 2>&1

echo "Tamatama $(cat VERSION 2>/dev/null): tamatama.$ARCH on $DEVICE_ARCH ($CFW_NAME)"

# It saves next to itself: resources/ (what it extracts from the .apk and the
# saved games), keyboard.map, controller.map and screenshots/.
export QEMU_ARM="$GAMEDIR/qemu-arm.aarch64"
export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"

$ESUDO chmod +x "$GAMEDIR/tamatama.$ARCH" "$GAMEDIR/loader-arm" "$GAMEDIR/qemu-arm.aarch64"

# Tamatama reads the controller itself (SDL), so there is no gptokeyb: in the
# menu any face button or Start accepts, "Configure controls" saves
# controller.map, and Select twice quits the game and goes back to the menu.
pm_platform_helper "$GAMEDIR/tamatama.$ARCH"
./tamatama.$ARCH --menu

pm_finish
