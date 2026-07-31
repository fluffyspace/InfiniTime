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
  }

  namespace Applications {
    namespace Screens {

      class Shortcuts : public Screen {
      public:
        explicit Shortcuts(Controllers::ShortcutService& shortcutService);
        ~Shortcuts() override;

        void OnButtonEvent(lv_obj_t* object);

        // Buttons that can be shown on a single (non-scrolling) screen. The
        // service itself accepts more (ShortcutService::MaxShortcuts); only the
        // first MaxDisplayed populated entries are rendered here. Follow-up:
        // paginate/scroll if the full capacity needs to be visible at once.
        static constexpr uint8_t MaxDisplayed = 4;

      private:
        Controllers::ShortcutService& shortcutService;

        std::array<lv_obj_t*, MaxDisplayed> buttons {};
        std::array<uint8_t, MaxDisplayed> buttonShortcutIds {};
        uint8_t buttonCount = 0;
      };
    }

    template <>
    struct AppTraits<Apps::Shortcuts> {
      static constexpr Apps app = Apps::Shortcuts;
      static constexpr const char* icon = Screens::Symbols::bolt;

      static Screens::Screen* Create(AppControllers& controllers) {
        return new Screens::Shortcuts(*controllers.shortcutService);
      };

      static bool IsAvailable(Pinetime::Controllers::FS& /*filesystem*/) {
        return true;
      };
    };
  }
}
