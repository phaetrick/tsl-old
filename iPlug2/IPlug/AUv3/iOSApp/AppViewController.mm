 /*
 ==============================================================================
 
 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers. 
 
 See LICENSE.txt for  more info.
 
 ==============================================================================
*/

#import "AppViewController.h"
#import "IPlugAUPlayer.h"
#import "IPlugAUAudioUnit.h"

#include "config.h"
#include "IPlugAUv3.h"
#include "IGraphicsEditorDelegate.h"
#include "app.h"

#import "IPlugAUViewController.h"
#import <CoreAudioKit/CoreAudioKit.h>

#if !__has_feature(objc_arc)
#error This file must be compiled with Arc. Use -fobjc-arc flag
#endif

@interface AppViewController ()
{
  IPlugAUPlayer* player;
  IPLUG_AUVIEWCONTROLLER* pluginVC;
  IBOutlet UIView* auView;
}
@end

@implementation AppViewController

- (BOOL) prefersStatusBarHidden
{
  return YES;
}

- (void) viewDidLoad
{
  [super viewDidLoad];

#if PLUG_HAS_UI
  NSString* storyBoardName = [NSString stringWithFormat:@"%s-iOS-MainInterface", PLUG_NAME];
  UIStoryboard* storyboard = [UIStoryboard storyboardWithName:storyBoardName bundle: nil];
  pluginVC = [storyboard instantiateViewControllerWithIdentifier:@"main"];
  [self addChildViewController:pluginVC];
#endif
  
  AudioComponentDescription desc;

#if PLUG_TYPE==0
#if PLUG_DOES_MIDI_IN
  desc.componentType = kAudioUnitType_MusicEffect;
#else
  desc.componentType = kAudioUnitType_Effect;
#endif
#elif PLUG_TYPE==1
  desc.componentType = kAudioUnitType_MusicDevice;
#elif PLUG_TYPE==2
  desc.componentType = 'aumi';
#endif

  desc.componentSubType = PLUG_UNIQUE_ID;
  desc.componentManufacturer = PLUG_MFR_ID;
  desc.componentFlags = 0;
  desc.componentFlagsMask = 0;

  [AUAudioUnit registerSubclass: IPLUG_AUAUDIOUNIT.class asComponentDescription:desc name:@"Local AUv3" version: UINT32_MAX];

  player = [[IPlugAUPlayer alloc] initWithComponentType:desc.componentType];

  [player loadAudioUnitWithComponentDescription:desc completion:^{
    self->pluginVC.audioUnit = (IPLUG_AUAUDIOUNIT*) self->player.currentAudioUnit;

    [self embedPlugInView];
#if defined(STANDALONE_MODE)
    {
      auto* au = (IPLUG_AUAUDIOUNIT*)self->player.currentAudioUnit;
      auto* plug = (iplug::IPlugAUv3*)[au getPlug];
      auto* delegate = dynamic_cast<iplug::igraphics::IGEditorDelegate*>(plug);
      auto* _appState = delegate ? delegate->_appState : nullptr;
      if (_appState) {
        _appState->player.stopAudioFunc = [self]() { [self->player stopAudio]; };
        _appState->player.startAudioFunc = [self]() -> bool { return [self->player startAudio] == YES; };
        _appState->player.setBufferSizeFunc = [self, _appState](int frames) {
          if (_appState->player.try_lock()) {
            AVAudioSession* session = [AVAudioSession sharedInstance];
            NSError* error = nil;
            [session setPreferredIOBufferDuration:(double)frames / session.sampleRate error:&error];
            if (!error) {
              bool wasPlaying = _appState->player.isPlaying();
              if (wasPlaying) [self->player stopAudio];
              if (wasPlaying) [self->player startAudio];
            } else {
              NSLog(@"[TSL] setBufferSize failed: %@", error);
            }
            _appState->player.unlock();
          }
        };
        // Apply preferred buffer size before engine starts
        if (_appState->player.preferredBufferSize > 0) {
          AVAudioSession* session = [AVAudioSession sharedInstance];
          NSError* error = nil;
          [session setPreferredIOBufferDuration:(double)_appState->player.preferredBufferSize / session.sampleRate error:&error];
        }
      }
    }
#endif
  }];
  
  [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(receiveNotification:) name:@"LaunchBTMidiDialog" object:nil];
}

- (void) receiveNotification:(NSNotification*) notification
{
  if ([notification.name isEqualToString:@"LaunchBTMidiDialog"])
  {
    NSDictionary* dict = notification.userInfo;
    NSNumber* x = (NSNumber*) dict[@"x"];
    NSNumber* y = (NSNumber*) dict[@"y"];
   
    CABTMIDICentralViewController* vc = [[CABTMIDICentralViewController alloc] init];
    UINavigationController* nc = [[UINavigationController alloc] initWithRootViewController:vc];
    nc.modalPresentationStyle = UIModalPresentationPopover;
    
    UIPopoverPresentationController* ppc = nc.popoverPresentationController;
    ppc.permittedArrowDirections = UIPopoverArrowDirectionAny;
    ppc.sourceView = self.view;
    ppc.sourceRect = CGRectMake([x floatValue], [y floatValue], 1., 1.);
    
    [self presentViewController:nc animated:YES completion:nil];
  }
}

- (void) embedPlugInView
{
#if PLUG_HAS_UI
  UIView* view = pluginVC.view;
  view.frame = auView.bounds;
  auView.multipleTouchEnabled = YES;
  self.view.multipleTouchEnabled = YES;
  [auView addSubview: view];
#if TARGET_OS_VISION && defined(VISIONOS_TRANSPARENT_VC)
  self.view.opaque = false;
  self.view.backgroundColor = UIColor.clearColor;
#endif
  view.translatesAutoresizingMaskIntoConstraints = NO;
  NSArray* constraints = [NSLayoutConstraint constraintsWithVisualFormat: @"H:|[view]|" options:0 metrics:nil views:NSDictionaryOfVariableBindings(view)];
  [auView addConstraints: constraints];
  constraints = [NSLayoutConstraint constraintsWithVisualFormat: @"V:|[view]|" options:0 metrics:nil views:NSDictionaryOfVariableBindings(view)];
  [auView addConstraints: constraints];

#if defined(OS_IOS) && defined(STANDALONE_MODE)
  NSLog(@"[TSL] embedPlugInView: pinning auView to self.view");
    auView.translatesAutoresizingMaskIntoConstraints = NO;
  NSArray* auConstraintsH = [NSLayoutConstraint constraintsWithVisualFormat: @"H:|[auView]|" options:0 metrics:nil views:NSDictionaryOfVariableBindings(auView)];
  [self.view addConstraints: auConstraintsH];
  NSArray* auConstraintsV = [NSLayoutConstraint constraintsWithVisualFormat: @"V:|[auView]|" options:0 metrics:nil views:NSDictionaryOfVariableBindings(auView)];
  [self.view addConstraints: auConstraintsV];
#endif
#endif
}

- (UIRectEdge) preferredScreenEdgesDeferringSystemGestures
{
  return UIRectEdgeAll;
}

#if TARGET_OS_VISION && defined(VISIONOS_TRANSPARENT_VC)
- (UIContainerBackgroundStyle) preferredContainerBackgroundStyle
{
  return UIContainerBackgroundStyleHidden;
}
#endif
@end

