# PS Touch Native Core — proof of concept

This is the first platform-neutral native-core experiment derived from the APK's discovered image-processing responsibilities. It is **not** a converted Photoshop Touch application and does not yet open the original SWF, reproduce the full filter suite, or generate a complete PS Touch EXE. PSD v1 import now supports flattened RGB/RGBA files and editable layer stacks within the documented compression limits; layered PSD v1 export is also available.

## Included
- RGBA8 image buffer with dimension/allocation guards.
- PNG and JPEG decoding/encoding via libpng and libjpeg.
- A small CLI that opens an image, applies the brightness/contrast primitive, and writes a result.
- Native document/layer model with layer order, visibility, opacity, offsets, and nine blend modes modeled on the blend-mode list found in the decompiled application.
- A versioned, bounded custom `.ptdoc` project format for round-tripping RGBA layer pixels and metadata.
- PSD v1 import for flattened 8-bit RGB/RGBA (raw and PackBits RLE) and layered 8-bit RGB documents with raw or PackBits RLE layer channels; export is available both as flattened images and editable layer stacks. The Win32 shell opens layered PSD into native layers and exports PSD from the current layer stack. PSD file operations accept UTF-8 paths, including non-ASCII filenames on Windows. Layer masks and adjustment layers are not supported by the layered importer.
- Layer operations: duplicate, rename, reorder, visibility, opacity, and snapshot-based undo/redo history with a 20-step cap and a 128 MiB estimated history-memory budget. Older snapshots are discarded first; if a single snapshot would exceed the budget, history is cleared rather than allocating another huge copy.
- Low-memory performance work: layer compositing clips each layer to the visible canvas before iterating, skips off-canvas pixels, and uses contiguous row-major pixel access to avoid per-pixel bounds checks. This reduces unnecessary CPU work for partially visible or oversized layers.
- Alpha premultiplication/unpremultiplication.
- Straight-alpha source-over compositing.
- Bilinear image resampling, crop, horizontal/vertical flip, 90-degree rotation, grayscale, sepia, saturation adjustment, color inversion, and configurable posterization.
- 100 built-in photo-grade presets across ten families (Natural, Warm, Cool, Vintage, Cinema, Fade, Vivid, Matte, Teal Orange, and Monochrome), each with ten strengths. Presets run on the CPU, preserve alpha, and expose names through the native API.
- Brightness/contrast primitive ported from the formula in the APK’s `contrastbrightness.fs` shader (still requires GPU-vs-CPU pixel-parity validation).
- Unit tests for the primitives above.

These functions are compatibility building blocks only. Pixel semantics must be compared against the original app before replacing its native extension behavior.

## Build on Windows 10/11 x64
Install Visual Studio 2022 Build Tools with the **Desktop development with C++** workload and CMake 3.20+. The repository now declares libpng and libjpeg-turbo in `vcpkg.json`; the Windows CI workflow configures these dependencies automatically.

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The test targets are console test executables, not the final application. GitHub Actions now defines Linux/GCC and Windows/MSVC build-and-test jobs; check the Actions tab for actual runner results. Local sandbox validation is not a substitute for a successful Windows CI run. The current porting gate is broader PSD interoperability, Windows GUI validation, and representative shader/filter output compared against reference output. GPU implementation and original ActionScript UI integration are not yet included.

## Command-line prototype

```powershell
.\build\Release\pstouch-image.exe input.png output.png 0.1 0.2
```

This validates an end-to-end native image pipeline only; it is not yet the PS Touch UI or its proprietary document format.

## Native document model

`Document` composites layers bottom-to-top and supports the blend mode names observed in `TTLayer.as` (Normal, Darken, Multiply, Lighten, Screen, Add, Overlay, Difference, Subtract). The custom `.ptdoc` container is a new prototype format and is **not** claimed to be compatible with Adobe PSD or PS Touch internal cache files. Flattened PSD import accepts 8-bit RGB/RGBA PSD v1 files with raw or PackBits RLE compression. Layered import supports RGB 8-bit layer records with raw or PackBits RLE channel compression and the standard blend modes represented by this model; it rejects unsupported blend modes and masks while preserving layer offsets that extend beyond the canvas. Layered export writes editable RGBA layers using raw channel compression. Adjustment layers, CMYK, 16-bit, and PS Touch proprietary formats are not supported. The current layer model is a compatibility-oriented starting point, not a claim of pixel-perfect equivalence with the original GPU renderer.

## Windows UI shell and portable distribution

A first Win32 desktop shell now lives in `windows/pstouch_win32.cpp`. It uses per-monitor DPI awareness, recalculates its layout on `WM_SIZE`, scales the image preview to the available canvas, and reduces/collapses side panels on narrow windows. It provides image open/save, grayscale/sepia filters, undo/redo, mockup placement, rotation and flips. The grayscale/sepia and transform commands route through the shared `pstouch_image_core` RGBA image operations via explicit GDI+ conversion helpers. The live layer panel, opacity, visibility, duplication/removal, text raster layers, and mockup artwork now use the native `Document` composite and checkpoint history for their native operations. A sibling `fonts/` directory is scanned at startup; `.ttf`, `.otf` and `.ttc` files that Windows accepts are loaded privately for the process and the successful file count is shown in the status bar. The fonts are not installed system-wide. This remains a UI integration POC, not the complete editor: the text tool now supports a basic click-to-place workflow (select Texto, click the canvas, type, Enter to commit, Esc to cancel) with white 32 px Arial text and undo; text remains rasterized after placement; post-placement text editing and true editable text layers are not yet implemented. GDI+ preview formats are Windows-dependent and PSD preview is not promised.

Portable packaging notes are in `portable/README.txt`. The intended release is a folder containing the EXE, required redistributable runtime files and resources, with configuration/cache/log paths kept beside the application. No installer is planned. The Linux sandbox used for this iteration does not provide an MSVC/Windows GUI runtime, so the Win32 target has not been compiled or executed here.

### Historical iteration 12 — first interactive editing commands
The Win32 shell now includes native Save As (PNG/JPEG/BMP/TIFF encoders), grayscale and sepia commands, a bounded 20-snapshot undo history, redo, and Ctrl+S/Ctrl+Z/Ctrl+Y shortcuts. These commands currently operate on a GDI+ bitmap owned by the UI shell; they are intentionally separate from the native `pstouch_image_core` and are not parity-tested against Photoshop Touch. This was the state at iteration 12; layered PSD import/export and native document-layer integration were implemented in later iterations. The full original editor is still not complete.


### Iteration 13 — mockup composition prototype
The Win32 shell now has a first mockup-composition workflow: open a product/background photo, choose a separate artwork image (transparent PNG recommended), reposition it with the arrow keys, resize with the mouse wheel or `+`/`-`, then commit or cancel. The result is flattened into the current bitmap and can be undone with Ctrl+Z. This is a useful placement mockup, but it is **not yet a Photoshop Smart Object mockup**: perspective/mesh warp, surface displacement, automatic object/material detection, non-destructive linked smart objects, masks, and lighting-aware blending are not implemented.

The latest reliability update also fixes save-over-existing behavior for `.ptdoc` and flattened `.psd` output on Windows using the Win32 replacement API; Linux retains `rename` semantics. A regression test covers saving a project twice to the same path.

The remaining roadmap is focused exclusively on the editor itself: (1) validate layered PSD roundtrips in Photoshop/GIMP and improve interoperability, (2) non-destructive adjustment layers and masks, (3) transform/perspective/warp tools for mockup composition, (4) editable text/vector layers and layer styles, (5) selection/refine-edge tools, and (6) content-aware fill as a separately evaluated image-editing capability. These are roadmap items, not features claimed as already working.


### Iteration 17 — first text placement workflow

The Win32 shell now has a basic keyboard-driven text tool: select **Texto** in the left tool list, click inside the image, type text, press **Enter** to insert a line break, **Ctrl+Enter** to commit, or **Esc** to cancel. Backspace also removes a complete UTF-16 surrogate pair when deleting a supplementary Unicode character. Committing creates an undo snapshot. The Windows font dialog now lets the user select a font family, size, style, and color before placing text. Font files from `fonts/` are also registered in a GDI+ private font collection for rendering, in addition to process-private Windows registration. The canvas now previews the selected font family, style, size and color while typing, scaled to the current canvas zoom. Text is still rasterized directly into the image when committed; editing text after placement and true editable text layers remain future work.

### Iteration 16 — custom font discovery

The Win32 shell now creates and scans a `fonts/` directory beside the executable at startup. It attempts to load `.ttf`, `.otf` and `.ttc` font files using the Windows private-font API, without installing them system-wide, and displays the number of successfully loaded font files in the status bar. `fonts/README.txt` documents usage and licensing expectations. Restart the application after changing the folder. This prepares fonts for future text tools; the text tool itself is not yet implemented.


### Iteration 18 — shared native image operations in the Windows shell

The Win32 executable now links against `pstouch_image_core`. Grayscale, sepia, 90-degree rotation, and horizontal/vertical flips convert the GDI+ canvas to the core RGBA image representation, run the core operation, and convert back only after a successful result. The UI retains its existing undo snapshots, so failed conversions do not commit partial edits. This was the first operational bridge at iteration 18. Later iterations connected the native `Document` model to the GUI layer panel, document compositing, layer operations, `.ptdoc` projects, and layered PSD import/export.


### Iteration 19 — regression coverage for document composition and history

Expanded the native document-operation test to assert that compositing honors layer offsets, top-layer ordering and visibility, and that checkpoint-based undo/redo restores layer state in both directions. This protects the core behavior needed before the Win32 layer panel is connected to `Document`; it does not claim that the UI layer panel is integrated yet.


### Iteration 20 — first live document/layer panel integration

The Win32 shell now initializes a native `Document` when opening an image and renders its composite. The layer panel lists document layers and supports adding a transparent layer, selecting a layer, duplicating it, removing it (while preserving at least one layer), toggling visibility, and changing opacity from the panel bar. Grayscale/sepia and flip/rotation operations target the selected layer; placed text and mockup artwork are created as separate transparent layers instead of being flattened into the existing image. Undo/redo consults document history when available. The initial image state is checkpointed so the first edit can be undone. This is the first real connection to the layer model; `.ptdoc` project-file dialogs, editable vector/text objects, and full Unicode layer-name editing are not yet wired into the UI.


### Iteration 19 — native project workflow and live layers

The Windows shell now offers **Abrir proj.** and **Salvar proj.** toolbar actions for the custom `.ptdoc` format, in addition to **Ctrl+Shift+O** and **Ctrl+Shift+S**. Saving preserves the native layer stack, visibility, opacity, offsets, blend-mode metadata and RGBA pixels rather than exporting only the flattened preview. Opening a project rebuilds the composite preview from the document and seeds the native undo history. Project paths are converted from Windows wide-character paths to UTF-8 and resolved through `std::filesystem::u8path`, including Unicode filenames.

The ordinary **Salvar** command exports PNG/JPEG/BMP/TIFF or PSD; PSD export preserves the current layer stack where supported. Use **Salvar proj.** to preserve all native `.ptdoc` metadata and pixels. `.ptdoc` is this project's own format, not the original Photoshop Touch document format. CI validation for the project-dialog and Unicode-path commits is pending; the Windows GUI has not been interactively exercised in this environment.


### Iteration 20 — transaction history and canvas rotation

Editor layer mutations now record the resulting document state after each completed operation. This aligns the Win32 layer actions with the native history model so undo restores the previous state and redo restores the edited state for visibility, opacity, layer add/remove/duplicate, text placement, mockup placement, filters and transforms. Regression coverage now follows this mutate-then-checkpoint transaction order.

The rotate commands now rotate the complete document canvas and all layer pixels/offsets together, swapping canvas dimensions and preserving off-canvas placement where representable. This avoids clipping wide canvases or layers merely because a single layer's dimensions changed. A regression test covers rotation of a non-square canvas, offset transformation, composite output and undo/redo. The latest CI runs for these changes must complete before the Windows build can be considered verified.
