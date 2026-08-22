#import "obj-c.h"
#import <Foundation/Foundation.h>
#import <Photos/Photos.h>

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

std::string Obj_C::obj_C_getMediaNameFromPhotoKit(const std::string& mediaId)
{
    @autoreleasepool {
        __block PHAuthorizationStatus authorizationStatus =
            [PHPhotoLibrary authorizationStatusForAccessLevel:PHAccessLevelReadWrite];
        if (authorizationStatus == PHAuthorizationStatusNotDetermined) {
            dispatch_semaphore_t authorizationFinished = dispatch_semaphore_create(0);
            [PHPhotoLibrary requestAuthorizationForAccessLevel:PHAccessLevelReadWrite
                                                       handler:^(PHAuthorizationStatus status) {
                                                         authorizationStatus = status;
                                                         dispatch_semaphore_signal(authorizationFinished);
                                                       }];
            dispatch_semaphore_wait(authorizationFinished, DISPATCH_TIME_FOREVER);
        }

        if (authorizationStatus != PHAuthorizationStatusAuthorized
            && authorizationStatus != PHAuthorizationStatusLimited)
            return OBJ_C_FAILURE_STRING;

        NSString* mediaIdString = [NSString stringWithUTF8String:mediaId.c_str()];
        if (mediaIdString == nil)
            return OBJ_C_FAILURE_STRING;

        // Files in a Photos library use the UUID portion of PhotoKit's opaque
        // local identifier. L0/001 is the standard identifier for originals;
        // retain the bare UUID as a candidate for libraries that expose it directly.
        NSArray<NSString*>* identifiers = @[ mediaIdString, [mediaIdString stringByAppendingString:@"/L0/001"] ];
        PHFetchResult<PHAsset*>* assets = [PHAsset fetchAssetsWithLocalIdentifiers:identifiers options:nil];
        if (assets.count == 0)
            return OBJ_C_FAILURE_STRING;

        NSArray<PHAssetResource*>* resources = [PHAssetResource assetResourcesForAsset:assets.firstObject];
        for (PHAssetResource* resource in resources) {
            if (resource.type != PHAssetResourceTypeVideo && resource.type != PHAssetResourceTypeFullSizeVideo)
                continue;

            const char* filename = resource.originalFilename.UTF8String;
            return filename == nullptr ? std::string() : std::string(filename);
        }
        return OBJ_C_FAILURE_STRING;
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
