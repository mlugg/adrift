# adrift

A simple, lightweight speedrun timer for Linux.

## Building

adrift is built using GNU Make. After installing its dependencies,
simply run `make` in the source tree to build the `adrift` binary. This
binary is standalone and can be installed to an appropriate location.

### Dependencies

adrift depends on [vtk](https://github.com/vktec/vtk) for its GUI.

## Usage

	adrift [directory]

adrift will look for and store all configuaration files, run
information, etc in the given directory, or, if none was given, the
current working directory. All files used by adrift are UTF-8 and use LF
line endings.

When adrift starts, a file named `splits` wil be read, which contains
the split names, each on their own line. Subsplits can be created by
indenting splits with tabs as follows:

	Chapter 1
		Map 1
		Map 2
	Chapter 2
		Map 3
		Map 4

adrift will also execute the file named `splitter`. This should be an
executable file which outputs a rift data stream on stdout for splitting
(see the Autosplitting section below).

## Configuration

When it starts, adrift will attempt to read a file named `config`. Each
line of this file is a string key, followed by any amount of whitespace
(excluding newlines), and a corresponding string value. A keys prefixed
with `col_` configures a color: its value should be a color code in the
form `RRGGBB` or `RRGGBBAA`, optionally with a leading `#` (the alpha
component is assumed to be `FF` if not given). The following
configuration keys currently exist:

- `game`
- `category`
- `col_background`
- `col_text`
- `col_timer`
- `col_timer_ahead`
- `col_timer_behind`
- `col_active_split`
- `col_split_gold`
- `col_split_ahead`
- `col_split_behind`
- `split_time_width`
- `window_width`
- `window_height`

## Autosplitting

Autosplitters are communicated with via the [rift
protocol](https://github.com/vktec/rift/blob/master/protocol.md). Any
rift-compliant autosplitter should work with adrift.

Included in the repo is an autosplitter which interfaces with
[SAR](https://github.com/Blenderiste09/SourceAutoRecord). This splitter
requires ptrace privileges to work, as it must read Portal 2's memory.
These can be given by running the following command as root:

	setcap cap_sys_ptrace=eip ./splitter
