# GUI Subsystem

This directory contains the graphical user interface components for `disgrace`, implemented using the **wxWidgets** toolkit. The original FLTK implementation has been fully ported.

## Purpose
The primary responsibilities of this subsystem include:
*   Providing a responsive and native-looking user interface.
*   Implementing the main application window with tabs for different workflows.
*   Presenting data from other subsystems (sequencer, mixer, instrument, project) in an interactive way.

## Components & Status

*   **Main Window (`wx_main_window`):**
    *   Hosts the main tabbed interface (Project, Tracker, Tracks, Notation, Instrument, Mixer, Settings).
    *   Manages the global Transport Bar.
    *   Handles global key bindings.

*   **Transport Bar (`wx_transportbar`):**
    *   Controls: Play, Stop, Record, Loop, Metronome.
    *   Settings: Tempo, LPB, Octave, Step Size.
    *   Feedback: Status, Clock, Master VU Meters.

*   **Project Panel (`wx_project_panel`):**
    *   File Operations: New, Load, Save, Import/Export (WAV).
    *   Track Management: Add/Remove tracks and buses, reorder tracks.
    *   Track Configuration: Name, Instrument, Notation type, Output Bus.

*   **Tracker Panel (`wx_tracker_panel` & `wx_tracker_view`):**
    *   **View:** Displays the pattern grid with syntax coloring. Supports keyboard navigation and editing.
    *   **Panel:** Manages pattern order list (Add, Remove, Copy, +/- index).

*   **Tracks Panel (`wx_tracks_panel`):**
    *   Provides a timeline view of patterns arranged by track.
    *   Supports zooming and scrolling.

*   **Mixer Panel (`wx_mixer_panel`):**
    *   **Channel Strips:** Volume faders, Pan sliders, Mute/Solo, VU Meters for Tracks, Buses, and Master.
    *   **FX Chain:** Add, Remove, Bypass, Move Up/Down effects in the insert chain.
    *   **FX Editor:** Auto-generated UI (sliders, choices) for all DSP effect parameters.

*   **Instrument Panel (`wx_instrument_panel`):**
    *   **Sampler:** Full sample editor with Waveform View (`wx_waveform_view`). Supports Add/Load sample, Record, Play/Stop.
    *   **Sample Processing:** Silence, Normalize, Fade In/Out (Lin/Log), Adjust Gain, Cut/Copy/Paste.
    *   **SoundFont:** Load SF2 files, select presets, adjust volume.
    *   **Plugin:** Scan DSSI/LADSPA plugins, generic parameter editor.
    *   **MIDI:** Channel and Program selection.

*   **Settings Panel (`wx_settings_panel`):**
    *   **Audio/MIDI:** Device selection (channels).
    *   **GUI:** Theme selection (Light/Dark/Classic), Color customization (BG, FG, Waveform).

*   **Notation Panel (`wx_notation_panel`):**
    *   (Placeholder/In Progress) intended for score view.

## Themes
The GUI supports theming via `ThemeManager`. The `SettingsPanel` allows switching between presets and customizing colors.

## Future Work
*   **Notation View:** Full implementation of the score editor.
*   **Plugin UI:** Support for custom plugin UIs (X11/Cocoa embedding) if possible, or refined generic UI.
*   **Key Binding Editor:** Allow users to customize key bindings in the Settings panel.
