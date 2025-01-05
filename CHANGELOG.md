# Changelog

All notable changes to this project will be documented in this file. See [standard-version](https://github.com/conventional-changelog/standard-version) for commit guidelines.

## [1.12.0](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/compare/v1.11.1...v1.12.0) (2025-01-05)


### Features

* **video:** added av1 video support through aom library in ffmpeg ([873859c](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/873859cf54cf9d44bf351ba5c3918936aeba64f6))
* **video:** warn about videos taking a long time to process ([14f7d47](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/14f7d478741081aeda47b819f6b0a427329e90ce))


### Bug Fixes

* **prefs:** actually reset settings as they wouldn't all reset because of static variables ([a9e6cf5](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/a9e6cf58e91b615c3e4c205578e959c1daed7099))
* **videos:** attempt to cancel video processing for more than 30s to avoid stuck state ([6b3ca7b](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/6b3ca7b970bda7575d449aee144e46bcca56d60a))

### [1.11.1](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/compare/v1.11.0...v1.11.1) (2024-12-25)


### Features

* **comparison:** locked folders are now saved and reloaded even after app is closed ([12604c2](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/12604c28fef952d703821f0086c07dc79149529f))
* **locked-folders:** can now use drag and drop of folders to locked folders list ([5ea335a](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/5ea335a0b144d43dc97ff229de5f85242e2cda33))
* **locked-folders:** last opened location for selecting locked folders is now reused each time ([19ec825](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/19ec8250ab707412343247b5906e688767bcde52))
* **logs:** message now raised when db is not accessible ([8506ffd](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/8506ffdecb0bc851970b5dd7ca20642031fa609a))
* **logs:** verbose mode will now also log when starting to process a video, not only when finished ([767d1f3](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/767d1f3671496b70d0138e6d1137c419de1e3a30))
* **preferences:** the chosen scan folders are now remembered between app restarts ([474ef41](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/474ef41462fbda8a94553cd16b9ebc4646daac12))
* **settings:** comparison mode now saved between restarts ([3c4de00](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/3c4de00f9a4efeba6cdf6f5a51340f14bcd0169f))
* **settings:** comparison threshold now saved between restarts ([c61403d](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/c61403d09869968526d05e4d4c0c8c60009a7425))
* **settings:** thumbnails mode now saved between app restarts ([615e6d8](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/615e6d84de975b4319b37dc6aa4ac00bb5067b6c))
* **settings:** use/don't use/use only cache option now saved between app restarts ([b211ba2](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/b211ba26613f2148262335cc8e3947ac3ab07dd2))


### Bug Fixes

* **comparison:** removed parallel reporting of estimated pairs as it was colliding with comp window ([163a58f](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/163a58f35e2ffc739d3b6fc8ae739cfd41c41b8b))

## [1.11.0](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/compare/v1.10.1...v1.11.0) (2024-10-27)


### Features

* **auto-remove:** when verbose is enabled, auto remove modes will also log the kept files ([1a5024d](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/1a5024d7bdb30e72e505f094bcf35b7c19cdee8b))
* **cache:** cache location now saved even when app is restarted ([371a1f9](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/371a1f94112fd2e39d2597d0e69521d41f93d3b9))
* **folders:** when opening folder explorer, last location will now be reused instead of home/videos ([85ed1b1](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/85ed1b1da6e71399c970eae68b621f38bb4b1a9d))
* **settings:** verbose is now a saved setting across restarts ([9cba0bc](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/9cba0bc4842e74867bc8f7f6368ae20e41a3e5cf))
* **verbose:** verbose mode will now also log to files for better crash trace ([11aa13f](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/11aa13f9a35f462abfe6590a05a2d0f2c774c732))


### Bug Fixes

* **ui:** changed file link color to support both light and dark mode ([e83c96e](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/e83c96eed8aac68728093ece21f9e8faf73190ba))

## 1.10.0 (2024-01-05)


### Features

* **speed:** compiled as native macOS arm architecture for approx 30% speed gains while keeping same performance on older intel macs ([c8f4722](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/c8f4722e83f0823c5dee569274c71c58fb8468d2))
* **cache:** can now declare pair as not duplicates and save to cache ([a7d4749](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/a7d4749518b9ad43018437aed00607ed075faf41))
* **ui:** improved ui as per [#105](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/issues/105) and fixed minor glitch ([b040739](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/b040739282c1c7c07e9462cf04a10a38c46822fc))


### Bug Fixes

* **comparison:** checked for nulls to try prevent two crash types in bothVideosMatch & rejectVideo ([d0a3f6a](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/d0a3f6adbbd10da3e03877b24e7b37e28cab8c09))

### [1.9.1](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/compare/v1.8.1...v1.9.1) (2023-03-19)


### Features

* **ui:** improved ui as per [#105](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/issues/105) and fixed minor glitch ([d0282f3](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/d0282f31ce37e818527daa04a93f8684f8f396dc))


### Bug Fixes

* **settings:** canceling while setting custom cache now actually cancels ([791b24c](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/791b24cf67123061798a1793059a93f93cf492c5))

## [1.9.0](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/compare/v1.8.1...v1.9.0) (2023-01-29)


### Features

* **trash:** add setting to delete files directly ([22f366c](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/22f366c94b4aec91302c38f3f1a9fb8d5ba47c43))

### [1.8.1](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/compare/v1.8.0...v1.8.1) (2022-10-25)


### Bug Fixes

* **comparison:** cache only option will now work on windows ([39093f8](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/39093f8826b08a2ac2d2d1a57d11ab6a89b8d735))

## [1.8.0](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/compare/v1.7.0...v1.8.0) (2022-10-24)


### Features

* **comparison:** new auto comparison mode for videos where only created or modified dates differ ([2a5fd0e](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/2a5fd0e351d6cce0795c5a3e05f7755c830c618a))
* **comparison:** new option to only use cached videos for faster scans if previously cached ([d922af2](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/d922af2db584195f8e4a05295b72e3361c61e186))
* **comparison:** only compare videos with names contained in another now for manual comparison ([e0c4f06](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/e0c4f0696fa1f19865831673ca02e7769d2f72b2))
* **comparison:** right/left arrows now shortcuts for delete right/left, up/down for next/prev ([258c29c](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/258c29c971e75bea7307611a354429ca1e8fc5f4))
* **comparison:** videos of bitrates of less than 5kb/s difference will now be considered equal ([e7e90c6](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/e7e90c65421f6029ec66219847f79e02f135e646))


### Bug Fixes

* **applephotos:** apple photos button will now open Pictures as it's macOS default .photoslibrary ([1194a53](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/1194a533a6a3e44d002aa9f51b7b688934bf9c5d))
* **applephotos:** should now skip videos that are Apple Photo derivative files - closes [#52](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/issues/52) ([cabd388](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/cabd388f2119bf622f511cb4eb915a8ad107927d))
* **cache:** default cache folder wouldn't work because of folder not existing closes [#69](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/issues/69) ([a1991d2](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/a1991d25ea1275f16177e972cb3ce2f5440250ae))
* **cache:** will now cache even if apostrophe in file name or path ([fc83504](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/fc83504705ee0c5269dd8255eba9d3ac519dc2e1))
* **comparison:** bitrates within 1kB/s are now considered equal, closes [#84](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/issues/84) ([d3a0b4d](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/d3a0b4d1cf75619b23e815c5280872a8fee6c759))
* **comparison:** fixed auto delete same except date, that would skip two videos ([316c609](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/316c609d610725c8233e57eb2407c50e723c9fe8))
* **comparison:** videos with very small size (100kB) difference are also considered in auto mode ([56b5a59](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/56b5a59049937441495fb403dd75bb1735799e47))
* **livephotos:** live photos should now be skipped upon scan - closes [#58](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/issues/58) ([fa6b5d0](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/fa6b5d01c4383ae42e4b117f4219f2490b00467a))

## [1.7.0](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/compare/v1.6.0...v1.7.0) (2022-04-11)


### Features

* **cache:** user can now select custom cache location and name ([e361b5d](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/e361b5d21141db7ac9d93b9a40ebd94e499b37be))


### Bug Fixes

* **duplicates:** fixed regression caused by update QT from 5 to 6 ([b6a62ff](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/b6a62ff2e8e7b2c0138c31ac6de9672e519a738b))
* **windows:** fixed microsoft store preventing default cache location ([00f9496](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/00f9496be56a6f1efcba739c0dee2035247b28a6))

## [1.6.0](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/compare/v1.5.1...v1.6.0) (2022-01-03)


### Features

* **apple photos:** enable selection of apple photos library with dedicated button ([89c0851](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/89c0851dd7d426d3ae58fe6382ff43e107dfe00f))
* **apple photos:** name of video inside apple photos is displayed in side by side ([85cc0ca](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/85cc0ca735b57edbf7d4bf7a09c4a1acbd29dac6))
* **apple photos:** reveal and play in apple photos instead of file manager if in Photos library ([ce6f579](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/ce6f579c30650c43f0c8b7d408280d769a1a8605))
* **apple photos:** trash if in Apple Photos will now move to specific album ([4785581](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/4785581f7a80bc1e87f460d129714a6a53ab57ad))

### [1.5.1](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/compare/v1.5.0...v1.5.1) (2022-01-01)


### Features

* **comparison:** will now display file create date ([a9093d3](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/a9093d3c985661b8a9162d2584a1d85214c915b6))


### Bug Fixes

* **move video:** fixed [#50](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/issues/50) moving now blocked in Apple Photos. Also confirmation is asked if locked ([c7593e1](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/c7593e10d486df4eedfc1a21fa0eb868187c1e66))

## [1.5.0](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/compare/v1.4.0...v1.5.0) (2021-06-23)


### Features

* **trash:** can now select custom folder into which duplicates should be moved, instead of trash ([0c2f1d1](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/0c2f1d1132e881c711eaabfdb7eba39031e4e6e0))


### Bug Fixes

* **macos:** will now block deletion of videos that are in an apple photos library. closes [#10](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/issues/10) ([0de825d](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/0de825d0ff88f94563007d5fd7d77f0038623174))
* **video:** solved bug for some video files that ffmpeg doesn't find an average fps for (div/0) ([d1b2c5f](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/commit/d1b2c5f65f8db94123d3390fcffe99e2aea0c20b))

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
