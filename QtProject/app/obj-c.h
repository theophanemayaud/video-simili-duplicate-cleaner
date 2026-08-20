#ifndef OBJCHEADER_H
#define OBJCHEADER_H

#define OBJ_C_SUCCESS_STRING "VidSimiliSuccess" // Arbitrary success string that must be checked by caller
#define OBJ_C_FAILURE_STRING "VidSimiliFailure" // Arbitrary failure string that must be checked by caller

class Obj_C
{
  public:
    /* *
         * function:
         * return : string OBJ_C_SUCCESS_STRING if success, or the error if error
        * */
    static char*
    obj_C_addMediaToAlbum(char* albumName,
                          char* mediaId); //We define a static method to call the function directly using the class_name

    /* *
         * function:
         * return :  string OBJ_C_SUCCESS_STRING if success, or the error if error
        * */
    static char* obj_C_revealMediaInPhotosApp(char* mediaId);
};

#endif // OBJCHEADER_H
