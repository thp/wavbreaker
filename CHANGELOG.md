# Changelog

All notable changes to this project will be documented in this file.

The format mostly follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [Unreleased]

### Changed

* Added `libcue` library dependency
* Upgraded Snap base to core24
* Upgraded Flatpak SDK to 23.08

### Fixed

* Importing of track breaks via CUE file now works with any well formatted CUE file
* mpg123-based MP3 playback live preview fix (mono vs stereo channel count bug)

## [0.16] -- 2022-12-20

### Added

* Support for reading Ogg Vorbis audio files using `libvorbisfile` (writing is not
  implemented yet; save track breaks to a .txt file and use `track-break-to-ffmpeg.py`
  to break up Ogg files until Ogg file writing is implemented)
* `wavcli`: New command-line interface to some features of wavbreaker
* `wavcli list` to list wavbreaker's parsing of track break files (TXT/CUE/TOC)
* `wavcli version` to show version and compiled-in file format support
* `wavcli analyze` to open an audio file, run analysis, and preview audio
* `wavcli split` to split an audio file to a folder using a track break list

### Changed

* Format modules: WAV, CDDA RAW, MP2/MP3 and Ogg Vorbis are now handled as separate
  modules for opening, reading, decoding (for playback) and lossless cutting
* File detection: CDDA RAW and MP2/MP3 files must have `cdda.raw`, `.mp2` and `.mp3`
  file extensions to be detected properly (avoids file misdetection)
* `wavinfo` uses the new format modules for MP2/MP3 and Ogg Vorbis support; the
  output format is slightly changed to show duration, number of samples and the
  uncompressed audio format
* The command-line utilities `wavinfo`, `wavmerge` and `wavgen` have been merged
  into the `wavcli` command-line utility. Instead of `wavinfo`, use `wavcli info`,
  instead of `wavmerge`, use `wavcli merge`, instead of `wavgen` use `wavcli gen`.
* The split progress dialog now has a "Cancel" button that cancels the operation
* The two waveforms in the main window now have a minimum height set, and resizing
  the views should be more intuitive due to changed expand rules
* Activating the "add track break" button during playback will insert the break at
  the playback cursor position (contributes to #12)

### Removed

* Removed support for little-endian RAW audio (CDDA); use e.g. `sox` to
  convert the raw audio to a file with WAV header and load that instead
* Removed interactive query for opening as RAW CDDA audio (use `.cdda.raw` instead)

### Fixed

* Duration of initial break point is shown properly when the file is first loaded
* Toggling of etree.org file naming scheme in preferences window works properly now

## [0.15] -- 2022-04-04

### Added

* Support for lossless cutting of MPEG-1 Audio Layer II (MP2) files, this is a
  special case of the MP3 support, and also needs `mpg123` as library dependency
* Add [AppStream metadata](https://freedesktop.org/software/appstream/docs/)


## [0.14] -- 2022-03-17

### Added

* `contrib/track-break-to-ffmpeg.py`: Script to convert an exported track break
  file to a shell script using ffmpeg
* `CHANGELOG.md`: Added

### Changed

* Migrated CI from Travis to Github Actions
* Improved .desktop file for Linux
 * Add `%F` to `Exec=` line (patch by Sebastian Ramacher, from Debian)
 * Add French translation (PR #11, by Olivier Humbert)
 * Add MP3 mime type, fix categories
* Snapcraft: Reduce snap size by not shipping system libraries
* Flatpak: Upgrade to Freedesktop SDK 20.08


[0.14]: https://github.com/thp/wavbreaker/compare/0.13..0.14
[0.15]: https://github.com/thp/wavbreaker/compare/0.14..0.15
[0.16]: https://github.com/thp/wavbreaker/compare/0.15..0.16
