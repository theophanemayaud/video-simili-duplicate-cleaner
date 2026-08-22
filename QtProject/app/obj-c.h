#ifndef OBJCHEADER_H
#define OBJCHEADER_H

#include <string>

#define OBJ_C_SUCCESS_STRING "VidSimiliSuccess" // Arbitrary success string that must be checked by caller
#define OBJ_C_FAILURE_STRING "VidSimiliFailure" // Arbitrary failure string that must be checked by caller

class Obj_C
{
  public:
    // from QT C++, convert char * with QString::fromLocal8Bit(char * stringHere)

    /* *
         * function:
         * return : string OBJ_C_SUCCESS_STRING if success, or the error if error
        * */
    static char*
    obj_C_addMediaToAlbum(char* albumName,
                          char* mediaId); //We define a static method to call the function directly using the class_name

    // Photos stores library videos under opaque IDs, so the on-disk filename is
    // useless to the user. We used to ask Photos for the real name with
    // NSAppleScript / osascript, which was slow on every comparison. PhotoKit
    // looks up one PHAsset by local identifier in a few milliseconds, and we
    // already declare Photos library access for adding media to albums.
    // The owning std::string is safe to pass back across the Objective-C++ boundary.
    static std::string obj_C_getMediaNameFromPhotoKit(const std::string& mediaId);

    /* *
         * function:
         * return :  string OBJ_C_SUCCESS_STRING if success, or the error if error
        * */
    static char* obj_C_revealMediaInPhotosApp(char* mediaId);
};

#endif // OBJCHEADER_H
