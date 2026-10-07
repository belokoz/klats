<p align="center"><img src="docs/images/icon.png" width="128" alt="Klats"></p>
<h1 align="center">Клац (Klats)</h1>
<p align="center">Select text, press ⌥⌘ on a Mac or Win+Alt on Windows, the keyboard layout is fixed.<br>A free utility for macOS and Windows with one job. No autocorrect, no dictionaries, and Klats never sends your text anywhere.</p>
<p align="center"><a href="https://github.com/belokoz/klats/releases/latest">Download for macOS</a> · <a href="https://github.com/belokoz/klats/releases/tag/windows-v0.1.0">Download for Windows</a> · <a href="README.md">Русский</a> · <a href="docs/HISTORY.md">How it was made</a></p>

## What it does

- **⌥⌘** on a Mac and **Win+Alt** on Windows retype the selected text in the other keyboard layout: `ghbdtn` becomes `привет`, `руддщ` becomes `hello`. Klats picks the direction itself and switches the system layout afterwards, so you keep typing in the right one.
- **⌥⌘Z** and **Win+Alt+Z** change the case of the selection: `пРИВЕТ` becomes `Привет`. An all-lowercase mode is in Settings.
- Nothing else. No automatic switching while you type, no autocorrect, no dictionaries, no statistics, no sounds. Klats goes online once per launch: it asks GitHub for the latest version number and offers to download a newer one. Klats never sends your text anywhere, and the check can be turned off in the settings.

Hotkeys are configurable, including modifier-only combinations. Klats works with any pair of layouts, not just Russian and English: the mapping tables are built from the real system layouts instead of being hard-coded.

<p align="center"><img src="docs/images/menu.png" width="360" alt="Klats menu"></p>
<p align="center"><img src="docs/images/settings.png" width="480" alt="Klats settings"></p>

## Install on a Mac

1. Download `Klats-0.2.0.dmg` from [Releases](https://github.com/belokoz/klats/releases/latest), open it and drag Клац to Applications.
2. Open Klats. macOS will not start it right away: there is no paid Apple account behind the project, so it cannot verify the developer. Confirm the launch once, after installing and after every update:
   - **macOS 15 and newer.** Double-click Клац in Applications. In the dialog click **Done** (not **Move to Trash**). Open System Settings (the Apple menu in the top left corner of the screen) → Privacy & Security, scroll down to Security and click **Open Anyway**. Enter your Mac password and click **Open**.
   - **macOS 12–14.** In Applications, right-click Клац (or Control-click it) and choose **Open**. In the dialog click **Open** again.
   - **Didn't work?** No **Open** button, or “The application “Клац” can’t be opened.”: see [below](#if-klats-will-not-open), it takes two minutes.
3. Klats asks for the Accessibility permission. It needs it to copy the selected text and paste the fixed one back. Enable Клац in the list (on macOS 12, click the lock at the bottom of the window first), return to the welcome window and try it on the sample field.

When a new version is out, Klats offers to download it at launch. It installs the same way, replacing the old copy, and keeps the Accessibility permission: the app is signed with a permanent certificate.

Requirements: macOS 12 Monterey or newer, Apple Silicon or Intel. The interface is in Russian and English.

To uninstall, quit Klats from its menu and drag it to the Trash. On macOS 12, first turn off «Launch at login» in the settings, or `io.github.belokoz.klats.login.plist` stays behind in `~/Library/LaunchAgents`.

### If Klats will not open

This happens especially when the DMG was sent through a messenger: macOS marks the file as downloaded from the internet and refuses to open it, even with a right-click. One command removes the mark.

1. Open Terminal: Finder → Applications → Utilities → Terminal. Or press ⌘ and Space, type Terminal and press Return.
2. Copy this line in full (the copy button is on its right):

   ```
   xattr -dr com.apple.quarantine /Applications/Клац.app
   ```

3. Click into the Terminal window, paste the line (⌘V) and press Return. If Terminal prints nothing, it worked.
4. Close Terminal and double-click Клац in Applications.

The command changes Klats and nothing else. If Terminal answers `Permission denied`, paste the same line again with `sudo` and a space in front, press Return and type your Mac password: it stays invisible while you type, that is normal.

## Install on Windows

1. Download `Klats-0.1.0-windows-x64.exe` from the [release page](https://github.com/belokoz/klats/releases/tag/windows-v0.1.0) and run it. Your browser may warn that the file is not commonly downloaded: confirm that you want to keep it.
2. Windows shows “Windows protected your PC”: Klats has no paid signature, so SmartScreen does not know the publisher. Click **More info**, then **Run anyway**. The same happens with every update.
3. Klats installs without administrator rights, into your user folder, and starts right away. Try Win+Alt on the sample field in the welcome window.

The Klats icon lives in the notification area next to the clock. Windows hides new icons behind the ^ arrow: drag it from there onto the taskbar to keep it at hand. Settings open from the icon's menu or by starting Klats again.

When a new version is out, Klats offers to download it at launch. The new installer closes Klats, installs over the old version and starts it again; your settings stay.

<p align="center"><img src="docs/images/windows-settings.png" width="480" alt="Klats settings on Windows"></p>

Requirements: Windows 10 version 1903 or newer, or Windows 11, 64-bit. The interface is in Russian and English.

To uninstall: Settings → Apps → Клац → Uninstall.

## Build from source

**Mac.** No Xcode needed, the Command Line Tools are enough (`xcode-select --install`). Building needs Swift 6, i.e. Command Line Tools 16 or newer, which install only on macOS 14.5 or newer (tested with Swift 6.3). On macOS 12 and 13, use the DMG; it is universal.

```bash
git clone https://github.com/belokoz/klats.git && cd klats
./scripts/install.sh        # build and put into /Applications
./scripts/test.sh           # core tests
```

**Windows.** Needs Visual Studio Build Tools 2022 with the “Desktop development with C++” workload (the compiler, Windows SDK, CMake and Ninja come with it), and Inno Setup 6 for the installer.

```powershell
git clone https://github.com/belokoz/klats.git; cd klats\windows
.\build.ps1             # build build\release\Klats.exe and run the core tests
.\build.ps1 -Installer  # and the installer
```

## How it works

One press. Klats catches ⌥⌘ or Win+Alt, takes the selection with a copy, converts it, pastes it back, restores the clipboard and switches the system layout. It copies and pastes with ⌘C and ⌘V on a Mac and with Ctrl+Insert and Shift+Insert on Windows. That is why it works everywhere copy and paste work, and not in terminals.

The character-to-key tables are built from the system's layouts in a fraction of a millisecond, with `UCKeyTranslate` on a Mac and `ToUnicodeEx` on Windows: character, the key it lives on in the source layout, the character on that key in the target layout. The direction comes from the text: characters that exist in only one of the two layouts vote.

Details live in the [concept](docs/CONCEPT.md) and [design](docs/DESIGN.md) of the Mac version and the [concept](docs/windows/CONCEPT.md) and [design](docs/windows/DESIGN.md) of the Windows version (Russian). The log, `~/Library/Logs/Klats/klats.log` on a Mac and `%LOCALAPPDATA%\Klats\klats.log` on Windows, never contains text: only the app, timing, lengths and outcome.

## Limitations

- Made a mistake or changed your mind: ⌘Z on a Mac and Ctrl+Z on Windows undo the replacement like any paste.
- Terminals, remote desktops and games are not supported. In password fields Klats does nothing.
- On Windows, Klats does not work in windows of programs run as administrator: that is how Windows protects them.
- Plain text is pasted; formatting comes from the surroundings.
- Input methods (Japanese, Chinese, Korean) are not part of a pair.

## Plans

- 1.1: clipboard history, like Win+V on Windows, on ⌃V.

## How it was made

Klats was written in a dialogue with Claude Code: the Mac version in three days, the Windows version in two hours. The author set the tasks, reviewed the artifacts and corrected course; Claude wrote the code and the documents. Concept, design, tested core, app, release, each stage reviewed. The story of the Mac version with dates, forks and numbers: [docs/HISTORY.md](docs/HISTORY.md) (Russian).

## From the author

🎙 [Диктуй](https://diktuy.ru/?utm_source=klats&utm_medium=github&utm_campaign=readme): voice to text.

## License

[MIT](LICENSE).
