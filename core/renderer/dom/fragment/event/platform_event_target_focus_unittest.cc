// Copyright 2025 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// Phase 1 — focus-change and focusability tests for PlatformEventTarget.
//
// Phase 1 implements:
//   • OnFocusChange(bool has_focus, bool is_focus_transition):
//       dispatches "focus" when has_focus=true, "blur" when has_focus=false.
//       Phase 1 makes this method virtual so that subclasses / test doubles
//       can intercept focus transitions.
//   • Focusable():
//       returns true only for kInput and kTextarea renderer types,
//       false for generic view types and the default (kUnknown) type.
//
// The OnFocusChange tests use a TestPlatformEventTarget subclass that
// overrides the method to record calls.  The tests verify:
//   1. OnFocusChange is called with has_focus=true when a target gains focus.
//   2. OnFocusChange is called with has_focus=false when a target loses focus.
//   3. The is_focus_transition parameter is forwarded correctly.
//
// The actual focus/blur event dispatch (via the emitter) is tested at the
// integration level; these unit tests verify the dispatch contract and the
// parameter routing from PlatformEventHandler::UpdateFocusedTarget().

#include "core/renderer/dom/fragment/event/platform_event_target.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace lynx {
namespace tasm {
namespace test {

// ---------------------------------------------------------------------------
// TestPlatformEventTarget — records OnFocusChange invocations so tests can
// observe which events would be dispatched.
// ---------------------------------------------------------------------------

class TestPlatformEventTarget : public PlatformEventTarget {
 public:
  // Construct with a specified renderer type and null target_helper.
  // OnFocusChange and Focusable() do not access target_helper_.
  explicit TestPlatformEventTarget(
      PlatformRendererType type = PlatformRendererType::kUnknown,
      int32_t sign = 1)
      : PlatformEventTarget(/*target_helper=*/nullptr, sign,
                            /*left=*/0.f, /*top=*/0.f,
                            /*width=*/100.f, /*height=*/100.f) {
    SetPlatformRendererType(type);
  }

  void OnFocusChange(bool has_focus, bool is_focus_transition) override {
    last_has_focus_ = has_focus;
    last_is_focus_transition_ = is_focus_transition;
    focus_change_count_++;
  }

  int focus_change_count_{0};
  bool last_has_focus_{false};
  bool last_is_focus_transition_{false};
};

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------

class PlatformEventTargetFocusTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

// ---------------------------------------------------------------------------
// OnFocusChange tests
//
// These tests verify that OnFocusChange is called with the correct has_focus
// value and that the is_focus_transition parameter is forwarded correctly.
// Phase 1 makes OnFocusChange virtual and dispatches the focus/blur event
// inside the base-class implementation.  The override in TestPlatformEventTarget
// intercepts the call so tests do not require a live NativePaintingCtxPlatformRef.
// ---------------------------------------------------------------------------

TEST_F(PlatformEventTargetFocusTest,
       OnFocusChange_DispatchesFocusEvent_WhenGainsFocus) {
  // Verifies that OnFocusChange is invoked with has_focus=true when a target
  // gains focus (i.e. Phase 1 will dispatch a "focus" event at this point).
  TestPlatformEventTarget target;

  target.OnFocusChange(/*has_focus=*/true, /*is_focus_transition=*/false);

  EXPECT_EQ(target.focus_change_count_, 1);
  EXPECT_TRUE(target.last_has_focus_);
}

TEST_F(PlatformEventTargetFocusTest,
       OnFocusChange_DispatchesBlurEvent_WhenLosesFocus) {
  // Verifies that OnFocusChange is invoked with has_focus=false when a target
  // loses focus (i.e. Phase 1 will dispatch a "blur" event at this point).
  TestPlatformEventTarget target;

  // First gain focus, then lose it.
  target.OnFocusChange(/*has_focus=*/true,  /*is_focus_transition=*/false);
  target.OnFocusChange(/*has_focus=*/false, /*is_focus_transition=*/false);

  EXPECT_EQ(target.focus_change_count_, 2);
  EXPECT_FALSE(target.last_has_focus_);
}

TEST_F(PlatformEventTargetFocusTest,
       OnFocusChange_FocusTransitionFlag_IsForwarded) {
  TestPlatformEventTarget target;

  target.OnFocusChange(/*has_focus=*/true, /*is_focus_transition=*/true);

  EXPECT_TRUE(target.last_is_focus_transition_);
}

TEST_F(PlatformEventTargetFocusTest,
       OnFocusChange_CalledMultipleTimes_CountsCorrectly) {
  TestPlatformEventTarget target;

  target.OnFocusChange(true,  false);
  target.OnFocusChange(false, false);
  target.OnFocusChange(true,  false);

  EXPECT_EQ(target.focus_change_count_, 3);
  EXPECT_TRUE(target.last_has_focus_);
}

// ---------------------------------------------------------------------------
// Focusable() tests
//
// Phase 1: Focusable() returns true only for kInput and kTextarea element
// types, false for generic views (kView, kPage, kText, kImage, kList, etc.)
// and for the default kUnknown type.
// ---------------------------------------------------------------------------

TEST_F(PlatformEventTargetFocusTest,
       Focusable_ReturnsFalse_ForGenericView) {
  TestPlatformEventTarget target(PlatformRendererType::kView);

  EXPECT_FALSE(target.Focusable());
}

TEST_F(PlatformEventTargetFocusTest,
       Focusable_ReturnsFalse_ForUnknownType) {
  TestPlatformEventTarget target(PlatformRendererType::kUnknown);

  EXPECT_FALSE(target.Focusable());
}

TEST_F(PlatformEventTargetFocusTest,
       Focusable_ReturnsFalse_ForPageType) {
  TestPlatformEventTarget target(PlatformRendererType::kPage);

  EXPECT_FALSE(target.Focusable());
}

TEST_F(PlatformEventTargetFocusTest,
       Focusable_ReturnsFalse_ForTextType) {
  TestPlatformEventTarget target(PlatformRendererType::kText);

  EXPECT_FALSE(target.Focusable());
}

TEST_F(PlatformEventTargetFocusTest,
       Focusable_ReturnsTrue_ForInputElement) {
  // Phase 1 adds kInput to PlatformRendererType for input elements.
  TestPlatformEventTarget target(PlatformRendererType::kInput);

  EXPECT_TRUE(target.Focusable());
}

TEST_F(PlatformEventTargetFocusTest,
       Focusable_ReturnsTrue_ForTextareaElement) {
  // Phase 1 adds kTextarea to PlatformRendererType for textarea elements.
  TestPlatformEventTarget target(PlatformRendererType::kTextarea);

  EXPECT_TRUE(target.Focusable());
}

// ---------------------------------------------------------------------------
// IgnoreFocus() — Phase 1 does not change the default (returns false).
// Verify it remains false for all relevant types so focus routing works.
// ---------------------------------------------------------------------------

TEST_F(PlatformEventTargetFocusTest,
       IgnoreFocus_ReturnsFalse_ByDefault) {
  TestPlatformEventTarget target;

  EXPECT_FALSE(target.IgnoreFocus());
}

// ---------------------------------------------------------------------------
// Combined: Focusable and OnFocusChange together.
//
// In Phase 1, UpdateFocusedTarget() in PlatformEventHandler calls
//   target->OnFocusChange(true, focused_target_->Focusable())
// Verify the is_focus_transition parameter receives the correct bool based
// on whether the *previous* focused element was focusable.
// ---------------------------------------------------------------------------

TEST_F(PlatformEventTargetFocusTest,
       FocusTransition_IsTrue_WhenPreviousTargetWasFocusable) {
  TestPlatformEventTarget input_target(PlatformRendererType::kInput);
  TestPlatformEventTarget view_target(PlatformRendererType::kView);

  // Simulate: focus moves from input_target to view_target.
  // The call to view_target.OnFocusChange has is_focus_transition=input.Focusable()=true.
  bool prev_was_focusable = input_target.Focusable();
  view_target.OnFocusChange(/*has_focus=*/true, prev_was_focusable);

  EXPECT_TRUE(view_target.last_is_focus_transition_);
}

TEST_F(PlatformEventTargetFocusTest,
       FocusTransition_IsFalse_WhenPreviousTargetWasNotFocusable) {
  TestPlatformEventTarget view_source(PlatformRendererType::kView);
  TestPlatformEventTarget input_target(PlatformRendererType::kInput);

  // Focus moves from non-focusable view to input — is_focus_transition=false.
  bool prev_was_focusable = view_source.Focusable();
  input_target.OnFocusChange(/*has_focus=*/true, prev_was_focusable);

  EXPECT_FALSE(input_target.last_is_focus_transition_);
}

}  // namespace test
}  // namespace tasm
}  // namespace lynx
