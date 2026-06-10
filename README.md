<img width="3840" height="2160" alt="Screenshot (154)" src="https://github.com/user-attachments/assets/19079bfc-47a2-4d61-9e6f-8cfbe4b51909" />
<img width="256" height="256" alt="fliip3d_light (Benutzerdefiniert)" src="https://github.com/user-attachments/assets/3204d3d5-41c6-47ca-916b-b3e64ed8324b" />
<img width="256" height="256" alt="flip3d_dark (Benutzerdefiniert)" src="https://github.com/user-attachments/assets/d3a3fa09-cda9-44ef-a194-b19eba0c4b3e" />


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
