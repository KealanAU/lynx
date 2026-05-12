// Copyright 2026 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#import "LynxKeyboardEventDispatcherUnitTest.h"
#import <Lynx/LynxContext.h>
#import <Lynx/LynxKeyboardEventDispatcher.h>
#import <OCMock/OCMock.h>
#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

// ---------------------------------------------------------------------------
// Test observer that records every call made to it.
// Implements the extended Phase 3 protocol methods via respondsToSelector:.
// ---------------------------------------------------------------------------

@interface LynxKeyboardObserverSpy : NSObject <LynxKeyboardEventObserver>
@property(nonatomic, assign) CGFloat lastHeight;
@property(nonatomic, assign) NSInteger willShowCallCount;
@property(nonatomic, assign) NSInteger willHideCallCount;
// Phase 3: onWillShowKeyboard: / onWillHideKeyboard: callbacks
@property(nonatomic, assign) NSInteger onWillShowCount;
@property(nonatomic, assign) NSInteger onWillHideCount;
@property(nonatomic, strong) NSNotification *lastOnWillShowNotification;
@end

@implementation LynxKeyboardObserverSpy

- (void)keyboardWillShow:(CGFloat)keyboardHeight {
  _willShowCallCount++;
  _lastHeight = keyboardHeight;
}

- (void)keyboardWillHide {
  _willHideCallCount++;
}

// Optional Phase 3 callbacks — dispatcher checks respondsToSelector: before calling.
- (void)onWillShowKeyboard:(NSNotification *)notification {
  _onWillShowCount++;
  _lastOnWillShowNotification = notification;
}

- (void)onWillHideKeyboard:(NSNotification *)notification {
  _onWillHideCount++;
}

@end

// ---------------------------------------------------------------------------

@implementation LynxKeyboardEventDispatcherUnitTest {
  LynxKeyboardEventDispatcher *_dispatcher;
  id _mockContext;
}

- (void)setUp {
  [super setUp];
  _mockContext = OCMClassMock([LynxContext class]);
  OCMStub([_mockContext sendGlobalEvent:[OCMArg any] withParams:[OCMArg any]]);
  _dispatcher = [[LynxKeyboardEventDispatcher alloc] initWithContext:_mockContext];
}

- (void)tearDown {
  [[NSNotificationCenter defaultCenter] removeObserver:_dispatcher];
  _dispatcher = nil;
  [_mockContext stopMocking];
  [super tearDown];
}

// ---------------------------------------------------------------------------
// Helper: post UIKeyboardWillShowNotification with a specific keyboard height.
//
// On iOS < 16 the dispatcher tries to walk the view hierarchy to find the
// keyboard window; this always returns nil in a unit-test host, causing the
// handler to return early without notifying observers.  We therefore gate
// those tests to iOS 16+ where the dispatcher uses UIKeyboardFrameEndUserInfoKey.
// ---------------------------------------------------------------------------

- (NSDictionary *)userInfoWithKeyboardHeight:(CGFloat)height {
  CGFloat screenHeight = UIScreen.mainScreen.bounds.size.height;
  CGRect frame = CGRectMake(0, screenHeight - height, UIScreen.mainScreen.bounds.size.width, height);
  return @{
    UIKeyboardFrameEndUserInfoKey : [NSValue valueWithCGRect:frame],
    UIKeyboardAnimationDurationUserInfoKey : @(0.25),
    UIKeyboardAnimationCurveUserInfoKey : @(UIViewAnimationCurveEaseInOut),
  };
}

// ---------------------------------------------------------------------------
// Tests — keyboard dispatcher
// ---------------------------------------------------------------------------

/// Posting UIKeyboardWillShowNotification must deliver keyboardWillShow: with
/// the correct height extracted from UIKeyboardFrameEndUserInfoKey (iOS 16+ path).
- (void)testKeyboardWillShowNotification_dispatchesHeightToObservers {
  if (@available(iOS 16, *)) {
    // Test is only reliable on iOS 16+ where the dispatcher uses userInfo directly.
  } else {
    return;
  }

  LynxKeyboardObserverSpy *spy = [[LynxKeyboardObserverSpy alloc] init];
  [_dispatcher addKeyboardEventObserver:spy];

  const CGFloat expectedHeight = 336.0;
  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIKeyboardWillShowNotification
                    object:nil
                  userInfo:[self userInfoWithKeyboardHeight:expectedHeight]];

  XCTAssertEqual(spy.willShowCallCount, 1,
                 @"Observer should receive exactly one keyboardWillShow: call");
  XCTAssertEqual((int)spy.lastHeight, (int)expectedHeight,
                 @"Observer should receive the keyboard height from the notification");
}

/// Posting UIKeyboardWillHideNotification must call keyboardWillHide on observers.
- (void)testKeyboardWillHideNotification_dispatchesZeroHeight {
  // Simulate keyboard showing first so _keyboardVisible is set to YES,
  // which matches real device behavior.  WillHide doesn't depend on iOS version.
  if (@available(iOS 16, *)) {
    [[NSNotificationCenter defaultCenter]
        postNotificationName:UIKeyboardWillShowNotification
                      object:nil
                    userInfo:[self userInfoWithKeyboardHeight:336.0]];
  } else {
    // On older OS the show path won't trigger (no keyboard window in tests),
    // so we still post WillHide and verify the dispatcher handles it gracefully.
  }

  LynxKeyboardObserverSpy *spy = [[LynxKeyboardObserverSpy alloc] init];
  [_dispatcher addKeyboardEventObserver:spy];

  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIKeyboardWillHideNotification
                    object:nil
                  userInfo:@{}];

  XCTAssertEqual(spy.willHideCallCount, 1,
                 @"Observer should receive exactly one keyboardWillHide call");
  XCTAssertEqual(spy.willShowCallCount, 0,
                 @"keyboardWillShow: should not be called on hide");
}

/// Posting UIKeyboardWillChangeFrameNotification while the keyboard is visible
/// (Phase 3 fix) must call onWillShowKeyboard: on observers that implement it.
- (void)testKeyboardWillChangeFrameNotification_updatesHeight {
  if (@available(iOS 16, *)) {
    // Test is only reliable on iOS 16+ (show path populates _keyboardVisible).
  } else {
    return;
  }

  LynxKeyboardObserverSpy *spy = [[LynxKeyboardObserverSpy alloc] init];
  [_dispatcher addKeyboardEventObserver:spy];

  // First bring the keyboard up so _keyboardVisible is set.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIKeyboardWillShowNotification
                    object:nil
                  userInfo:[self userInfoWithKeyboardHeight:216.0]];

  const CGFloat newHeight = 291.0;
  NSDictionary *changeUserInfo = [self userInfoWithKeyboardHeight:newHeight];

  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIKeyboardWillChangeFrameNotification
                    object:nil
                  userInfo:changeUserInfo];

  // The dispatcher forwards frame changes via onWillShowKeyboard:.
  XCTAssertEqual(spy.onWillShowCount, 2,
                 @"onWillShowKeyboard: should be called for initial show + frame change");
  XCTAssertNotNil(spy.lastOnWillShowNotification,
                  @"onWillShowKeyboard: must receive the notification");

  // Verify the notification carried the updated height.
  NSValue *frameValue =
      spy.lastOnWillShowNotification.userInfo[UIKeyboardFrameEndUserInfoKey];
  CGRect updatedFrame = [frameValue CGRectValue];
  XCTAssertEqualWithAccuracy(updatedFrame.size.height, newHeight, 1.0,
                             @"Frame-change notification should contain the new keyboard height");
}

/// When the keyboard frame changes BEFORE the keyboard is visible,
/// the dispatcher must NOT forward the notification (guards against spurious
/// UIKeyboardWillChangeFrameNotification during show animation).
- (void)testKeyboardWillChangeFrameNotification_ignoredWhenKeyboardNotVisible {
  LynxKeyboardObserverSpy *spy = [[LynxKeyboardObserverSpy alloc] init];
  [_dispatcher addKeyboardEventObserver:spy];

  // Post change without preceding WillShow — _keyboardVisible starts as NO.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIKeyboardWillChangeFrameNotification
                    object:nil
                  userInfo:[self userInfoWithKeyboardHeight:336.0]];

  XCTAssertEqual(spy.onWillShowCount, 0,
                 @"onWillShowKeyboard: must not be called when keyboard is not visible");
}

/// Registering two observers and posting a single notification must deliver it
/// to both.
- (void)testObserverRegistration_multipleInputs {
  if (@available(iOS 16, *)) {
    // Test is only reliable on iOS 16+.
  } else {
    return;
  }

  LynxKeyboardObserverSpy *spy1 = [[LynxKeyboardObserverSpy alloc] init];
  LynxKeyboardObserverSpy *spy2 = [[LynxKeyboardObserverSpy alloc] init];
  [_dispatcher addKeyboardEventObserver:spy1];
  [_dispatcher addKeyboardEventObserver:spy2];

  const CGFloat expectedHeight = 216.0;
  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIKeyboardWillShowNotification
                    object:nil
                  userInfo:[self userInfoWithKeyboardHeight:expectedHeight]];

  XCTAssertEqual(spy1.willShowCallCount, 1,
                 @"First observer should receive keyboardWillShow:");
  XCTAssertEqual(spy2.willShowCallCount, 1,
                 @"Second observer should receive keyboardWillShow:");
  XCTAssertEqual((int)spy1.lastHeight, (int)expectedHeight,
                 @"First observer should receive correct height");
  XCTAssertEqual((int)spy2.lastHeight, (int)expectedHeight,
                 @"Second observer should receive correct height");

  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIKeyboardWillHideNotification
                    object:nil
                  userInfo:@{}];

  XCTAssertEqual(spy1.willHideCallCount, 1, @"First observer should receive keyboardWillHide");
  XCTAssertEqual(spy2.willHideCallCount, 1, @"Second observer should receive keyboardWillHide");
}

@end
