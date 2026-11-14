#import "obj-c.h"
#import <Foundation/Foundation.h>
#import <Photos/Photos.h>
#import <Security/SecTask.h>

#include <QFileInfo>
#include <QHash>

namespace
{
bool appIsSandboxed()
{
    static const bool sandboxed = [] {
        SecTaskRef task = SecTaskCreateFromSelf(kCFAllocatorDefault);
        if (task == nullptr)
            return false;
        CFTypeRef value = SecTaskCopyValueForEntitlement(task, CFSTR("com.apple.security.app-sandbox"), nullptr);
        CFRelease(task);
        const bool enabled = value != nullptr && CFGetTypeID(value) == CFBooleanGetTypeID()
                             && CFBooleanGetValue(static_cast<CFBooleanRef>(value));
        if (value != nullptr)
            CFRelease(value);
        return enabled;
    }();
    return sandboxed;
}
}

struct MacDirectoryAccess::Impl
{
    QHash<QString, NSURL*> directories;

    ~Impl()
    {
        for (NSURL* url : directories) {
            [url stopAccessingSecurityScopedResource];
            [url release];
        }
    }
};

MacDirectoryAccess::MacDirectoryAccess() : _impl(std::make_unique<Impl>()) {}
MacDirectoryAccess::~MacDirectoryAccess() = default;

QString MacDirectoryAccess::retainDirectory(const QString& directory)
{
    if (!appIsSandboxed())
        return {};

    @autoreleasepool {
        // Keep Qt's existing user-granted scope alive while obtaining our own
        // native URL. FFmpeg then shares the directory grant for this session.
        const QFileInfo folder(directory);
        const QString path = folder.canonicalFilePath();
        if (path.isEmpty() || !folder.isDir())
            return QStringLiteral("Folder is not accessible");
        if (_impl->directories.contains(path))
            return {};

        NSString* nativePath = path.toNSString();
        if ([nativePath isEqualToString:NSHomeDirectory()]
            || [nativePath hasPrefix:[NSHomeDirectory() stringByAppendingString:@"/"]]
            || [nativePath isEqualToString:NSBundle.mainBundle.bundlePath]
            || [nativePath hasPrefix:[NSBundle.mainBundle.bundlePath stringByAppendingString:@"/"]])
            return {}; // The container and application bundle need no external grant.

        NSError* error = nil;
        NSURL* url = [NSURL fileURLWithPath:nativePath isDirectory:YES];
        NSData* bookmark = [url bookmarkDataWithOptions:NSURLBookmarkCreationWithSecurityScope
                        includingResourceValuesForKeys:nil relativeToURL:nil error:&error];
        if (bookmark == nil)
            return QStringLiteral("Could not create folder access: %1").arg(QString::fromNSString(error.localizedDescription));

        NSURL* scopedUrl = [NSURL URLByResolvingBookmarkData:bookmark
                        options:NSURLBookmarkResolutionWithSecurityScope | NSURLBookmarkResolutionWithoutUI
                        relativeToURL:nil bookmarkDataIsStale:nullptr error:&error];
        if (scopedUrl == nil)
            return QStringLiteral("Could not resolve folder access: %1").arg(QString::fromNSString(error.localizedDescription));
        if (![scopedUrl startAccessingSecurityScopedResource])
            return QStringLiteral("Folder access could not be retained");

        _impl->directories.insert(path, [scopedUrl retain]);
        return {};
    }
}

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
            return OBJ_C_NO_PHOTOS_ACCESS_STRING;

        NSString* mediaIdString = [NSString stringWithUTF8String:mediaId.c_str()];
        if (mediaIdString == nil)
            return OBJ_C_FAILURE_STRING;

        // Only the System Photo Library can be reached: PhotoKit exposes no public
        // way to open an arbitrary .photoslibrary, so videos scanned from another
        // library find no asset and keep their on-disk UUID name. AppleScript could
        // query whichever library Photos had open, but it is far slower and needs
        // Apple Events permission, which is not worth regaining that one case.
        //
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
