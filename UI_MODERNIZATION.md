# UI modernization

## Purpose

This document describes the UI modernization work for the FreeDV desktop application. The goal is to improve workspace flexibility, appearance, and signal-display readability while preserving modem, audio, radio-control, reporting, and other operational behavior as closely as practical.

The implementation adds an Independent Windows workspace alongside the existing Notebook workspace and introduces a shared visual system for Light and Dark appearance, typography, signal displays, gauges, and selected controls.

## Changes at a glance

- Added an Independent Windows workspace using the existing six FreeDV signal displays and operational controls rather than duplicated implementations.
- Added persistent Independent Control and display geometry, visibility, workspace selection, multi-monitor recovery, and transient Frm Mic presentation during transmit and Voice Keyer operation.
- Added window snapping for the Independent workspace while retaining normal platform window movement and resizing.
- Added native Light and Dark appearance selection with persisted startup preference and a session-only command-line Dark override.
- Replaced the SNR and Level indicators with platform-neutral gauges while preserving their existing operational ranges and semantics.
- Unified Waterfall, Spectrum, waveform, SNR plot, and gauge presentation under the existing Multicolor, Black & White, and Blue Tint Signal Display Style preference.
- Improved signal visualization with magnitude-aware waveform coloring, stronger waveform edge definition, 2-pixel Spectrum and SNR traces, translucent plot fills, and a theme-accent frame around signal plot areas.
- Added a restrained typography hierarchy using native platform fonts.
- Gave the five primary Control buttons additional visual weight while retaining native rendering and existing operational state colors.
- Added Local and UTC clocks to the main controls.
- Improved Stats field spacing and FreeDV Reporter column-header emphasis without changing their operational behavior.
- Added compatibility guards and helpers for supported wxWidgets versions, including wxWidgets 3.0 DPI handling and version-dependent appearance and Reporter styling.

## Design principles

- Preserve existing operational behavior and event handlers wherever practical.
- Prefer presentation-layer changes over functional changes.
- Reuse operational controls rather than maintaining synchronized copies.
- Prefer standard cross-platform wxWidgets behavior and public APIs.
- Isolate platform-specific handling where necessary for equivalent behavior or appearance.
- Preserve semantic state indications, including PTT, Voice Keyer, recording, synchronization/status, and Reporter state colors.
- Avoid unrelated refactoring.

## Workspace architecture

### Notebook

The existing Notebook workspace remains the default for a fresh installation. Its six displays are Waterfall, Spectrum, Frm Radio, Frm Mic, Frm Decoder, and SNR.

Notebook retains the existing responsive wxWidgets sizer behavior and tab-based presentation. Switching to Independent Windows does not create replacement plots; the existing display instances are transferred between presentations.

### Independent Windows

`DisplayWorkspace` coordinates the same six plot instances used by Notebook. It transfers them from the notebook into top-level `DisplayFrame` windows and restores them when returning to Notebook.

Each display can be moved, resized, shown, and hidden independently. Closing a detached frame hides it without destroying its plot. The Displays checkboxes in Independent Control reflect user-selected visibility, including changes made by closing a display window.

The workspace can be selected using the checkable Tools > Independent Windows item or the Workspace radio buttons. All selectors use the same transition path and reflect the active workspace. Workspace changes are disabled during transmission/changeover, Voice Keyer operation or recording, and application shutdown.

Independent mode also uses a dedicated **FreeDV Control** presentation for the non-display controls. `TopFrame` moves the existing control-group sizers between Notebook and Independent layouts, retaining the same widgets, handlers, and state rather than creating a second operational implementation.

Conditional controls such as Mode and Squelch retain their existing configuration and mode-dependent behavior. The existing `enableLegacyModes` configuration remains authoritative for the Mode group, Squelch, ReSync, and Center RX. The workspace implementation does not introduce new mode-selection behavior.

### Frm Mic behavior

In Independent mode, an operational Frm Mic request temporarily shows the existing Frm Mic frame if the user has it hidden. Its visibility checkbox remains unchecked, and this temporary presentation is not saved as user-selected visibility. Frm Mic hides again when the operation ends.

If Frm Mic was already user-visible, it remains visible throughout the operation and afterward. Checking Frm Mic while it is temporarily visible promotes it to normal user-selected visibility. Closing it during a temporary presentation suppresses it for the remainder of that operation without preventing it from appearing on a later operation.

Notebook retains its existing Frm Mic selection and return behavior, including split notebook groups.

## Workspace persistence

`FreeDVConfiguration` owns the persisted workspace fields. `MainFrame` captures the outgoing workspace before switching, restores the incoming workspace, and captures the active workspace during configuration export and shutdown.

Configuration reload first returns the displays to Notebook, loads the configuration and existing tab layout, and then restores the configured presentation. Notebook tab-layout persistence retains its existing experimental-feature gate.

| Path | State |
| --- | --- |
| `/Windows/Independent/active` | Last active workspace: false for Notebook, true for Independent. |
| `/Windows/Independent/visibilitySaved` | Distinguishes first use from a saved selection with all displays hidden. |
| `/Windows/Independent/Control/{left,top,width,height}` | Independent Control geometry. |
| `/Windows/Independent/Displays/<id>/{left,top,width,height,visible}` | Detached display geometry and visibility. |

The existing `/MainFrame/{left,top,width,height}` values continue to represent Notebook Main geometry and are not overwritten by Independent Control geometry.

Display identities are `Waterfall`, `Spectrum`, `FrmRadio`, `FrmMic`, `FrmDecoder`, and `SNR`. These correspond to stable `DisplayId` values rather than notebook page order or translated captions. Reporter geometry remains separate.

On first use, Independent Windows shows Waterfall when no visibility state has previously been saved. After visibility has been saved, the exact selection is restored, including a valid selection with all displays hidden.

Window restoration uses wxWidgets monitor work areas and accepts negative multi-monitor coordinates. Positions that retain a usable portion of the title bar on an available work area are preserved. Inaccessible positions are recovered onto an available monitor, and sizes are bounded where practical while respecting layout minimums.

Minimized or maximized windows retain their previously captured ordinary geometry rather than saving an iconized or maximized rectangle. Actual placement reuses `RestoreWindowPosition`, including its GTK initial-placement handling.

## Appearance and visual system

### Light and Dark appearance

The application supports Light and Dark appearance through native wxWidgets appearance handling. The selected appearance is persisted through the existing FreeDV configuration store and is applied at startup.

Appearance selectors are available in both Notebook and Independent Control. Appearance changes take effect on the next launch rather than attempting to restyle existing top-level windows during a session.

A session-only `--dark-mode` command-line option is also available. Version guards are used where appearance APIs differ between supported wxWidgets versions.

### Signal displays

Signal-display colors are centralized in `FreeDVTheme` and follow the existing FreeDV display styles: Multicolor, Black & White, and Blue Tint. Their numeric identities remain compatible with the existing `/Waterfall/Color` setting.

The selected style is shared by Waterfall, Spectrum, Frm Radio, Frm Mic, Frm Decoder, SNR, and the signal gauges. The historic display palette remains available for intensity rendering such as Waterfall, while plot traces use higher-visibility derivatives where needed for readability.

Frm Radio, Frm Mic, and Frm Decoder use symmetric magnitude-based coloring. Waveform plots use a 2-pixel outline derived from the same magnitude-dependent display color. Spectrum and SNR use 2-pixel traces with translucent color-matched fills. Signal plot areas use a thin theme-accent frame.

Spectrum and Waterfall buffers are initialized to the existing minimum-magnitude floor before their first paint so an idle Spectrum begins in the quiet state rather than treating zero-initialized bins as maximum signal.

The SNR and Level indicators use a platform-neutral `LevelGauge`. SNR follows the existing -10 through +35 dB presentation range, while Level retains its existing 0-100 peak-level behavior. These changes do not alter the underlying signal, DSP, or audio-level calculations.

### Typography and primary controls

The main application uses a small typography hierarchy based on the native platform GUI font. Operational group headings use the Emphasized role, and Independent Control uses the Heading role for its primary title. Body controls retain native platform typography.

The five primary Control buttons -- Start/Stop Modem, Analog/Digital, Tune, Voice Keyer, and XMIT -- use the Emphasized typography role and a 36 DIP minimum height. They retain native button rendering and the existing operational colors.

### Time, Stats, and Reporter

A Time group shows Local and UTC time in 24-hour `HH:MM:SS` format and updates once per second. The same control group moves between the Notebook and Independent layouts rather than using separate implementations.

The Stats group retains its existing fields and behavior with a small additional inset from the group border. Some statistics can legitimately show `unk` in modes that do not supply those values, including RADE.

FreeDV Reporter retains its existing table layout, row colors, sorting, filtering, and controls. On wxWidgets 3.1 or newer, its column headings use the Emphasized typography role through `wxDataViewCtrl::SetHeaderAttr()`. wxWidgets 3.0 builds retain the native Reporter header appearance.

## Compatibility and implementation notes

The modernization is implemented primarily in the presentation and workspace layers. Operational modem, DSP, audio, radio-control, and Reporter behavior remains handled by the existing application code.

The main workspace implementation is in:

- `src/gui/displays/DisplayWorkspace.{h,cpp}`
- `src/gui/displays/DisplayFrame.{h,cpp}`
- `src/gui/displays/WindowSnapManager.{h,cpp}`
- `src/gui/util/WindowPositionRestore.{h,cpp}`
- `src/gui/util/DpiUtils.h`
- `src/gui/theme/FreeDVTheme.{h,cpp}`

`MainFrame` and `TopFrame` provide the integration points for workspace transitions, shared control layout, appearance preferences, and operational display requests.

Compatibility helpers and version guards are used for supported wxWidgets versions rather than maintaining separate platform UI implementations. In particular, DPI conversion supports wxWidgets 3.0, native appearance selection is guarded according to API availability, and Reporter header emphasis is omitted where the required DataView API is unavailable.

## Known limitations

- Persistent docking relationships and group movement are not implemented.
- Automatic Independent window arrangement is not implemented.
- A vertical Independent Control orientation is not implemented.
- Physically disconnected-monitor recovery has not been runtime-validated.
- Native Linux runtime behavior, including GTK/Wayland placement, still needs runtime validation.
- Independent Windows operate on macOS, but snapping behavior may need additional platform-specific refinement.
