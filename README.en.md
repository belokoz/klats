<p align="center"><img src="docs/images/icon.png" width="128" alt="Klats"></p>
<h1 align="center">Клац (Klats)</h1>
<p align="center">Select text, release ⌥⌘, the keyboard layout is fixed.<br>A free macOS utility with one job. No autocorrect, no dictionaries, and Klats never sends your text anywhere.</p>
<p align="center"><a href="README.md">Русский</a> · <a href="https://github.com/belokoz/klats/releases/latest">Download</a> · <a href="docs/HISTORY.md">How it was made</a></p>

## What it does

- **⌥⌘** retypes the selected text in the other keyboard layout: `ghbdtn` becomes `привет`, `руддщ` becomes `hello`. It picks the direction itself and switches the system layout afterwards, so you keep typing in the right one.
- **⌥⌘Z** changes the case of the selection: `пРИВЕТ` becomes `Привет`. An all-lowercase mode is in Settings.
- Nothing else. No automatic switching while you type, no autocorrect, no dictionaries, no statistics, no sounds. Klats goes online once per launch: it asks GitHub for the latest version number and offers to download a newer one. Klats never sends your text anywhere, and the check can be turned off in the settings.

Hotkeys are configurable, including modifier-only combinations. Klats works with any pair of layouts, not just Russian and English: the mapping tables are built from the real system layouts instead of being hard-coded.

<p align="center"><img src="docs/images/menu.png" width="360" alt="Klats menu"></p>
<p align="center"><img src="docs/images/settings.png" width="480" alt="Klats settings"></p>

## Install

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

### Build from source

No Xcode needed, the Command Line Tools are enough (`xcode-select --install`). Building needs Swift 6, i.e. Command Line Tools 16 or newer, which install only on macOS 14.5 or newer (tested with Swift 6.3). On macOS 12 and 13, use the DMG; it is universal.

```bash
git clone https://github.com/belokoz/klats.git && cd klats
./scripts/install.sh        # build and put into /Applications
./scripts/test.sh           # core tests
```

## How it works

One press. Klats catches the release of ⌥⌘, takes the selection with ⌘C, converts it, pastes it back with ⌘V, restores the clipboard and switches the system layout. That is why it works everywhere ⌘C and ⌘V work, and not in terminals.

The character-to-key tables are built from the system's layouts with `UCKeyTranslate` in a fraction of a millisecond: character, the key it lives on in the source layout, the character on that key in the target layout. The direction comes from the text: characters that exist in only one of the two layouts vote.

Details live in the [concept](docs/CONCEPT.md) and [design](docs/DESIGN.md) documents (Russian). The log at `~/Library/Logs/Klats/klats.log` never contains text: only the app, timing, lengths and outcome.

## Limitations

- Made a mistake or changed your mind: ⌘Z undoes the replacement like any paste.
- Terminals, remote desktops and games are not supported. In password fields Klats does nothing.
- Plain text is pasted; formatting comes from the surroundings.
- Input methods (Japanese, Chinese, Korean) are not part of a pair.

## Plans

- 1.1: clipboard history, like Win+V on Windows, on ⌃V.
- A Windows version.

## How it was made

Klats was written in three days in a dialogue with Claude Code. The author set the tasks, reviewed the artifacts and corrected course; Claude wrote the code and the documents. Concept, design, tested core, app, release, each stage reviewed. The story with dates, forks and numbers: [docs/HISTORY.md](docs/HISTORY.md) (Russian).

## From the author

🎙 [Диктуй](https://diktuy.ru/?utm_source=klats&utm_medium=github&utm_campaign=readme): voice to text.

## License

[MIT](LICENSE).
