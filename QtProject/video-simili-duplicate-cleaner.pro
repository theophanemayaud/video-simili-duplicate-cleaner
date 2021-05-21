TEMPLATE = subdirs

SUBDIRS = app tests

#How to compile this program:
    #Qt5.xx (https://www.qt.io/) MingW-32 is the default compiler and was used for the development of the program
    #If compilation fails, click on the computer icon in lower left corner of Qt Creator and select a kit

    #OpenCV 3.xx (32 bit) (https://www.opencv.org/)
    #Compiling OpenCV with MingW can be hard, so download binaries from https://github.com/huihut/OpenCV-MinGW-Build
    #put OpenCV \bin folder in source folder (only libopencv_core and libopencv_imgproc dll files are needed)
    #put OpenCV \opencv2 folder in source folder (contains the header files)
    #add path to \bin folder: Projects -> Build Environment -> Details -> Path -> C:\the_full_Qt_path\program\bin
    #The program will crash on start if the path to \bin was not set or the OpenCV DLL files are not in \bin

    #FFmpeg 4.xx (https://ffmpeg.org/)
    #ffmpeg.exe must be in same folder where the program executable is generated (or any folder in %PATH%)

    #extensions.ini must be in folder where the program executable is generated
