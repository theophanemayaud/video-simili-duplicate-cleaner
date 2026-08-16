#import "obj-c.h"
#import <Foundation/Foundation.h>

char* Obj_C::obj_C_addMediaToAlbum(char* albumName, char* mediaId)
{
    NSString* objAlbumName = [NSString stringWithUTF8String:albumName];
    NSString* mediaIdS = [NSString stringWithUTF8String:mediaId];

    NSString* source = [NSString stringWithFormat:@"tell application \"Photos\"\n"
                                                  @"    set selMedia to (get media items whose id contains \"%@\")\n"
                                                  @"    if not (album \"Trash from %@\" exists) then\n"
                                                  @"        make new album named \"Trash from %@\"\n"
                                                  @"    end if\n"
                                                  @"    add selMedia to album \"Trash from %@\"\n"
                                                  @"end tell",
                                                  mediaIdS, objAlbumName, objAlbumName, objAlbumName];

    NSDictionary* errorDictionary;
    NSAppleScript* script = [[NSAppleScript alloc] initWithSource:source];

    NSAppleEventDescriptor* resultDesc = [script executeAndReturnError:&errorDictionary];

    NSString* returnString = @OBJ_C_SUCCESS_STRING;
    if (resultDesc) { // was successful
        return (char*)[returnString UTF8String];
    }
    else {
        returnString = [NSString stringWithFormat:@"%@", errorDictionary];
        return (char*)[returnString UTF8String];
    }
}

std::string Obj_C::obj_C_getMediaName(const std::string& mediaId)
{
    // This can run on QtConcurrent's worker thread, which has no application
    // autorelease pool. Copy the value before this scoped pool drains so C++
    // never receives a borrowed NSString buffer.
    @autoreleasepool {
        NSString* mediaIdS = [NSString stringWithUTF8String:mediaId.c_str()];

        NSString* source = [NSString stringWithFormat:@"tell application \"Photos\"\n"
                                                      @"    set selMedia to (get media items whose id contains \"%@\")\n"
                                                      @"    return filename of item 1 of selMedia\n"
                                                      @"end tell",
                                                      mediaIdS];

        NSDictionary* errorDictionary;
        NSAppleScript* script = [[NSAppleScript alloc] initWithSource:source];
        NSAppleEventDescriptor* resultDesc = [script executeAndReturnError:&errorDictionary];

        std::string result = OBJ_C_FAILURE_STRING;
        if (resultDesc) {
            const char* name = resultDesc.stringValue.UTF8String;
            result = name == nullptr ? std::string() : std::string(name);
        }

        // This project is not ARC-managed. Release the alloc/init script only
        // after copying its result into C++ storage.
        [script release];
        return result;
    }
}

char* Obj_C::obj_C_revealMediaInPhotosApp(char* mediaId)
{
    NSString* mediaIdS = [NSString stringWithUTF8String:mediaId];

    NSString* source = [NSString stringWithFormat:@"tell application \"Photos\"\n"
                                                  @"    set selMedia to (get media items whose id contains \"%@\")\n"
                                                  @"    spotlight item 1 of selMedia\n"
                                                  @"    activate\n"
                                                  @"end tell",
                                                  mediaIdS];

    NSDictionary* errorDictionary;
    NSAppleScript* script = [[NSAppleScript alloc] initWithSource:source];

    NSAppleEventDescriptor* resultDesc = [script executeAndReturnError:&errorDictionary];

    NSString* returnString = @OBJ_C_SUCCESS_STRING;
    if (resultDesc) { // was successful
        return (char*)[returnString UTF8String];
    }
    else { // was an error
        returnString = [NSString stringWithFormat:@"%@", errorDictionary];
        return (char*)[returnString UTF8String];
    }
}
