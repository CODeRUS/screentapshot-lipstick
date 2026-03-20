# screentapshot-lipstick
Simple screenshot application with overlay button.

Build with Qt: open **`screentapshot-lipstick.pro`**.

## Building RPMs in Docker

Same approach as the GitHub Action [coderus/github-sfos-build](https://github.com/CODeRUS/github-sfos-build): a **Linux** container `coderus/sailfishos-platform-sdk` with a chosen **release tag** runs **`mb2`** against the mounted tree. On **Windows**, use **Docker Desktop** with **Linux containers** (WSL2 backend).

From the repo root:

```powershell
.\build-docker.ps1
```

Optional parameters (defaults match `.github/workflows/build.yml`):

```powershell
.\build-docker.ps1 -Release 4.6.0.13 -Arch i486
.\build-docker.ps1 -Arch armv7hl
```

- **`-Release`** — image tag; available tags: [Docker Hub → sailfishos-platform-sdk](https://hub.docker.com/r/coderus/sailfishos-platform-sdk/tags).
- **`-Arch`** — `mb2` target: **`i486`** (e.g. tablet/emulator) or **`armv7hl`** (phones).
- **`-ImageSuffix`** — only if you use a non-standard image name.

The script runs `docker run --rm --privileged` with the repo mounted at **`/workspace`**; built packages are copied to **`RPMS/`** next to the sources. Ensure the Docker daemon is running before execution.

Equivalent one-liner (bash/Linux), for reference:

```bash
docker run --rm --privileged -e INPUT_RELEASE=4.6.0.13 -e INPUT_ARCH=i486 \
  -v "$PWD:/workspace" coderus/sailfishos-platform-sdk:4.6.0.13 \
  /bin/bash -lc 'mkdir -p build && cd build && cp -r /workspace/* . && mb2 -t SailfishOS-${INPUT_RELEASE}-${INPUT_ARCH} build && mkdir -p /workspace/RPMS && shopt -s nullglob && cp -v RPMS/*.rpm /workspace/RPMS/'
```

Screen grabs use the **lipstick Wayland protocol** (`lipstick_recorder`), same family as [screencast](https://github.com/coderus/screencast): SHM buffers, `record_frame`, and `repaint`. The app must run in a **privileged** session (as configured via `mapplauncherd` / `privileges.d`) so lipstick allows binding `lipstick_recorder_manager`.

**SHM format:** create Wayland SHM buffers as **`WL_SHM_FORMAT_ARGB8888`** (Qt compositor) while reading pixels as **`QImage::Format_RGBA8888`** for lipstick’s **GL_RGBA** fill — same as [screencast](https://github.com/coderus/screencast).

Settings are stored under **`/apps/screentapshot-lipstick/`** (previously `/apps/harbour-screentapshot/` when the package was named harbour-screentapshot2).
