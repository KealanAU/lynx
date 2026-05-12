// Copyright 2025 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "core/event/keyboard_event.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace lynx {
namespace event {
namespace test {

// ---------------------------------------------------------------------------
// KeyboardEvent — Phase 1 unit tests
//
// These tests verify the DOM-aligned properties added in Phase 1:
//   key_, code_, key_code_ (int), ctrl_key_, shift_key_, alt_key_, meta_key_,
//   repeat_, is_composing_, and the EVENT_KEY_DOWN / EVENT_KEY_UP constants.
// ---------------------------------------------------------------------------

TEST(KeyboardEventTest, KeyDownEventName) {
  // Phase 1 adds EVENT_KEY_DOWN = "keydown" constant and uses it as the
  // event type string when action_type == 0.
  EXPECT_STREQ(KeyboardEvent::EVENT_KEY_DOWN, "keydown");
}

TEST(KeyboardEventTest, KeyUpEventName) {
  // Phase 1 adds EVENT_KEY_UP = "keyup" constant used when action_type == 1.
  EXPECT_STREQ(KeyboardEvent::EVENT_KEY_UP, "keyup");
}

TEST(KeyboardEventTest, DefaultConstruction) {
  // Construct a keydown event with no modifiers and verify every modifier
  // bool and state flag defaults to false.
  KeyboardEvent evt(KeyboardEvent::EVENT_KEY_DOWN, "a", "KeyA", 65);

  EXPECT_FALSE(evt.ctrl_key());
  EXPECT_FALSE(evt.shift_key());
  EXPECT_FALSE(evt.alt_key());
  EXPECT_FALSE(evt.meta_key());
  EXPECT_FALSE(evt.repeat());
  EXPECT_FALSE(evt.is_composing());
}

TEST(KeyboardEventTest, KeyStringAndCode) {
  KeyboardEvent evt(KeyboardEvent::EVENT_KEY_DOWN, "Enter", "Enter", 13);

  EXPECT_EQ(evt.key(), "Enter");
  EXPECT_EQ(evt.code(), "Enter");
  EXPECT_EQ(evt.key_code(), 13);
}

TEST(KeyboardEventTest, KeyUpEventType) {
  KeyboardEvent evt(KeyboardEvent::EVENT_KEY_UP, "Escape", "Escape", 27);

  EXPECT_EQ(evt.type(), std::string(KeyboardEvent::EVENT_KEY_UP));
}

TEST(KeyboardEventTest, ModifierFields_Ctrl) {
  // Construct with ctrl modifier set.
  KeyboardEvent evt(KeyboardEvent::EVENT_KEY_DOWN, "c", "KeyC", 67,
                    /*ctrl_key=*/true, /*shift_key=*/false,
                    /*alt_key=*/false, /*meta_key=*/false);

  EXPECT_TRUE(evt.ctrl_key());
  EXPECT_FALSE(evt.shift_key());
  EXPECT_FALSE(evt.alt_key());
  EXPECT_FALSE(evt.meta_key());
}

TEST(KeyboardEventTest, ModifierFields_Shift) {
  KeyboardEvent evt(KeyboardEvent::EVENT_KEY_DOWN, "A", "KeyA", 65,
                    /*ctrl_key=*/false, /*shift_key=*/true,
                    /*alt_key=*/false, /*meta_key=*/false);

  EXPECT_FALSE(evt.ctrl_key());
  EXPECT_TRUE(evt.shift_key());
  EXPECT_FALSE(evt.alt_key());
  EXPECT_FALSE(evt.meta_key());
}

TEST(KeyboardEventTest, ModifierFields_AltAndMeta) {
  KeyboardEvent evt(KeyboardEvent::EVENT_KEY_DOWN, "F4", "F4", 115,
                    /*ctrl_key=*/false, /*shift_key=*/false,
                    /*alt_key=*/true, /*meta_key=*/true);

  EXPECT_FALSE(evt.ctrl_key());
  EXPECT_FALSE(evt.shift_key());
  EXPECT_TRUE(evt.alt_key());
  EXPECT_TRUE(evt.meta_key());
}

TEST(KeyboardEventTest, AllModifiersSet) {
  KeyboardEvent evt(KeyboardEvent::EVENT_KEY_DOWN, "Delete", "Delete", 46,
                    /*ctrl_key=*/true, /*shift_key=*/true,
                    /*alt_key=*/true, /*meta_key=*/true);

  EXPECT_TRUE(evt.ctrl_key());
  EXPECT_TRUE(evt.shift_key());
  EXPECT_TRUE(evt.alt_key());
  EXPECT_TRUE(evt.meta_key());
}

TEST(KeyboardEventTest, BubblesAndCancelable) {
  // KeyboardEvent should bubble (matching DOM spec / touch-event pattern).
  // Phase 1 keeps cancelable=kNo consistent with the existing KeyboardEvent
  // constructor that uses Cancelable::kNo.
  KeyboardEvent evt(KeyboardEvent::EVENT_KEY_DOWN, "a", "KeyA", 65);

  EXPECT_TRUE(evt.bubbles());
  // capture is set to kYes in the existing constructor.
  EXPECT_TRUE(evt.capture());
}

TEST(KeyboardEventTest, EventType_IsKeyboardEvent) {
  KeyboardEvent evt(KeyboardEvent::EVENT_KEY_DOWN, "a", "KeyA", 65);
  EXPECT_EQ(evt.event_type(), Event::EventType::kKeyboardEvent);
}

TEST(KeyboardEventTest, RepeatFlag) {
  KeyboardEvent evt(KeyboardEvent::EVENT_KEY_DOWN, "a", "KeyA", 65,
                    false, false, false, false, /*repeat=*/true);

  EXPECT_TRUE(evt.repeat());
}

TEST(KeyboardEventTest, IsComposingFlag) {
  KeyboardEvent evt(KeyboardEvent::EVENT_KEY_DOWN, "a", "KeyA", 65,
                    false, false, false, false, /*repeat=*/false,
                    /*is_composing=*/true);

  EXPECT_TRUE(evt.is_composing());
}

}  // namespace test
}  // namespace event
}  // namespace lynx
