PS TOUCH PC — PORTABLE BUILD LAYOUT

This project is designed to be distributed as an extracted folder, without an installer.
Expected release layout:
  PS-Touch.exe
  *.dll          runtime DLLs beside the executable for direct Windows loading
  licenses/      third-party license notices included with the package
  fonts/         optional user-supplied .ttf, .otf and .ttc files; never install system-wide
  resources/     icons, UI resources and shaders with redistribution rights
  config/        user settings stored locally beside the application
  projects/      optional user projects
  cache/         disposable local cache
  logs/          diagnostic logs

The current CI preview ZIP places runtime DLLs beside PS-Touch.exe because Windows does not search arbitrary subfolders for dependencies. A future package may use a runtime subfolder only if it includes a launcher or configures DLL search safely.

The application must resolve writable paths relative to its own executable directory,
not rely on the current working directory, registry state, or machine-wide packages.
Do not put private credentials or cloud tokens in this folder.

This repository snapshot is a development POC. It does NOT contain a final PS Touch
replacement or a compiled Windows executable. The Win32 UI is an adaptive shell; its
image preview uses Windows GDI+ and does not yet connect all editor commands to the
native image core. PSD preview is not guaranteed in this shell.

Iteration 13 adds a mockup placement prototype: open a product/background image, use the Mockup command to import a design image (transparent PNG works best), position it with arrow keys, resize with mouse wheel or +/- and press Enter/Aplicar to flatten it into the image. Escape cancels placement. This is not a perspective-warp Smart Object implementation yet.
