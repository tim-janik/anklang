% ANKLANG(1)	anklang-0 | Anklang Manual

# NAME
anklang - Music composition and modular synthesis application


# SYNOPSIS
**anklang** [*OPTIONS*] [*FILES*...]


# DESCRIPTION

**Anklang** is a digital audio synthesis application for live creation
and composition of music and other audio material.
It is released as free software under the MPL-2.0.

Anklang comes with synthesis devices which can be arranged in tracks
and controlled via MIDI input devices or pre-programmed clips which
contain MIDI notes.

The Anklang sound engine is a dedicated process which is controlled
by a user interface based on web technologies that can be run in a
special process (like electron) or modern browsers
like **firefox**(1) or **google-chrome**(1).

# OPTIONS

Anklang supports short and long options which start with two dashes ('-').

**--**
:   Stop argument processing, treat remaining arguments as files.

**-h**, **--help**
:   Show a help message about command line usage.

**--fatal-warnings**
:   Abort on warnings and failing assertions, useful for test modes.

**--check**
:   Execute internal tests and benchmarks.

**--disable-randomization**
:   Enable deterministic random numbers for test modes.

**--ui-js=SCRIPT**
:   Run JavaScript after UI startup. Electron runs headless by default and closes
    the window when the script finishes. Call `window.auto_close(false)` in the
    script to keep the window open. On automatic close, a number returned by
    the script becomes the exit status.

**--headless[=BOOL]**
:   Run the browser headless. This is the default for `--ui-test` and `--ui-js`.
    Use `--headless=false` to show the window while running a script.

**-v**, **--version**
:   Print information about the program version.

# SEE ALSO

[**Anklang Website**](http://anklang.testbit.eu){.external}
