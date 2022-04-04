# Changelog

All notable changes to this project will be documented in this file.

The format mostly follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## UNRELEASED

### Added

* Support for lossless cutting of MPEG-1 Audio Layer II (MP2) files, this is a
  special case of the MP3 support, and also needs `mpg123` as library dependency


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
