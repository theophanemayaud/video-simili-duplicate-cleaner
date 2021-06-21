[![Bountysource](https://www.bountysource.com/badge/team?team_id=599354&style=bounties_posted)](https://www.bountysource.com/teams/video-simili-duplicate-cleaner/issues)
# Video simili duplicate cleaner

Find and remove duplicates : *Video simili duplicate cleaner* is a program that can find duplicate or simply similar video files.
Video simili duplicate cleaner compares the actual video content regardless of different format or compression used, while other software only find identical files (digital video fingerprinting).

[![Download from the Apple App Store](https://developer.apple.com/app-store/marketing/guidelines/images/badge-download-on-the-mac-app-store.svg)](https://apps.apple.com/us/app/video-simili-duplicate-cleaner/id1557271308)

This app is [made available via the App Store with a small cost](https://apps.apple.com/us/app/video-simili-duplicate-cleaner/id1557271308), in order to support further development work. You are of course free to check out the source code on GitHub, report bugs, suggest new features, and contribute code ! But please support my work via the App Store purchase, and do not redistribute the binary, although of course you may use the code for you own projects, no problemo !

You can also support the project, or specific issues, via [BOUNTYSOURCE](https://www.bountysource.com/teams/video-simili-duplicate-cleaner/issues). You can make donations for the project, or post a bounty as a "reward when a specific issue is completed". This helps identify which issues are important and should be done first.


## Contact

Want to report a bug 🐞, suggest new ideas 💡, give general feedback 🧐 or have some specific question 🙋 ? That's what our [DISCUSSIONS PAGE](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/discussions) is for 🚀 [Issues](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/issues) can also be used for bugs and feature suggestions, but it's often good to first chat about them in the discussions 💬


## Examples

In the samples folder, you will find two videos with different sizes and compression levels, which result in the following two windows of the app :

[![Demo video](samples/Demo.mp4 "Demo video")](https://theophanemayaud.github.io/video-simili-duplicate-cleaner/samples/Demo.mp4)

![Main window](samples/MainWindow.png "Main window")

![Comparison window](samples/ComparisonWindow.png "Comparison window")

![Comparison window auto options](samples/ComparisonWindowAutoOptions.png "Comparison window auto options")

![Main window results](samples/MainWindowResults.png "Main window results")

## Features

 - Simple, easy to use graphical user interface for comparing videos.
 - Supports all widely used video formats.
 - Multithreaded operation, makes full use of all available CPU threads.
 - Employs two powerful image comparison methods: pHash (perceptual hashing) and SSIM (structural similarity).
 - Cross-platform and open source development: source code available for compiling on Windows/Linux/macOS.

## Changelog

See [CHANGELOG.md](CHANGELOG.md) file.

## Roadmap

To see what I want to implement next, see [ROADMAP.md](ROADMAP.md). These things may or may not happen, in no guaranteed time frame. You can suggest ideas on [the project's github issues page](https://github.com/theophanemayaud/video-simili-duplicate-cleaner/issues).

## Usage
 
After starting the program you must enter which folders to scan for video files. Folders can be added by typing them in, dragging and dropping a folder onto the window or using the folder browser button next to the input box.
All folders must be separated by a semicolon ( ; ).

Comparison is started by pressing the "Find duplicates" button and all video files in selected folders are scanned.
A lengthy search for videos can be aborted by pressing the button again (that now reads Stop).
Note: some videos may be too broken for the program to read and will be rejected.

## Settings

The default settings have been chosen to get best results with a minimum amount of false positives.  
Thumbnails:  
How many image captures are taken from each video. The larger the number of thumbnails, the slower the scanning of video files is.  
After deleting all duplicate videos, some additional matching ones may still be found by scanning again with a different thumbnail size.  
CutEnds compares the beginning and end of videos separately, trying to find matching videos of different length. This is twice as slow.  
pHash:  
A fast and accurate algorithm for finding duplicate videos.  
SSIM:  
Even better at finding matches (less false positives especially, not necessarily more matches). Noticeably slower than pHash.  
SSIM block size: A smaller value means that the thumbnail is analyzed as smaller, separate images. Note: selecting the value 2 will be quite slow.  
Comparison:  
When comparing two videos, a comparison value is generated. If the value is below this threshold, videos are considered a match.  
threshold:  
A threshold that is too low or too high will either display videos that don't match or none at all.  
Raise threshold:  
These two options increase/decrease the selected threshold when two videos have almost same length  
Lower threshold:  
(meaning very likely that they match even if the computer algorithm does not think so).


## Comparison window

If matching videos are found, they will be displayed in a separate window side by side, with the thumbnail on top and file properties on bottom.  
Clicking on the thumbnail will launch the video in the default video player installed.  
Scrolling on the thumbnail with the mouse wheel will load a full size screen capture and zoom it, allowing a visual comparison of image quality,  
Clicking on the filename in blue will open the file manager with the video file selected.  
File properties are colour coded:
 - Tan: both videos have same property
 - Green: "better" property
 - Black: "worse" property (or not used)

Prev and next buttons: cycle backwards and forwards through all matching files.  
Delete: Delete the video.  
Move: Move the video to folder of opposite side.  
Swap filenames: Change filenames between videos.


Beware that a poor quality video can be encoded to seem better than a good quality video.  
Trust your eyes, watch both videos in a video player before deleting.

## Disk cache

Searching for videos the first time using the program will be slow. All screen captures are taken one by one with FFmpeg and are saved in the file
cache.db in the program's folder. When you search for videos again, those screen captures are already taken and the program loads them much faster.
Different thumbnail modes share some of the screen captures, so searching in 3x4 mode will be faster if you have already done so using 2x2 mode.
A cache.db made with an older version of the program is not guaranteed to to be compatible with newer versions.


## Platform speecific notes

### MacOS

On MacOS, the Apple Photos app stores its photos within a container, which has the .photoslibrary extension, but it is actually a folder. The app can't delete videos from within that folder, as it would break Apple Photos. But it will scan for the videos and show you the dupplicates, you just won't be able to trash them. The file names of the videos in the Apple Photos Library will however not be meaningfull, as Apple Photos stores them with a random generated name. If you need to delete videos that were reported from within the Apple Photos Library, you must manually look for them inside the Apple Photos app and delete them from there.

### Libraries/Dependencies

ffmpeg and all other libraries are packaged all within the app file. You can change ffmpeg executable by right clicking on the app, then "Show Package Contents", then navigate to where ffmpeg is located and replace it.

See [DEPENDENCIES.md](DEPENDENCIES.md) for more details.


## Privacy Policy

This app does not collect any personnal information, in fact no information is sent anywhere.


## Credits

See [CREDITS.md](CREDITS.md) file for more information.
