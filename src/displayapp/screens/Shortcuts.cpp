#include "displayapp/screens/Shortcuts.h"
#include "displayapp/InfiniTimeTheme.h"
#include "components/ble/ShortcutService.h"
#include "components/motion/MotionController.h"
#include "components/motor/MotorController.h"
#include <cstdlib>

using namespace Pinetime::Applications::Screens;

namespace {
  // Indexed by GestureDirection; shown next to each shortcut's name so the wrist
  // movement that triggers it doesn't have to be memorized.
  constexpr std::array<const char*, Shortcuts::MaxDisplayed> gestureHints {"^", "v", "<", ">"};
}

namespace {
  void ButtonEventHandler(lv_obj_t* obj, lv_event_t event) {
    if (event != LV_EVENT_CLICKED) {
      return;
    }
    auto* screen = static_cast<Shortcuts*>(obj->user_data);
    screen->OnButtonEvent(obj);
  }
}

Shortcuts::Shortcuts(Controllers::ShortcutService& shortcutService,
                     Controllers::MotionController& motionController,
                     Controllers::MotorController& motorController)
  : shortcutService {shortcutService}, motionController {motionController}, motorController {motorController} {
  lv_obj_set_style_local_bg_color(lv_scr_act(), LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_make(0, 0, 0));

  std::array<const Controllers::ShortcutService::Shortcut*, MaxDisplayed> selectedShortcuts {};
  for (const auto& shortcut : shortcutService.GetShortcuts()) {
    if (buttonCount >= MaxDisplayed) {
      break;
    }
    if (shortcut.has_value()) {
      selectedShortcuts[buttonCount] = &shortcut.value();
      buttonShortcutIds[buttonCount] = shortcut->id;
      buttonCount++;
    }
  }

  if (buttonCount == 0) {
    lv_obj_t* label = lv_label_create(lv_scr_act(), nullptr);
    lv_label_set_long_mode(label, LV_LABEL_LONG_BREAK);
    lv_obj_set_width(label, LV_HOR_RES - 20);
    lv_label_set_align(label, LV_LABEL_ALIGN_CENTER);
    lv_label_set_text_static(label, "No shortcuts configured.\nSet them up in the\ncompanion app.");
    lv_obj_align(label, nullptr, LV_ALIGN_CENTER, 0, 0);
    return;
  }

  lv_obj_t* container = lv_cont_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_bg_opa(container, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_TRANSP);
  lv_obj_set_style_local_border_width(container, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 0);
  static constexpr int innerPad = 4;
  lv_obj_set_style_local_pad_inner(container, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, innerPad);

  lv_obj_set_pos(container, 0, 0);
  lv_obj_set_width(container, LV_HOR_RES - 8);
  lv_obj_set_height(container, LV_VER_RES);
  lv_cont_set_layout(container, LV_LAYOUT_COLUMN_LEFT);

  const int btnHeight = (LV_VER_RES_MAX - ((buttonCount - 1) * innerPad)) / buttonCount;

  for (uint8_t i = 0; i < buttonCount; i++) {
    lv_obj_t* button = lv_btn_create(container, nullptr);
    lv_obj_set_style_local_radius(button, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, btnHeight / 3);
    lv_obj_set_style_local_bg_color(button, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, Colors::bgAlt);
    lv_obj_set_width(button, LV_HOR_RES - 8);
    lv_obj_set_height(button, btnHeight);
    lv_obj_set_event_cb(button, ButtonEventHandler);
    lv_btn_set_layout(button, LV_LAYOUT_OFF);
    button->user_data = this;

    lv_obj_t* label = lv_label_create(button, nullptr);
    lv_label_set_text_fmt(label, "%s  %s", selectedShortcuts[i]->name.data(), gestureHints[i]);
    lv_obj_align(label, nullptr, LV_ALIGN_CENTER, 0, 0);

    buttons[i] = button;
  }

  // Baseline wrist orientation for this screen visit; gestures are recognized as a deviation
  // from this rather than an absolute angle, so it works no matter how the wrist was held.
  baselineX = motionController.X();
  baselineY = motionController.Y();
  baselineZ = motionController.Z();
  refreshTask = lv_task_create(RefreshTaskCallback, LV_DISP_DEF_REFR_PERIOD, LV_TASK_PRIO_MID, this);
}

Shortcuts::~Shortcuts() {
  if (refreshTask != nullptr) {
    lv_task_del(refreshTask);
  }
  lv_obj_clean(lv_scr_act());
}

void Shortcuts::OnButtonEvent(lv_obj_t* object) {
  for (uint8_t i = 0; i < buttonCount; i++) {
    if (object == buttons[i]) {
      shortcutService.Trigger(buttonShortcutIds[i]);
      HighlightButton(i);
      return;
    }
  }
}

void Shortcuts::HighlightButton(uint8_t index) {
  if (index >= buttonCount) {
    return;
  }
  lv_obj_set_style_local_bg_color(buttons[index], LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_MAKE(0x20, 0xc0, 0x60));
  highlightRemaining[index] = highlightTicks;
}

void Shortcuts::FireGesture(GestureDirection direction) {
  uint8_t index = static_cast<uint8_t>(direction);
  if (index >= buttonCount) {
    return;
  }
  shortcutService.Trigger(buttonShortcutIds[index]);
  motorController.RunForDuration(20);
  HighlightButton(index);
  gestureArmed = false;
}

void Shortcuts::Refresh() {
  if (buttonCount == 0) {
    return;
  }

  for (uint8_t i = 0; i < buttonCount; i++) {
    if (highlightRemaining[i] > 0) {
      highlightRemaining[i]--;
      if (highlightRemaining[i] == 0) {
        lv_obj_set_style_local_bg_color(buttons[i], LV_BTN_PART_MAIN, LV_STATE_DEFAULT, Colors::bgAlt);
      }
    }
  }

  int16_t pitch = Controllers::MotionController::DegreesRolled(motionController.Y(), motionController.Z(), baselineY, baselineZ);
  int16_t roll = Controllers::MotionController::DegreesRolled(motionController.X(), motionController.Z(), baselineX, baselineZ);

  if (!gestureArmed) {
    // Wrist has to come back near the baseline orientation before another gesture can fire, so
    // holding a tilt doesn't keep re-triggering the same shortcut.
    if (std::abs(pitch) < gestureResetDegrees && std::abs(roll) < gestureResetDegrees) {
      gestureArmed = true;
      gestureHysteresis.fill(0);
    }
    return;
  }

  const std::array<bool, 4> active {
    pitch < -gestureThresholdDegrees,
    pitch > gestureThresholdDegrees,
    roll < -gestureThresholdDegrees,
    roll > gestureThresholdDegrees,
  };

  for (uint8_t i = 0; i < active.size(); i++) {
    if (active[i]) {
      if (gestureHysteresis[i] < gestureSustainTicks) {
        gestureHysteresis[i]++;
      }
    } else {
      gestureHysteresis[i] = 0;
    }
  }

  for (uint8_t i = 0; i < gestureHysteresis.size(); i++) {
    if (gestureHysteresis[i] >= gestureSustainTicks) {
      gestureHysteresis.fill(0);
      FireGesture(static_cast<GestureDirection>(i));
      break;
    }
  }
}
