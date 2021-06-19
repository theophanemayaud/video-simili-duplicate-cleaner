TEMPLATE = subdirs

SUBDIRS = \
    app \
    tests/test_mainwindow \
    tests/test_video

#How to compile this program:
    # See the dependencies.md file for info about how to build the ffmpeg and opencv libraries
    # If compilation fails, click on the computer icon in lower left corner of Qt Creator and select a kit
