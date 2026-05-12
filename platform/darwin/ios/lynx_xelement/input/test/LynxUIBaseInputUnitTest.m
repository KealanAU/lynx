// Copyright 2026 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#import "LynxUIBaseInputUnitTest.h"
#import <Lynx/LynxUI+Internal.h>
#import <OCMock/OCMock.h>
#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>
#import <XElement/LynxUIBaseInput.h>
#import <objc/runtime.h>

// ---------------------------------------------------------------------------
// Expose private API for testing
// ---------------------------------------------------------------------------

@interface LynxUIBaseInput (TestAccess)
- (CGFloat)handleAvoidKeyboard:(BOOL)keyboardDisplayed notification:(NSNotification *)notification;
@property(nonatomic, assign) BOOL avoidKeyboardInLynxView;
@property(nonatomic, assign) CGFloat keyboardHeight;
@property(nonatomic, assign) CGFloat avoidKeyboardDist;
@end

// ---------------------------------------------------------------------------
// Globals used by the swizzled UIView animation stub
// ---------------------------------------------------------------------------

static NSTimeInterval gCapturedDuration = -1.0;
static NSInteger gAnimationCallCount = 0;

static void swizzledAnimate(id self_cls,
                            SEL _cmd,
                            NSTimeInterval duration,
                            NSTimeInterval delay,
                            UIViewAnimationOptions options,
                            void (^animations)(void),
                            void (^completion)(BOOL)) {
  gCapturedDuration = duration;
  gAnimationCallCount++;
  if (animations) {
    animations();
  }
}

// ---------------------------------------------------------------------------

@implementation LynxUIBaseInputUnitTest {
  IMP _originalAnimateIMP;
  SEL _animateSEL;

  // Strong references that outlive the test method — required because
  // LynxUI._context is a __weak ivar and we must keep the mock alive.
  id _mockView;
  id _mockRootView;
  id _mockUIContext;
  LynxUIBaseInput *_input;
}

- (void)setUp {
  [super setUp];
  gCapturedDuration = -1.0;
  gAnimationCallCount = 0;

  // Swizzle +[UIView animateWithDuration:delay:options:animations:completion:]
  _animateSEL = @selector(animateWithDuration:delay:options:animations:completion:);
  Method original = class_getClassMethod([UIView class], _animateSEL);
  _originalAnimateIMP = method_getImplementation(original);
  method_setImplementation(original, (IMP)swizzledAnimate);

  [self buildInput];
}

- (void)tearDown {
  Method original = class_getClassMethod([UIView class], _animateSEL);
  method_setImplementation(original, _originalAnimateIMP);

  [_mockView stopMocking];
  [_mockRootView stopMocking];
  [_mockUIContext stopMocking];
  _input = nil;

  [super tearDown];
}

// ---------------------------------------------------------------------------
// Build a minimal LynxUIBaseInput wired up for avoid-keyboard testing.
// The view is positioned so the keyboard gap is positive, ensuring the
// animation block actually fires.
// ---------------------------------------------------------------------------

- (void)buildInput {
  // View at y=200 with height=44; bottom is at y=244.
  // Screen height ~667; keyboard height 336 → bottom-to-screen = 667-244 = 423.
  // gap = 336 - 423 + 0 = -87 → gap NOT > 0 with avoidKeyboardDist==0.
  // So position the view at the bottom of the screen to make gap positive.
  // View bottom at y=640 → bottom-to-screen = 667-640 = 27; gap = 336-27 = 309 > 0.
  CGRect viewBounds = CGRectMake(0, 0, 375, 44);
  CGRect viewInScreen = CGRectMake(0, 596, 375, 44);  // bottom at y=640

  _mockView = OCMClassMock([UITextField class]);
  OCMStub([_mockView isFirstResponder]).andReturn(YES);
  OCMStub([_mockView bounds]).andReturn(viewBounds);
  OCMStub([_mockView convertRect:viewBounds toView:[OCMArg any]]).andReturn(viewInScreen);

  _mockRootView = OCMClassMock([UIView class]);
  OCMStub([_mockRootView frame]).andReturn(CGRectMake(0, 0, 375, 667));

  _mockUIContext = OCMClassMock(NSClassFromString(@"LynxUIContext"));
  OCMStub([_mockUIContext rootView]).andReturn(_mockRootView);

  _input = [[LynxUIBaseInput alloc] initWithView:_mockView];
  // Inject _context using KVC. The strong reference is kept alive by _mockUIContext ivar.
  [_input setValue:_mockUIContext forKey:@"_context"];
  _input.avoidKeyboardInLynxView = YES;
  _input.keyboardHeight = 336.0;
  _input.avoidKeyboardDist = 0;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

- (NSDictionary *)keyboardUserInfoWithDuration:(NSTimeInterval)duration height:(CGFloat)height {
  CGFloat screenHeight = UIScreen.mainScreen.bounds.size.height;
  CGRect frame = CGRectMake(0, screenHeight - height, 375, height);
  return @{
    UIKeyboardFrameEndUserInfoKey : [NSValue valueWithCGRect:frame],
    UIKeyboardAnimationDurationUserInfoKey : @(duration),
    UIKeyboardAnimationCurveUserInfoKey : @(UIViewAnimationCurveEaseInOut),
  };
}

// ---------------------------------------------------------------------------
// Test: Phase 3 fix — animation duration read from notification userInfo
// ---------------------------------------------------------------------------

- (void)testHandleAvoidKeyboard_usesSystemAnimationDuration {
  const NSTimeInterval kSystemDuration = 0.55;  // deliberately different from 0.3 fallback
  NSDictionary *userInfo = [self keyboardUserInfoWithDuration:kSystemDuration height:336.0];
  NSNotification *notification =
      [NSNotification notificationWithName:UIKeyboardWillShowNotification
                                    object:nil
                                  userInfo:userInfo];

  [_input handleAvoidKeyboard:YES notification:notification];

  XCTAssertEqual(gAnimationCallCount, 1,
                 @"+[UIView animateWithDuration:...] should be called once");
  XCTAssertEqualWithAccuracy(gCapturedDuration, kSystemDuration, 0.001,
                             @"Animation duration must come from notification userInfo, "
                             @"not the hardcoded 0.3 fallback");
}

// ---------------------------------------------------------------------------
// Test: fallback to 0.3 when no notification is provided
// ---------------------------------------------------------------------------

- (void)testHandleAvoidKeyboard_fallbackDurationWhenNoNotification {
  [_input handleAvoidKeyboard:YES notification:nil];

  XCTAssertEqual(gAnimationCallCount, 1,
                 @"+[UIView animateWithDuration:...] should be called once");
  XCTAssertEqualWithAccuracy(gCapturedDuration, 0.3, 0.001,
                             @"Animation duration should fall back to 0.3 when notification is nil");
}

@end
