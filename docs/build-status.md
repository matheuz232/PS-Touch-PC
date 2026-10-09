# Build and test status — iteration 4

- Core RGBA/image math tests: PASS (strict warnings enabled, warnings as errors).
- PNG encode/decode round trip: PASS, including alpha preservation.
- CMake project registers separate core and image-I/O test targets.
- JPEG encode/decode smoke test: PASS, decoded image is opaque as expected.
- Unsupported file extension rejection: PASS.
- CLI smoke test: PASS, reads PNG, applies the APK-derived brightness formula, writes PNG, and output pixel values/dimensions were checked.
- AddressSanitizer + UndefinedBehaviorSanitizer image I/O test: PASS.
- Windows x64 build: NOT RUN in this Linux sandbox. CMake executable is absent here, so Windows-specific build and UI integration remain unverified.
- PS Touch project format, brushes, layer model, history, GPU filters, and original UI: NOT IMPLEMENTED in this iteration.

- Native layer/document model: PASS (layer order, alpha compositing, visibility, opacity, offsets, and blend-mode metadata).
- Custom PTDOC project round-trip: PASS (dimensions, document name, layer names/pixels/metadata).
- Undo/redo and 20-checkpoint retention: PASS.
- CMake executable was unavailable in this sandbox; sources were compiled directly with g++ and strict warnings. Windows x64 build remains unverified.
- Flattened PSD v1 writer: header and raw planar channel test added; full compatibility with Photoshop must still be validated; a smoke test successfully reopened the exported file using Pillow as PSD/RGBA and verified dimensions and pixel values.


## Iteration 8
- Added bounded flattened PSD v1 import for 8-bit RGB/RGBA, supporting raw and PackBits RLE channel data.
- Added layer duplicate, rename, visibility, opacity, and existing reorder operations.
- Added separate PSD-import and layer-operation test targets.
- Limitations: layered PSD import/export, masks, adjustment layers, CMYK, 16-bit, and native PS Touch file formats remain unsupported.

## Iteration 9
- CLI detects `.psd` inputs/outputs and routes through the native PSD reader/writer; PNG/JPEG flow remains available.
- PSD PackBits row lengths are bounded before allocation to reject hostile or malformed RLE tables.
- Added regression coverage for oversized PSD RLE row lengths.

## Iteration 10
- Added native crop, horizontal/vertical flip, 90-degree rotation, grayscale, sepia, and saturation operations.
- CLI exposes grayscale, sepia, saturation, flip, and rotate through `--op`.
- Added image-operation regression suite covering alpha preservation and invalid crop/parameter handling.

## Iteration 11 — adaptive Win32 UI shell
- Added `windows/pstouch_win32.cpp`: native Win32 window, GDI+ image preview, open dialog, wheel zoom, checkerboard canvas, responsive tool/layer panel collapse thresholds, and per-monitor DPI awareness.
- Added a Windows-only CMake target named `PS-Touch` linking system `gdiplus` and `comdlg32`.
- Added `portable/README.txt` with the no-installer folder layout and portable-path constraints.
- Verification limitation: this Linux sandbox lacks a Windows compiler/runtime, so the GUI target is source-reviewed only and has not been compiled or launched. Existing native core regression tests remain separate; this UI shell is not yet connected to the image-editing core.

## Iteration 12 — save, filters, undo/redo
- Added native Save As through GDI+ image encoders (PNG, JPEG, BMP, TIFF).
- Added grayscale and sepia pixel operations, each recording a prior bitmap snapshot.
- Added bounded 20-step snapshot undo, redo, and Ctrl+S/Ctrl+Z/Ctrl+Y shortcuts.
- Open dialog now only advertises supported GDI+ formats, avoiding a false promise of PSD support in the current shell.
- Verification limitation remains: no Windows compiler/runtime is available in the sandbox, so this Win32 GUI source is not compiled or executed here. Core image and image-operation suites are independently compilable on Linux.


## Iteration 13 — mockup placement
Added a Win32 mockup-placement prototype: user opens a base/product photo, imports a design image, previews it centered over the base, repositions with arrow keys, scales with wheel or `+`/`-`, then applies or cancels. Applying creates one flattened composite and stores an undo snapshot. This does not perform perspective warping or lighting/material-aware wrapping and is not equivalent to Photoshop's Smart Object mockups. Windows compilation and runtime validation remain outstanding because this sandbox has no Windows toolchain.


## Iteration 14 — Windows replacement semantics and CI
- Added a cross-platform finalization helper for `.ptdoc` and flattened `.psd` saves. Windows uses `MoveFileExA` with `MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH`; POSIX uses `std::rename`. This fixes the Windows case where a second save to an existing destination can fail.
- Added a regression to save a `.ptdoc` twice to the same destination and reload the newer content.
- Added `vcpkg.json` for libpng/libjpeg-turbo and `.github/workflows/native-build.yml` with Linux/GCC and Windows x64/MSVC build/test jobs, including the Win32 application target on Windows.
- Local verification: all seven native test executables compiled with GCC using `-Wall -Wextra -Wpedantic -Wconversion` and passed, including the new overwrite regression.
- CI status is pending an actual GitHub Actions run; Windows/MSVC compilation and GUI execution are not claimed as passed from this sandbox.


## Iteration 15 — editor-first scope and transform tools
- Removed the desktop Photoshop plugin compatibility document; plugin runtimes, plugin managers, CEP/UXP/JSX and native .8bf support are no longer part of the project scope.
- Refocused the README roadmap on the editor's own document model, layers, PSD I/O, masks, transforms and selection tools.
- Added Win32 toolbar commands for 90-degree rotation in both directions and horizontal/vertical flipping. Each transform snapshots the previous image so Ctrl+Z can undo it; transform controls are hidden while mockup placement is active to preserve the Apply/Cancel interaction.
- These new GUI commands are committed but still require Windows/MSVC CI verification. The Win32 shell remains separate from the native document/layer core; wiring that integration is the next major engineering task.


## Iteration 16 — custom font discovery
- Added `fonts/README.txt` as the drop-in location for custom `.ttf`, `.otf` and `.ttc` files.
- At startup, the Win32 shell creates/scans the folder beside the executable and loads supported files privately into the process; successfully loaded file count is shown in the status bar. Loaded fonts are released on shutdown and are not installed system-wide.
- Font discovery code and toolbar syntax correction are committed. Windows/MSVC CI must confirm the build; the current text tool is still not implemented, so this stage loads fonts for future text features rather than providing font selection in a text editor.
