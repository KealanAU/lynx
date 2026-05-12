// Copyright 2024 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "core/event/keyboard_event.h"

#include "base/include/value/table.h"

namespace lynx {
namespace event {

KeyboardEvent::KeyboardEvent(const std::string& event_name,
                             const std::string& key, const std::string& code,
                             int key_code, bool ctrl_key, bool shift_key,
                             bool alt_key, bool meta_key, bool repeat,
                             bool is_composing)
    : Event(event_name, Event::EventType::kKeyboardEvent, Event::Capture::kYes,
            Event::Bubbles::kYes, Event::Cancelable::kYes,
            Event::ComposedMode::kComposed),
      key_(key),
      code_(code),
      key_code_(key_code),
      ctrl_key_(ctrl_key),
      shift_key_(shift_key),
      alt_key_(alt_key),
      meta_key_(meta_key),
      repeat_(repeat),
      is_composing_(is_composing) {}

KeyboardEvent::~KeyboardEvent() = default;

void KeyboardEvent::HandleEventCustomDetail() {
  BASE_STATIC_STRING_DECL(kKey, "key");
  BASE_STATIC_STRING_DECL(kCode, "code");
  BASE_STATIC_STRING_DECL(kKeyCode, "keyCode");
  BASE_STATIC_STRING_DECL(kCtrlKey, "ctrlKey");
  BASE_STATIC_STRING_DECL(kShiftKey, "shiftKey");
  BASE_STATIC_STRING_DECL(kAltKey, "altKey");
  BASE_STATIC_STRING_DECL(kMetaKey, "metaKey");
  BASE_STATIC_STRING_DECL(kRepeat, "repeat");
  BASE_STATIC_STRING_DECL(kIsComposing, "isComposing");

  auto dict = detail_.Table();
  dict->SetValue(kKey, key_);
  dict->SetValue(kCode, code_);
  dict->SetValue(kKeyCode, static_cast<double>(key_code_));
  dict->SetValue(kCtrlKey, ctrl_key_);
  dict->SetValue(kShiftKey, shift_key_);
  dict->SetValue(kAltKey, alt_key_);
  dict->SetValue(kMetaKey, meta_key_);
  dict->SetValue(kRepeat, repeat_);
  dict->SetValue(kIsComposing, is_composing_);
}

}  // namespace event
}  // namespace lynx
