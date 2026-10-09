# PS Touch PC 1.0 — Release Checklist

This checklist defines the gates for a first public Windows x64 release. Passing CI is necessary, but does not by itself mean the product is ready to ship.

## Current verified baseline

- [x] Windows x64 MSVC configuration and build succeed in GitHub Actions.
- [x] Win32 shell and native image core compile together.
- [x] Core CTest suite passes on Windows and Linux for the latest validated workflow.
- [x] Alpha-correct bilinear resampling and Gaussian blur have regression coverage.
- [x] Repository documents current format and compatibility limitations.

## P0 — Required before calling it 1.0

- [ ] Define and test a clean portable release folder on Windows 10 and Windows 11 x64.
- [ ] Verify all runtime DLLs and third-party licenses are included or clearly documented; confirm launch on a clean Windows environment without a developer toolchain.
- [ ] Exercise the Win32 app manually: launch, open, edit, undo/redo, save as, close, and reopen.
- [ ] Verify PNG, JPEG, BMP and TIFF workflows and document PSD support/limitations accurately.
- [ ] Verify layered PSD import/export with representative files in an external editor; record unsupported cases and avoid claiming full PSD compatibility.
- [ ] Test layer creation, duplication, visibility, opacity, ordering, offsets, deletion, and .ptdoc save/reopen.
- [ ] Test filter apply/cancel, history exhaustion behavior, alpha handling, and large-image memory pressure.
- [ ] Resolve any reproducible crash, data-loss, corrupted-save, or broken-startup issue found during manual testing.
- [ ] Add a release smoke-test procedure and preserve its results as release evidence.
- [ ] Publish a versioned changelog, known-limitations list, build instructions, and portable-package instructions.

## P1 — Strongly recommended

- [ ] Validate high-DPI and narrow-window behavior on real Windows displays.
- [ ] Check Unicode and non-ASCII file paths for open/save and project operations.
- [ ] Check behavior with missing fonts, missing optional resources, and read-only install folders.
- [ ] Test representative small, medium, and large images on a low-memory machine; record memory use rather than assuming a performance target.
- [ ] Add a deterministic packaging script or workflow that creates a versioned ZIP and checksum.
- [ ] Confirm application identity, icon, About/version information, and user-facing error messages.

## Explicit non-claims

The current codebase is a native image-editor prototype and compatibility core. A successful build does **not** mean the original Android Photoshop Touch APK has been statically recompiled into a native EXE, nor that the original SWF/ActionScript UI, proprietary document formats, all original filters, GPU shader parity, or complete Photoshop Touch behavior have been reproduced.

Do not label any of those capabilities as complete unless implemented and validated with evidence.

## Release decision

A 1.0 release candidate may be tagged only after all P0 gates are checked with evidence. If any P0 item is not feasible, document the limitation and decide explicitly whether 1.0 is a clearly scoped preview or must wait; do not silently waive it.
