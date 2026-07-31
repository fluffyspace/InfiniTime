#pragma once

#include <array>
#include <cstdint>
#include <atomic>
#include <optional>

#define min // workaround: nimble's min/max macros conflict with libstdc++
#define max
#include <host/ble_gap.h>
#include <host/ble_uuid.h>
#undef max
#undef min

int ShortcutListCallback(uint16_t connHandle, uint16_t attrHandle, struct ble_gatt_access_ctxt* ctxt, void* arg);

namespace Pinetime {
  namespace Controllers {
    class NimbleController;

    // Lets the watch notify the companion app (Gadgetbridge) that a user-configured
    // shortcut was triggered (app tap, button pattern, gesture, ...). The companion
    // app is responsible for mapping the shortcut id to an action (e.g. a Home
    // Assistant call relayed through MacroDroid/Tasker). The companion app also
    // pushes the configured shortcut list down to the watch over a second
    // characteristic, so it can be displayed without hardcoding it in firmware.
    class ShortcutService {
    public:
      explicit ShortcutService(NimbleController& nimble);

      void Init();

      // Sends shortcutId to the companion app if it is currently subscribed.
      void Trigger(uint8_t shortcutId);

      void SubscribeNotification(uint16_t attributeHandle);
      void UnsubscribeNotification(uint16_t attributeHandle);

      int OnShortcutListWrite(struct ble_gatt_access_ctxt* ctxt);

      static constexpr uint8_t MaxShortcuts = 8;
      static constexpr uint8_t MaxNameLength = 16;

      using Name = std::array<char, MaxNameLength + 1>; // +1 for '\0'

      struct Shortcut {
        uint8_t id;
        Name name;
      };

      [[nodiscard]] const std::array<std::optional<Shortcut>, MaxShortcuts>& GetShortcuts() const {
        return shortcuts;
      }

    private:
      // 0006yyxx-78fc-48fe-8e23-433b3a1942d0
      static constexpr ble_uuid128_t CharUuid(uint8_t x, uint8_t y) {
        return ble_uuid128_t {.u = {.type = BLE_UUID_TYPE_128},
                              .value = {0xd0, 0x42, 0x19, 0x3a, 0x3b, 0x43, 0x23, 0x8e, 0xfe, 0x48, 0xfc, 0x78, y, x, 0x06, 0x00}};
      }

      NimbleController& nimble;

      ble_uuid128_t shortcutServiceUuid {CharUuid(0x00, 0x00)};
      ble_uuid128_t shortcutTriggerCharUuid {CharUuid(0x00, 0x01)};
      ble_uuid128_t shortcutListCharUuid {CharUuid(0x00, 0x02)};

      struct ble_gatt_chr_def characteristicDefinition[3];
      struct ble_gatt_svc_def serviceDefinition[2];

      uint16_t shortcutTriggerHandle {};
      uint16_t shortcutListHandle {};
      std::atomic_bool shortcutTriggerNotificationEnabled {false};

      std::array<std::optional<Shortcut>, MaxShortcuts> shortcuts;
    };
  }
}
