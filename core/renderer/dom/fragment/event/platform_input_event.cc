// Copyright 2025 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "core/renderer/dom/fragment/event/platform_input_event.h"

#include <chrono>
#include <utility>

namespace lynx {
namespace tasm {

PlatformInputEvent::PlatformInputEvent(int int_event_data[],
                                       float float_event_data[],
                                       const char* key_string) {
  event_type_ = int_event_data[0];
  action_type_ = int_event_data[1];
  event_source_ = int_event_data[2];
  // Indices 3 and 4 carry keyboard-specific data and are only valid for
  // keyboard events (event_type == 1).
  if (event_type_ == 1) {
    key_code_ = int_event_data[3];
    modifier_flags_ = int_event_data[4];
  }
  time_stamp_ = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count();
  if (key_string != nullptr) {
    key_string_ = key_string;
  }
}

}  // namespace tasm
}  // namespace lynx
