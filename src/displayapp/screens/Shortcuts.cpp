#include "displayapp/screens/Shortcuts.h"
#include "displayapp/InfiniTimeTheme.h"
#include "components/ble/ShortcutService.h"

using namespace Pinetime::Applications::Screens;

namespace {
  void ButtonEventHandler(lv_obj_t* obj, lv_event_t event) {
    if (event != LV_EVENT_CLICKED) {
      return;
    }
    auto* screen = static_cast<Shortcuts*>(obj->user_data);
    screen->OnButtonEvent(obj);
  }
}

Shortcuts::Shortcuts(Controllers::ShortcutService& shortcutService) : shortcutService {shortcutService} {
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
    lv_label_set_text_fmt(label, "%s", selectedShortcuts[i]->name.data());
    lv_obj_align(label, nullptr, LV_ALIGN_CENTER, 0, 0);

    buttons[i] = button;
  }
}

Shortcuts::~Shortcuts() {
  lv_obj_clean(lv_scr_act());
}

void Shortcuts::OnButtonEvent(lv_obj_t* object) {
  for (uint8_t i = 0; i < buttonCount; i++) {
    if (object == buttons[i]) {
      shortcutService.Trigger(buttonShortcutIds[i]);
      return;
    }
  }
}
