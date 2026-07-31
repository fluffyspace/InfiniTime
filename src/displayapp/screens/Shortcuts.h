#pragma once

#include <array>
#include <cstdint>
#include <lvgl/lvgl.h>
#include "displayapp/screens/Screen.h"
#include "displayapp/apps/Apps.h"
#include "displayapp/Controllers.h"
#include "Symbols.h"

namespace Pinetime {
  namespace Controllers {
    class ShortcutService;
    class MotionController;
    class MotorController;
  }

  namespace Applications {
    namespace Screens {

      class Shortcuts : public Screen {
      public:
        Shortcuts(Controllers::ShortcutService& shortcutService,
                  Controllers::MotionController& motionController,
                  Controllers::MotorController& motorController);
        ~Shortcuts() override;

        void OnButtonEvent(lv_obj_t* object);
        void Refresh() override;

        // Buttons that can be shown on a single (non-scrolling) screen. The
        // service itself accepts more (ShortcutService::MaxShortcuts); only the
        // first MaxDisplayed populated entries are rendered here. Follow-up:
        // paginate/scroll if the full capacity needs to be visible at once.
        static constexpr uint8_t MaxDisplayed = 4;

        // One gesture direction per displayed button/slot, in display order.
        enum class GestureDirection : uint8_t { TiltUp = 0, TiltDown = 1, TwistIn = 2, TwistOut = 3 };

      private:
        Controllers::ShortcutService& shortcutService;
        Controllers::MotionController& motionController;
        Controllers::MotorController& motorController;

        std::array<lv_obj_t*, MaxDisplayed> buttons {};
        std::array<uint8_t, MaxDisplayed> buttonShortcutIds {};
        uint8_t buttonCount = 0;

        void FireGesture(GestureDirection direction);
        void HighlightButton(uint8_t index);

        lv_task_t* refreshTask = nullptr;

        // Wrist orientation captured when the screen opened; gestures are recognized as an
        // angular deviation from this baseline rather than an absolute orientation, so the
        // feature works regardless of how the wrist happened to be held at the time.
        int16_t baselineX = 0;
        int16_t baselineY = 0;
        int16_t baselineZ = 0;

        static constexpr int16_t gestureThresholdDegrees = 35;
        static constexpr int16_t gestureResetDegrees = 15;
        // The accelerometer itself only updates every ~100ms (SystemTask::UpdateMotion), so this
        // is really "held past the threshold across several distinct sensor samples", not a
        // literal 200ms timer -- refresh ticks are LV_DISP_DEF_REFR_PERIOD (20ms) each.
        static constexpr uint8_t gestureSustainTicks = 10;

        std::array<uint8_t, 4> gestureHysteresis {};
        bool gestureArmed = true; // false right after a gesture fires, until the wrist returns near baseline
        static constexpr uint8_t highlightTicks = 25; // ~500ms at the 20ms refresh period
        std::array<uint8_t, MaxDisplayed> highlightRemaining {};
      };
    }

    template <>
    struct AppTraits<Apps::Shortcuts> {
      static constexpr Apps app = Apps::Shortcuts;
      static constexpr const char* icon = Screens::Symbols::bolt;

      static Screens::Screen* Create(AppControllers& controllers) {
        return new Screens::Shortcuts(*controllers.shortcutService, controllers.motionController, controllers.motorController);
      };

      static bool IsAvailable(Pinetime::Controllers::FS& /*filesystem*/) {
        return true;
      };
    };
  }
}
