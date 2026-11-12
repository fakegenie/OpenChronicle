#ifndef PH_IOS_H
#define PH_IOS_H

#include <SDL3/SDL.h>

#include "ph_files.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(SDL_PLATFORM_IOS)
void ph_ios_tap(void);
void ph_ios_fit_view(SDL_Renderer *renderer);
#endif

#ifdef __cplusplus
}
#endif

#endif

#if defined(PH_IOS_IMPLEMENTATION) && defined(__OBJC__) && defined(SDL_PLATFORM_IOS) && !defined(PH_IOS_IMPLEMENTED)
#define PH_IOS_IMPLEMENTED

#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

@interface PHFilePicker : NSObject <UIDocumentPickerDelegate>
@end

@implementation PHFilePicker

- (void)documentPicker:(UIDocumentPickerViewController *)controller didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls
{
    ph_file_picked(urls.firstObject ? urls.firstObject.fileSystemRepresentation : NULL);
}

- (void)documentPickerWasCancelled:(UIDocumentPickerViewController *)controller
{
    ph_file_picked(NULL);
}

@end

static PHFilePicker *ph_file_picker;

int ph_ios_pick_file(SDL_Window *window)
{
    SDL_PropertiesID properties = SDL_GetWindowProperties(window);
    UIWindow *ui_window = (__bridge UIWindow *)SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER, NULL);
    UIViewController *root = ui_window.rootViewController;
    if (!root) {
        return 0;
    }
    if (!ph_file_picker) {
        ph_file_picker = [PHFilePicker new];
    }
    UIDocumentPickerViewController *picker = [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:@[ UTTypeItem ] asCopy:YES];
    picker.delegate = ph_file_picker;
    [root presentViewController:picker animated:YES completion:nil];
    return 1;
}

void ph_ios_tap(void)
{
    static UIImpactFeedbackGenerator *generator;
    if (!generator) {
        generator = [[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleLight];
    }
    [generator impactOccurred];
    [generator prepare];
}

void ph_ios_fit_view(SDL_Renderer *renderer)
{
    SDL_PropertiesID properties = SDL_GetWindowProperties(SDL_GetRenderWindow(renderer));
    UIWindow *window = (__bridge UIWindow *)SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER, NULL);
    UIView *view = window.rootViewController.view;
    if (!view || !view.superview || CGRectEqualToRect(view.frame, view.superview.bounds)) {
        return;
    }
    view.frame = view.superview.bounds;
    [view layoutIfNeeded];
}

#endif
