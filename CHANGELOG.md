# Changelog

All notable changes to this project will be documented in this file. See [standard-version](https://github.com/conventional-changelog/standard-version) for commit guidelines.

## [0.3.0](https://github.com/theophanemayaud/similar-and-duplicate-videos-finder/compare/v0.2.0...v0.3.0) (2021-02-19)


### Features

* **comparison:** added auto trash feature when sizes differ ([2d75323](https://github.com/theophanemayaud/similar-and-duplicate-videos-finder/commit/2d75323b6a494977dfb7066ff41cd8a6543b7b6e))
* **comparison:** added function to check if file names for matches are contained in one or another ([603b6df](https://github.com/theophanemayaud/similar-and-duplicate-videos-finder/commit/603b6df0fb76dfe15275c4d396fc399af0701bf2))
* **comparison:** deletion for identical files with contained names will always be longest first ([3eb4181](https://github.com/theophanemayaud/similar-and-duplicate-videos-finder/commit/3eb418148f56526a0f03f8b18cb795debbc5fbc4))
* **video support:** add specific error message for failed videos ([213e2b2](https://github.com/theophanemayaud/similar-and-duplicate-videos-finder/commit/213e2b22c2e0bf5485ae2dbaed4649ed6e7ec46d))

## [0.2.0](https://github.com/theophanemayaud/similar-and-duplicate-videos-finder/compare/v0.1.2...v0.2.0) (2021-02-16)


### Features

* **comparison:** implemented auto deletion option for identical files ([eacb699](https://github.com/theophanemayaud/similar-and-duplicate-videos-finder/commit/eacb699a1b93c97d957bf756aaaa140b864c085b))

### [0.1.2](https://github.com/theophanemayaud/similar-and-duplicate-videos-finder/compare/v0.1.1...v0.1.2) (2021-01-26)


### Features

* **comparison:** audio&video codec now shown orange if equal ([ee38ead](https://github.com/theophanemayaud/similar-and-duplicate-videos-finder/commit/ee38eadb2e818cec08c7b610cdfb800c3d320b5a))

# Previous versions :
## Similar and duplicate videos finder 0.1.0 : branch and release for mac from fork of windows
- FEATURE : add option to disable delete confirmations (when needing to delete a lot of files, it's one less step !).
- FEATURE : moves files to trash instead of deleting them outright.


### Vidupe 1.211 (released 2019-09-18) changelog:
 - Fix crash when scrolling mouse wheel outside program window

### Vidupe 1.21 (released 2019-09-13) changelog:
 - New thumbnail mode, CutEnds, for finding videos with modified beginning or end. This is default now.
 - Faster comparison
 - Threads won't hang on videos timing out as long as before
 - Tiny amount of videos wrongly marked as broken

### Vidupe 1.2 (released 2019-05-27) changelog:
 - Disk cache for screen captures, >10x faster loading once cached
 - Much faster comparison. Comparison window opens faster
 - Threshold modifiers for same/different video durations now work
 - More accurate interpolation
 - Much old C-style code rewritten

### Vidupe 1.1 (released 2019-05-05) changelog:
 - Partial disk cache (video metadata only)
 - Faster loading of videos
 - Removed delay between videos after move/delete
 - Videos kept in memory if comparison window accidentally closed
 - Improved zoom
