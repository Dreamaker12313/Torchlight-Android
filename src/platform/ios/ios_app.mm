#import <UIKit/UIKit.h>
#import "OgreEAGL2View.h"
#import "OgreEAGL2ViewController.h"
#import <atomic>
#import <thread>
#import <string>
#include "core/touch_pad.h"

extern int tl_engine_main(int argc, char** argv);
static std::atomic<bool> g_active{true};
static std::atomic<bool> g_engine_started{false};
static UIWindow* g_render_window = nil;

static void copyBundledGameData(const std::string& documents, const std::string& bundled) {
    @autoreleasepool {
        NSFileManager* files = [NSFileManager defaultManager];
        NSString* sourceRoot = [NSString stringWithUTF8String:bundled.c_str()];
        NSString* destinationRoot = [NSString stringWithUTF8String:documents.c_str()];
        for (NSString* name in @[@"pak.zip", @"music", @"console_ui"]) {
            NSString* source = [sourceRoot stringByAppendingPathComponent:name];
            NSString* destination = [destinationRoot stringByAppendingPathComponent:name];
            if (![files fileExistsAtPath:source] || [files fileExistsAtPath:destination])
                continue;
            NSError* error = nil;
            if (![files copyItemAtPath:source toPath:destination error:&error]) {
                NSLog(@"Torchlight: could not copy bundled %@ into Documents: %@", name, error);
            }
        }
    }
}

@interface TorchlightAppDelegate : UIResponder <UIApplicationDelegate>
@property(nonatomic, strong) UIWindow* window;
@end

@interface TorchlightSceneDelegate : UIResponder <UIWindowSceneDelegate>
@property(nonatomic, strong) UIWindow* window;
@end

@interface TorchlightRenderView : EAGL2View
@end

@interface TorchlightViewController : EAGL2ViewController
@end

@implementation TorchlightViewController
- (BOOL)shouldAutorotate { return YES; }
- (UIInterfaceOrientationMask)supportedInterfaceOrientations {
    return UIInterfaceOrientationMaskLandscape;
}
@end

@implementation TorchlightRenderView
- (void)layoutSubviews {
    // OGRE's EAGL2View implementation now guards UIKit's early layout calls.
    [super layoutSubviews];
}
- (void)forwardTouches:(NSSet<UITouch*>*)touches phase:(NSInteger)phase {
    auto& pad = tl::core::touch_pad();
    const CGFloat scale = self.contentScaleFactor;
    for (UITouch* touch in touches) {
        const CGPoint point = [touch locationInView:self];
        const int id = static_cast<int>(touch.hash & 0x7fffffff);
        if (phase == 0) pad.touch_down(id, point.x * scale, point.y * scale);
        else if (phase == 1) pad.touch_move(id, point.x * scale, point.y * scale);
        else pad.touch_up(id);
    }
}
- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event { (void)event; [self forwardTouches:touches phase:0]; }
- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event { (void)event; [self forwardTouches:touches phase:1]; }
- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event { (void)event; [self forwardTouches:touches phase:2]; }
- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event { (void)event; [self forwardTouches:touches phase:2]; }
@end

@implementation TorchlightSceneDelegate
- (void)scene:(UIScene*)scene willConnectToSession:(UISceneSession*)session options:(UISceneConnectionOptions*)options {
    (void)session; (void)options;
    if (![scene isKindOfClass:[UIWindowScene class]]) return;
    UIWindowScene* windowScene = (UIWindowScene*)scene;
    self.window = [[UIWindow alloc] initWithWindowScene:windowScene];
    g_render_window = self.window;
    EAGL2ViewController* controller = [TorchlightViewController new];
    EAGL2View* renderView = [[TorchlightRenderView alloc] initWithFrame:self.window.bounds];
    renderView.contentScaleFactor = windowScene.screen.nativeScale;
    renderView.multipleTouchEnabled = YES;
    renderView.opaque = YES;
    controller.view = renderView;
    self.window.rootViewController = controller;
    [self.window makeKeyAndVisible];
    if (!g_engine_started.exchange(true)) {
        NSString* docs = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
        const CGFloat scale = windowScene.screen.nativeScale;
        const int width = (int)(self.window.bounds.size.width * scale);
        const int height = (int)(self.window.bounds.size.height * scale);
        tl::core::touch_pad().set_screen((float)width, (float)height);
        std::string root = std::string(docs.UTF8String) + "/";
        std::string bundledGameData =
            std::string(NSBundle.mainBundle.resourcePath.UTF8String) + "/GameData";
        std::thread([root, bundledGameData, width, height]() mutable {
            copyBundledGameData(root, bundledGameData);
            std::string w = std::to_string(width), h = std::to_string(height);
            char app[] = "torchlight";
            char data_flag[] = "--game-data";
            char width_flag[] = "--width";
            char height_flag[] = "--height";
            char media_flag[] = "--ogre-media";
            std::string media = std::string(NSBundle.mainBundle.resourcePath.UTF8String);
            char* argv[] = {app, data_flag, root.data(), width_flag, w.data(), height_flag, h.data(), media_flag, media.data()};
            (void)tl_engine_main(9, argv);
        }).detach();
    }
}
@end

@implementation TorchlightAppDelegate
- (BOOL)application:(UIApplication*)application didFinishLaunchingWithOptions:(NSDictionary*)options {
    (void)application; (void)options;
    return YES;
}
- (UISceneConfiguration*)application:(UIApplication*)application configurationForConnectingSceneSession:(UISceneSession*)connectingSceneSession options:(UISceneConnectionOptions*)options {
    (void)application; (void)connectingSceneSession; (void)options;
    UISceneConfiguration* config = [[UISceneConfiguration alloc] initWithName:@"Torchlight" sessionRole:UIWindowSceneSessionRoleApplication];
    config.delegateClass = TorchlightSceneDelegate.class;
    return config;
}
- (void)applicationWillResignActive:(UIApplication*)application { (void)application; g_active.store(false); }
- (void)applicationDidBecomeActive:(UIApplication*)application { (void)application; g_active.store(true); }
- (void)applicationWillTerminate:(UIApplication*)application { (void)application; g_active.store(false); }
@end

extern "C" void* tl_ios_view_handle(void) {
    UIWindow* window = g_render_window;
    return (__bridge void*)window.rootViewController.view;
}
extern "C" void* tl_ios_controller_handle(void) {
    UIWindow* window = g_render_window;
    return (__bridge void*)window.rootViewController;
}
extern "C" bool tl_ios_app_active(void) { return g_active.load(); }

int main(int argc, char* argv[]) {
    @autoreleasepool { return UIApplicationMain(argc, argv, nil, NSStringFromClass(TorchlightAppDelegate.class)); }
}
