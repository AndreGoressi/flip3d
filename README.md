# flip3d

`flip3d` is a Windows-only Direct3D 11 prototype that recreates the classic Flip3D-style window switcher.

The app enumerates eligible top-level windows, captures them through DWM thumbnail plus Windows.Graphics.Capture interop, and renders the card stack with Direct3D 11.

## Controls
- `Win+Tab`, to open/start: Flip3d :) but only when you have installed it right "flip3d.ahk" and the Windhawk mod...!
//
- `Tab` / `Shift+Tab`, arrow keys, mouse wheel: rotate the stack
- `Enter`, release `Win`, or left click a card: activate the selected window
- `Home`: rotate back to the original front window
- `Esc`: exit
- `Space`: replay the enter animation

## Notes

- Default animation and camera values are compiled into the app
- Some preferences can be overridden through `HKCU\Software\Microsoft\Windows\DWM` or `HKLM\Software\Microsoft\Windows\DWM`
