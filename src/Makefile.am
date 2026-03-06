bin_PROGRAMS = disgrace

disgrace_SOURCES = \
    main.cpp \
    core/engine.cpp \
    core/engine_save.cpp \
    core/key_bindings.cpp \
    core/masterbus.cpp \
    core/metronome.cpp \
    core/transport.cpp \
    core/undo_stack.cpp \
    audio/jack_backend.cpp \
    audio/resampler.cpp \
    audio/sample_buffer.cpp \
    audio/sample_voice.cpp \
    audio/timestretch.cpp \
    dsp/dsp_chain.cpp \
    gui/main_window.cpp \
    gui/project_panel.cpp \
    gui/tab_project.cpp \
    gui/tab_tracker.cpp \
    gui/tab_instrument.cpp \
    gui/tab_mixer.cpp \
    gui/tab_settings.cpp \
    gui/tab_help.cpp \
    gui/tracker_panel.cpp \
    gui/instrument_panel.cpp \
    gui/mixer_panel.cpp \
    gui/settings_panel.cpp \
    gui/vu_meter.cpp \
    gui/detached_window.cpp \
    gui/model/mixer_model.cpp \
    gui/tracker_view.cpp \
    gui/transport_panel.cpp \
    gui/transportbar.cpp \
    gui/waveform_view.cpp \
    gui/spectral_view.cpp \
    instrument/instrument.cpp \
    instrument/instrument_rack.cpp \
    instrument/sample_instrument.cpp \
    instrument/soundfont_instrument.cpp \
    instrument/dssi_instrument.cpp \
    instrument/lv2_instrument.cpp \
    instrument/midi_instrument.cpp \
    io/audio_file.cpp \
    io/project_archive.cpp \
    io/song_serializer.cpp \
    midi/midi_input.cpp \
    mixer/track.cpp \
    mixer/mixer_bus.cpp \
    sequencer/automation.cpp \
    sequencer/pattern.cpp \
    sequencer/sequencer.cpp \
    sequencer/timing.cpp \
    analysis/fft_analyzer.cpp

disgrace_CXXFLAGS = \
    $(JACK_CFLAGS) \
    $(FLTK_CFLAGS) \
    $(SNDFILE_CFLAGS) \
    $(SAMPLERATE_CFLAGS) \
    $(SNDTOUCH_CFLAGS) \
    $(NLOHMANN_JSON_CFLAGS) \
    $(FFTW3_CFLAGS) \
    $(LIBARCHIVE_CFLAGS) \
    $(FLUIDSYNTH_CFLAGS)

disgrace_LDADD = \
    $(JACK_LIBS) \
    $(FLTK_LIBS) \
    $(SNDFILE_LIBS) \
    $(SAMPLERATE_LIBS) \
    $(SNDTOUCH_LIBS) \
    $(FFTW3_LIBS) \
    $(LIBARCHIVE_LIBS) \
    $(FLUIDSYNTH_LIBS)
