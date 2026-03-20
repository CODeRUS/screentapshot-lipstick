# screentapshot-lipstick
Simple screenshot application with overlay button.

Build with Qt: open **`screentapshot-lipstick.pro`**.

Screen grabs use the **lipstick Wayland protocol** (`lipstick_recorder`), same family as [screencast](https://github.com/coderus/screencast): SHM buffers, `record_frame`, and `repaint`. The app must run in a **privileged** session (as configured via `mapplauncherd` / `privileges.d`) so lipstick allows binding `lipstick_recorder_manager`.

Settings are stored under **`/apps/screentapshot-lipstick/`** (previously `/apps/harbour-screentapshot/` when the package was named harbour-screentapshot2).
