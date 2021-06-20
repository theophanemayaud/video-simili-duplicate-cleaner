# Changelog

All notable changes to this project will be documented in this file. See [standard-version](https://github.com/conventional-changelog/standard-version) for commit guidelines.

## [1.4.0](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/compare/v1.3.0...v1.4.0) (2021-06-20)


### Features

* **cache:** can now clear or disable cache, cleaned up but now only identified by path name ([90c2105](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/90c2105d2670187d037ffa778d86e84b735d9961))
* **help:** added contact and about links to website ([f182a58](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/f182a58e101357863aad61fb30f83168d2b52e12))
* **windows:** now builds successfully on windows with ffmpeg and opencv. Still icon and tests to do ([30a4b28](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/30a4b28c32aa93ebf02314e0888dff45b94c43dc))


### Bug Fixes

* **db:** uuid is now properly generated, move from date had not been done well ([10f8882](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/10f8882232347a7483369799b4a882fb8f0fad38))
* **qt-bug:** multi thread datetime objects caused trouble because of QT bug, using Uuid in db ([632b123](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/632b123f8989d06775f0348399a4297da65d06dd))
* **video:** fix a memory leak while capturing frames, which would take ram useage through the roof ([f1e15f5](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/f1e15f59fe971bfab405e94bb672db07ba16d762))

## [1.3.0](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/compare/v1.2.0...v1.3.0) (2021-06-04)


### Features

* **dependencies:** image captures are now done with library ffmpeg only: much (approx 2x) faster ([2382162](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/2382162541a4afe26109030203cdc742ba90d3cd))

## [1.2.0](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/compare/v1.1.0...v1.2.0) (2021-05-22)


### Features

* **comparison:** added new ui for locked and important folders ([57dd50c](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/57dd50c6a06dddf0482f1dd6436b34e356a644ec))
* **comparison:** locked folders now working, must implement other features ([80a28bb](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/80a28bb03d2897336079f728d3c1813894f627d1))
* **comparison:** working on folder settings ui for comparison ([da90ee9](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/da90ee91c5f3c343a34283898be2b43ce1d6a542))
* **extensions:** added .dv extensions as compatible ([1325fb3](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/1325fb3aa441d1df171336235f80763044a163cc))

## [1.1.0](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/compare/v1.0.2...v1.1.0) (2021-04-21)

### [1.0.2](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/compare/v1.0.1...v1.0.2) (2021-04-21)


### Bug Fixes

* **comparison:** fixed crashes due to ffmpeg library file opening errors ([3c702ef](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/3c702effca02d95e0bc040c8964f77482660fe30))

### [1.0.1](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/compare/v1.0.0...v1.0.1) (2021-03-23)

## [1.0.0](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/compare/v0.4.0...v1.0.0) (2021-03-21)

## [0.4.0](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/compare/v0.3.0...v0.4.0) (2021-02-22)


### Features

* **comparison:** when auto trash is stopped it will stay at current video ([5b86ee6](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/5b86ee673e2207a7302c93a8fe1628599447e316))

## [0.3.0](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/compare/v0.2.0...v0.3.0) (2021-02-22)


### Features

* **comparison:** added auto trash feature when sizes differ ([c0baace](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/c0baaceabb8fe5bd530aa14ccd304e60039f918e))
* **comparison:** added function to check if file names for matches are contained in one or another ([4076fca](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/4076fcac6300bb62f25093573ce1556f7d11c15f))
* **comparison:** deletion for identical files with contained names will always be longest first ([efa1a1d](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/efa1a1d074cda3752d313692fd43adbc9c431f05))
* **video support:** add specific error message for failed videos ([a0ec760](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/a0ec760db2ef5a626a023d49b39e811cd1c07706))

## [0.2.0](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/compare/v0.1.2...v0.2.0) (2021-02-22)


### Features

* **comparison:** implemented auto deletion option for identical files ([83dd3d5](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/83dd3d581d3793735e5e0e6515c0ac56c99ffe69))

### [0.1.2](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/compare/v0.1.1...v0.1.2) (2021-02-22)


### Features

* **comparison:** audio&video codec now shown orange if equal ([df3188e](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/df3188ecf85fcb15a0664bfd94b7a2285bc11877))
* print number of added videos as they are being added ([f7df736](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/f7df736d003ad49dbe473710131506726cfe3e05))

### [0.1.1](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/compare/v0.1.0...v0.1.1) (2021-02-22)


### Features

* minor message change to trash not delete ([ae5669f](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/ae5669fec831d0212f7a9f5711cb26d353301367))
* **comparison:** videos are now sorted by size before comparison ([9ea630b](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/9ea630b02d1f354684f1fb4c87e5c26e0b199749))


### Bug Fixes

* fix file size sorting, add verbose checkbox and set default open folder ([6dff757](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/6dff75739f332567170f89b65af590b650cc6a01))
