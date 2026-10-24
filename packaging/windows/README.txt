Y2JB Installer for Windows
==========================

This ZIP contains one executable. There is nothing to install; double-click
y2jb-installer.exe.

Requirements
------------
- Windows 10 (1803 or later) or Windows 11.
- Microsoft Edge WebView2. It is built into Windows 11 and installed with
  Edge on Windows 10. If it is missing, install the Evergreen WebView2
  Runtime from Microsoft.
- The PS5 must be on firmware 4.03-13.60, with kernel read/write access and
  an ELF loader listening on port 9021. Firmware 11.40 is not supported by
  the Y2JB autoloader.

No administrator rights, WSL, MSYS2, Python or .NET are needed. The
executable carries its own runtime except for WebView2, which is part of
Windows.

First launch
------------
Windows SmartScreen will warn that the app is unsigned. Choose "More info",
then "Run anyway". The build is not code-signed.

Where data is kept
------------------
Downloads and settings live in:

    %APPDATA%\dev.y2jb.installer

Delete that folder to reclaim the space or start fresh. Remove the app by
deleting the executable.

Before launching YouTube
------------------------
Disconnect the console from the internet first. Opening YouTube online can
show the sign-in screen and overwrite download0.dat, which breaks the Y2JB
setup and requires reinstalling it.

Safety
------
The app talks to Sony's package servers and GitHub from this PC, and to the
console on your local network. It does not contain an exploit. The payload it
sends runs only on a console that already has kernel read/write access.

License: GPL-3.0-or-later.
