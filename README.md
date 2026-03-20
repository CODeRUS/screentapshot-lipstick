# screentapshot-lipstick
Simple screenshot application with overlay button.

Build with Qt: open **`screentapshot-lipstick.pro`**.

Screen grabs use the **lipstick Wayland protocol** (`lipstick_recorder`), same family as [screencast](https://github.com/coderus/screencast): SHM buffers, `record_frame`, and `repaint`. The app must run in a **privileged** session (as configured via `mapplauncherd` / `privileges.d`) so lipstick allows binding `lipstick_recorder_manager`.

**PIE / mapplauncherd:** if the grid shows *cannot dynamically load executable*, the binary was not linked as PIE; `screentapshot-lipstick.pro` sets **`-fPIE` / `-pie`** so `booster-silica-qt5` can `dlopen()` the app.

**Desktop / Sailjail:** apps in `privileges.d` (real GID `privileged` for `lipstick_recorder_manager`) **cannot** run under the default Sailjail sandbox: firejail hits *Error: can't chdir to privileged* and the invoker bails out. The `.desktop` file therefore sets **`[X-Sailjail]` + `Sandboxing=Disabled`**. `mapplauncherd` still applies `privileges.d` for the binary path.

**SHM format:** create Wayland SHM buffers as **`WL_SHM_FORMAT_ARGB8888`** (Qt compositor) while reading pixels as **`QImage::Format_RGBA8888`** for lipstick’s **GL_RGBA** fill — same as [screencast](https://github.com/coderus/screencast).

Settings are stored under **`/apps/screentapshot-lipstick/`** (previously `/apps/harbour-screentapshot/` when the package was named harbour-screentapshot2).
