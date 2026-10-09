# Desktop Photoshop plugin compatibility

## Objective

Add an extension layer without replacing the Photoshop Touch visual language. Existing panels, dialogs, toolbars, icons, typography, spacing, transitions, and interaction patterns remain the UI baseline. New extension controls must be presented through the same visual system.

This document describes the compatibility plan; none of the plugin runtimes below is implemented by the v13 proof of concept yet.

## Compatibility is per API surface, not just file extension

A plugin loading successfully does not prove that it behaves correctly. Every supported plugin must be tested against the document model, pixel format, selection state, undo/redo, color management, and UI integration.

### Tier A — script and action bridge

- Define a documented host API for reading document metadata, enumerating layers, requesting supported image operations, and registering commands.
- Run scripts in a restricted process/runtime with explicit time, memory, and filesystem limits.
- Reject unsupported host APIs with actionable diagnostics instead of silently returning incorrect results.
- Keep document mutations transactional so one plugin command can be undone as one operation.

### Tier B — CEP/legacy panel subset

- Consider HTML/JavaScript panels and a limited JSX bridge after the host API stabilizes.
- Implement only explicitly documented bridge calls. Photoshop-specific DOM objects and methods must be mapped individually.
- Use a separate panel process and a narrow message protocol; do not expose arbitrary host memory or unrestricted shell execution.
- Report unsupported calls and the required host-version assumptions in a compatibility report.

### Tier C — UXP subset

- Implement a versioned JavaScript runtime and selected UXP-like APIs only after document/layer commands and permissions are stable.
- Model plugin permissions, manifest validation, command registration, and asynchronous document transactions.
- Do not claim general UXP compatibility: APIs differ by Photoshop release and plugin dependencies.
- Package each plugin with its declared API version and capabilities.

### Tier D — native `.8bf` filters

- Treat native filters as a separate Windows ABI project. A DLL is not compatible merely because it can be loaded.
- Start with a probe/diagnostic harness and a small, legally obtained test corpus.
- Isolate native filters in a helper process with a bounded pixel-buffer protocol; validate dimensions, channel layout, bit depth, and output bounds.
- Make crashes, timeouts, and malformed output recoverable without corrupting the open document.
- Add filters incrementally only after their entry points, host callbacks, parameter dialogs, and pixel semantics are understood.

## Required host contracts before compatibility work

1. A stable document/layer API with unique IDs and transactional mutations.
2. Undo/redo integration for all plugin writes.
3. Explicit pixel-buffer contract (RGBA order, alpha convention, row stride, color space, dimensions).
4. Selection/mask semantics and cancellation behavior.
5. A versioned plugin manifest and capability declaration.
6. A plugin manager that lists type, version, permissions, compatibility status, and failure details.
7. Automated compatibility tests for success, cancellation, invalid input, timeout, and crash recovery.

## Security and reliability

- Never execute an untrusted plugin in the main UI process.
- Require explicit user consent for file access outside the project and for network access.
- Keep installation/removal confined to the application's plugin directory.
- Do not silently elevate permissions or download/run dependencies.
- Preserve the original document if a plugin fails; commit its result only after output validation succeeds.

## Implementation gates

- **Gate 0 — current v13:** native image/document prototype and Win32 UI shell; no plugin runtime.
- **Gate 1:** stabilize document commands, layer IDs, undo transactions, and pixel-buffer contracts.
- **Gate 2:** script/action API plus plugin manifest, manager, diagnostics, and sandbox.
- **Gate 3:** evaluate CEP/JSX and UXP subsets against real test plugins and publish a compatibility matrix.
- **Gate 4:** investigate native `.8bf` filters through an isolated Windows helper and ABI tests.

A compatibility matrix must distinguish **implemented**, **partially implemented**, **unsupported**, and **not yet tested**. Do not advertise a plugin as compatible until its real behavior has passed the relevant tests.
