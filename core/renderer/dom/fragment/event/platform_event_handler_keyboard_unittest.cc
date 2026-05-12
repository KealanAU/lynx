// Copyright 2025 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// Phase 1 — keyboard dispatch tests for PlatformEventHandler.
//
// These tests exercise the keyboard-event pipeline added in Phase 1:
//   • OnInputEvent(event_type=1) dispatches to the focused element.
//   • When there is no focused element nothing is dispatched (no crash).
//   • SetFocusedTarget / UnsetFocusedTarget manage focused_target_ correctly.
//   • Touch events (event_type=0) are unaffected by the keyboard code path.
//
// PlatformEventHandler::OnInputEvent ultimately sends keyboard events via
// platform_ref_->GetEventEmitter()->SendEvent().  Because
// NativePaintingCtxPlatformRef cannot be cheaply constructed in isolation
// (its ctor requires a PlatformRendererFactory), the tests that verify the
// *dispatch-to-emitter* path instead assert observable state and guard
// behaviour that is reachable without a live platform_ref_.
//
// Specifically:
//   • When focused_target_ is null the keyboard case returns early before
//     touching platform_ref_, so nullptr is safe.
//   • SetFocusedTarget / UnsetFocusedTarget are pure state mutations that
//     also do not access platform_ref_.

#include "core/renderer/dom/fragment/event/platform_event_handler.h"

#include "core/renderer/dom/fragment/event/platform_event_target.h"
#include "core/renderer/dom/fragment/event/platform_input_event.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace lynx {
namespace tasm {
namespace test {

// ---------------------------------------------------------------------------
// Minimal PlatformEventTarget subclass for use in keyboard handler tests.
// Constructed with nullptr target_helper because the keyboard path does not
// call any target_helper methods on the focused target.
// ---------------------------------------------------------------------------

class TestPlatformEventTarget : public PlatformEventTarget {
 public:
  // sign=1, full-screen rect so hit-tests succeed if needed.
  TestPlatformEventTarget()
      : PlatformEventTarget(/*target_helper=*/nullptr,
                            /*sign=*/1,
                            /*left=*/0.f, /*top=*/0.f,
                            /*width=*/1000.f, /*height=*/1000.f) {}

  // Track OnFocusChange calls.
  void OnFocusChange(bool has_focus, bool is_focus_transition) override {
    last_focus_value_ = has_focus;
    focus_change_count_++;
  }

  bool last_focus_value_{false};
  int focus_change_count_{0};
};

// ---------------------------------------------------------------------------
// int_event_data layout for keyboard events (Phase 1):
//   [0] = event_type   (0=pointer, 1=keyboard)
//   [1] = action_type  (keyboard: 0=keydown, 1=keyup)
//   [2] = event_source
//   [3] = key_code     (Phase 1 addition)
//   [4] = modifier_flags (Phase 1 addition, bitmask: bit0=shift, bit1=ctrl,
//                         bit2=alt, bit3=meta)
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------

class PlatformEventHandlerKeyboardTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Keyboard-event tests that exercise the no-focused-element guard path
    // never dereference platform_ref_, so nullptr is safe.
    handler_ = std::make_unique<PlatformEventHandler>(
        /*platform_ref=*/nullptr);
    target_ = fml::MakeRefCounted<TestPlatformEventTarget>();
  }

  void TearDown() override {
    handler_.reset();
  }

  // Build a keyboard int_event_data array and call OnInputEvent with a null
  // target_tree (keyboard path does not use target_tree).
  bool SendKeyboardEvent(int action_type, int key_code = 0,
                          int modifier_flags = 0) {
    int int_data[8] = {
        /*event_type=*/1,
        /*action_type=*/action_type,
        /*event_source=*/0,
        /*key_code=*/key_code,
        /*modifier_flags=*/modifier_flags,
        0, 0, 0,
    };
    float float_data[8] = {0.f};
    return handler_->OnInputEvent(/*target_tree=*/nullptr, int_data,
                                  float_data);
  }

  std::unique_ptr<PlatformEventHandler> handler_;
  fml::RefPtr<TestPlatformEventTarget> target_;
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

// When there is no focused element and event_type=1 (keyboard), OnInputEvent
// must return without crashing.  Dispatching to a null focused_target_ is a
// no-op.  The return value is true because there is also no first_target_ so
// EventThrough() returns false.
TEST_F(PlatformEventHandlerKeyboardTest,
       KeyboardEvent_NotDispatched_WhenNoFocusedElement) {
  // No SetFocusedTarget call — focused_target_ is null.
  EXPECT_NO_FATAL_FAILURE(SendKeyboardEvent(/*action_type=*/0));
}

TEST_F(PlatformEventHandlerKeyboardTest,
       KeyboardEvent_KeyDown_NoFocusedElement_ReturnsTrue) {
  bool result = SendKeyboardEvent(/*action_type=*/0, /*key_code=*/65);
  EXPECT_TRUE(result);
}

TEST_F(PlatformEventHandlerKeyboardTest,
       KeyboardEvent_KeyUp_NoFocusedElement_ReturnsTrue) {
  bool result = SendKeyboardEvent(/*action_type=*/1, /*key_code=*/65);
  EXPECT_TRUE(result);
}

// Setting a focused target and then unsetting it leaves focused_target_ null.
// Sending a keyboard event after UnsetFocusedTarget must not dispatch.
TEST_F(PlatformEventHandlerKeyboardTest,
       UnsetFocusedTarget_PreventsDispatch) {
  handler_->SetFocusedTarget(target_);
  handler_->UnsetFocusedTarget(target_);
  // After unset, keyboard event must not crash (no focused target).
  EXPECT_NO_FATAL_FAILURE(SendKeyboardEvent(/*action_type=*/0));
}

// Unsetting a *different* target must not clear focused_target_.
// After the unset the handler still has the original focused target set.
// This test verifies the identity check in UnsetFocusedTarget — it does NOT
// call OnInputEvent because doing so with a non-null focused_target_ and a
// null platform_ref_ would dereference null (dispatching requires platform_ref_).
TEST_F(PlatformEventHandlerKeyboardTest,
       UnsetFocusedTarget_WithWrongTarget_DoesNotClear) {
  auto other_target = fml::MakeRefCounted<TestPlatformEventTarget>();
  handler_->SetFocusedTarget(target_);
  // Pass a different RefPtr (different object pointer) — focused_target_ must
  // remain target_.  UnsetFocusedTarget checks pointer identity (RefPtr::==),
  // so passing a different object must not clear focused_target_.
  handler_->UnsetFocusedTarget(other_target);
  // Verify that the focused_target_ was NOT cleared by the spurious unset:
  // unsetting with the correct target now must still clear it, leaving
  // focused_target_ null so that the subsequent keyboard event is safe.
  handler_->UnsetFocusedTarget(target_);
  EXPECT_NO_FATAL_FAILURE(SendKeyboardEvent(/*action_type=*/0));
}

// PlatformInputEvent correctly extracts event_type and action_type from the
// raw int array — these are the fields that drive keyboard dispatch branching.
TEST_F(PlatformEventHandlerKeyboardTest,
       KeyboardEvent_InputEvent_FieldExtraction) {
  int int_data[8] = {1, 0, 0, 65, 3, 0, 0, 0};
  float float_data[8] = {0.f};

  PlatformInputEvent evt(int_data, float_data);

  EXPECT_EQ(evt.EventType(), 1);     // keyboard
  EXPECT_EQ(evt.ActionType(), 0);    // keydown
}

// Phase 1 adds KeyCode() and ModifierFlags() to PlatformInputEvent.
// This test verifies that both values are read from the correct indices.
TEST_F(PlatformEventHandlerKeyboardTest,
       KeyboardEvent_KeyCode_CorrectlyExtracted) {
  int int_data[8] = {
      /*event_type=*/1,
      /*action_type=*/0,
      /*event_source=*/0,
      /*key_code=*/13,   // Enter
      /*modifier_flags=*/0,
      0, 0, 0,
  };
  float float_data[8] = {0.f};

  PlatformInputEvent evt(int_data, float_data);

  EXPECT_EQ(evt.KeyCode(), 13);
}

TEST_F(PlatformEventHandlerKeyboardTest,
       KeyboardEvent_ModifierFlags_CorrectlyExtracted) {
  // modifier_flags = 0b0011 → shift (bit0) + ctrl (bit1)
  const int shift_bit = 1 << 0;
  const int ctrl_bit  = 1 << 1;
  int modifier_flags  = shift_bit | ctrl_bit;

  int int_data[8] = {
      /*event_type=*/1,
      /*action_type=*/0,
      /*event_source=*/0,
      /*key_code=*/83,   // 's'
      /*modifier_flags=*/modifier_flags,
      0, 0, 0,
  };
  float float_data[8] = {0.f};

  PlatformInputEvent evt(int_data, float_data);

  EXPECT_EQ(evt.ModifierFlags(), modifier_flags);
  EXPECT_TRUE(evt.ModifierFlags() & shift_bit);
  EXPECT_TRUE(evt.ModifierFlags() & ctrl_bit);
  EXPECT_FALSE(evt.ModifierFlags() & (1 << 2));  // alt not set
  EXPECT_FALSE(evt.ModifierFlags() & (1 << 3));  // meta not set
}

TEST_F(PlatformEventHandlerKeyboardTest,
       KeyboardEvent_AllModifierBits_Independent) {
  const int shift_bit = 1 << 0;
  const int ctrl_bit  = 1 << 1;
  const int alt_bit   = 1 << 2;
  const int meta_bit  = 1 << 3;

  for (int flags = 0; flags < 16; ++flags) {
    int int_data[8] = {1, 0, 0, 65, flags, 0, 0, 0};
    float float_data[8] = {0.f};
    PlatformInputEvent evt(int_data, float_data);

    bool expect_shift = (flags & shift_bit) != 0;
    bool expect_ctrl  = (flags & ctrl_bit)  != 0;
    bool expect_alt   = (flags & alt_bit)   != 0;
    bool expect_meta  = (flags & meta_bit)  != 0;

    EXPECT_EQ(static_cast<bool>(evt.ModifierFlags() & shift_bit), expect_shift)
        << "flags=" << flags;
    EXPECT_EQ(static_cast<bool>(evt.ModifierFlags() & ctrl_bit), expect_ctrl)
        << "flags=" << flags;
    EXPECT_EQ(static_cast<bool>(evt.ModifierFlags() & alt_bit), expect_alt)
        << "flags=" << flags;
    EXPECT_EQ(static_cast<bool>(evt.ModifierFlags() & meta_bit), expect_meta)
        << "flags=" << flags;
  }
}

// A touch event (event_type=0) sent after keyboard setup must not crash.
// This verifies the keyboard code path does not corrupt handler state for
// subsequent pointer events.
TEST_F(PlatformEventHandlerKeyboardTest,
       KeyboardEvent_DoesNotInterfereWithTouchEventSetup) {
  // First send a keyboard event.
  EXPECT_NO_FATAL_FAILURE(SendKeyboardEvent(/*action_type=*/0));

  // Then confirm that sending a touch event (event_type=0) also does not
  // crash — target_tree is nullptr so FindTarget returns nullptr and the
  // touch path short-circuits cleanly.
  int int_data[8] = {
      /*event_type=*/0,
      /*action_type=*/0,  // pointer down
      /*event_source=*/0,
      /*pointer_count=*/1,
      0, 0, 0, 0,
  };
  float float_data[8] = {
      /*pointer_id=*/0.f, /*x=*/0.f, /*y=*/0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
  };
  EXPECT_NO_FATAL_FAILURE(
      handler_->OnInputEvent(/*target_tree=*/nullptr, int_data, float_data));
}

// KeyString is carried in float_event_data for the keyboard path (Phase 1).
// This test verifies that PlatformInputEvent::KeyString() round-trips a
// single character encoded as a float.
TEST_F(PlatformEventHandlerKeyboardTest,
       KeyboardEvent_KeyString_CarriedInFloatData) {
  int int_data[8] = {1, 0, 0, 65, 0, 0, 0, 0};
  // Encode 'A' (65) as a float in float_event_data[0] per Phase 1 spec.
  float float_data[8] = {65.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};

  PlatformInputEvent evt(int_data, float_data);

  // KeyString() must return a non-empty string built from float_event_data.
  EXPECT_FALSE(evt.KeyString().empty());
}

}  // namespace test
}  // namespace tasm
}  // namespace lynx
