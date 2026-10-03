
## Install

**macOS 11 or later:** download `Starfield.saver.zip`, unzip it, and double-click `Starfield.saver`. It isn't notarized by Apple (that costs money), so the first time macOS blocks it: open **System Settings → Privacy & Security** and click **Open Anyway**, or run `xattr -d com.apple.quarantine ~/Library/Screen\ Savers/Starfield.saver`. Then pick **Starfield** in **System Settings → Screen Saver**.

**Windows 10 or 11:** download `Starfield-windows.zip` and unzip it. Right-click `Starfield.scr` → **Properties**, tick **Unblock**, and click **OK**. Move it to a folder where it can stay (for every user, `C:\Windows\System32`), then right-click it and choose **Install**. It isn't code-signed, so if SmartScreen appears, click **More info → Run anyway**.

**Linux (XScreenSaver):** download the tarball for your machine (`x86_64` for most PCs, `arm64` for a Raspberry Pi 4/5 on a 64-bit OS), then:

```sh
tar xzf starfield-linux-*.tar.gz && cd starfield
sudo ./install.sh     # copies the saver and its settings into place
./install.sh --add    # adds it to your XScreenSaver list
```

Then choose **Starfield** in `xscreensaver-settings`.
