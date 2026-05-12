// Copyright 2024 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef CORE_EVENT_KEYBOARD_EVENT_H_
#define CORE_EVENT_KEYBOARD_EVENT_H_

#include <string>

#include "core/event/event.h"

namespace lynx {
namespace event {

class KeyboardEvent : public Event {
 public:
  static constexpr const char* EVENT_KEY_DOWN = "keydown";
  static constexpr const char* EVENT_KEY_UP = "keyup";

  KeyboardEvent(const std::string& event_name, const std::string& key,
                const std::string& code, int key_code, bool ctrl_key,
                bool shift_key, bool alt_key, bool meta_key, bool repeat,
                bool is_composing);
  ~KeyboardEvent();

  const std::string& key() const { return key_; }
  const std::string& code() const { return code_; }
  int key_code() const { return key_code_; }
  bool ctrl_key() const { return ctrl_key_; }
  bool shift_key() const { return shift_key_; }
  bool alt_key() const { return alt_key_; }
  bool meta_key() const { return meta_key_; }
  bool repeat() const { return repeat_; }
  bool is_composing() const { return is_composing_; }

  void HandleEventCustomDetail() override;

 private:
  std::string key_;
  std::string code_;
  int key_code_{0};
  bool ctrl_key_{false};
  bool shift_key_{false};
  bool alt_key_{false};
  bool meta_key_{false};
  bool repeat_{false};
  bool is_composing_{false};
};

}  // namespace event
}  // namespace lynx

#endif  // CORE_EVENT_KEYBOARD_EVENT_H_
