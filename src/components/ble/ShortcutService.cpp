#include "components/ble/ShortcutService.h"
#include "components/ble/NimbleController.h"
#include <algorithm>
#include <cstring>
#include <nrf_log.h>

using namespace Pinetime::Controllers;

namespace {
  int ShortcutTriggerCallback(uint16_t /*connHandle*/, uint16_t /*attrHandle*/, struct ble_gatt_access_ctxt* ctxt, void* /*arg*/) {
    // Read-back is not meaningful for a trigger characteristic; report "no shortcut" (0).
    uint8_t buffer[1] = {0};
    int res = os_mbuf_append(ctxt->om, buffer, 1);
    return (res == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
  }
}

int ShortcutListCallback(uint16_t /*connHandle*/, uint16_t /*attrHandle*/, struct ble_gatt_access_ctxt* ctxt, void* arg) {
  return static_cast<ShortcutService*>(arg)->OnShortcutListWrite(ctxt);
}

ShortcutService::ShortcutService(NimbleController& nimble)
  : nimble {nimble},
    characteristicDefinition {{.uuid = &shortcutTriggerCharUuid.u,
                               .access_cb = ShortcutTriggerCallback,
                               .arg = this,
                               .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                               .val_handle = &shortcutTriggerHandle},
                              {.uuid = &shortcutListCharUuid.u,
                               .access_cb = ShortcutListCallback,
                               .arg = this,
                               .flags = BLE_GATT_CHR_F_WRITE,
                               .val_handle = &shortcutListHandle},
                              {0}},
    serviceDefinition {
      {.type = BLE_GATT_SVC_TYPE_PRIMARY, .uuid = &shortcutServiceUuid.u, .characteristics = characteristicDefinition},
      {0},
    } {
}

void ShortcutService::Init() {
  ble_gatts_count_cfg(serviceDefinition);
  ble_gatts_add_svcs(serviceDefinition);
}

void ShortcutService::Trigger(uint8_t shortcutId) {
  if (!shortcutTriggerNotificationEnabled) {
    return;
  }

  uint16_t connectionHandle = nimble.connHandle();
  if (connectionHandle == 0 || connectionHandle == BLE_HS_CONN_HANDLE_NONE) {
    return;
  }

  uint8_t buffer[1] = {shortcutId};
  auto* om = ble_hs_mbuf_from_flat(buffer, 1);
  ble_gattc_notify_custom(connectionHandle, shortcutTriggerHandle, om);
  NRF_LOG_INFO("Shortcut triggered : id=%d", shortcutId);
}

void ShortcutService::SubscribeNotification(uint16_t attributeHandle) {
  if (attributeHandle == shortcutTriggerHandle) {
    shortcutTriggerNotificationEnabled = true;
  }
}

void ShortcutService::UnsubscribeNotification(uint16_t attributeHandle) {
  if (attributeHandle == shortcutTriggerHandle) {
    shortcutTriggerNotificationEnabled = false;
  }
}

int ShortcutService::OnShortcutListWrite(struct ble_gatt_access_ctxt* ctxt) {
  static constexpr size_t headerSize = 3; // messageType, version, count
  static constexpr size_t entryHeaderSize = 3; // id, nameLen, flags
  static constexpr size_t maxMessageSize = headerSize + MaxShortcuts * (entryHeaderSize + MaxNameLength);
  static constexpr uint8_t flagCloseOnTrigger = 0x01;

  size_t messageSize = OS_MBUF_PKTLEN(ctxt->om);
  if (messageSize < headerSize || messageSize > maxMessageSize) {
    NRF_LOG_INFO("Shortcut list : invalid message size %d", messageSize);
    return 0;
  }

  uint8_t buffer[maxMessageSize];
  os_mbuf_copydata(ctxt->om, 0, messageSize, buffer);

  uint8_t messageType = buffer[0];
  uint8_t version = buffer[1];
  if (messageType != 0 || version != 1) {
    NRF_LOG_INFO("Shortcut list : unsupported messageType=%d version=%d", messageType, version);
    return 0;
  }

  uint8_t count = std::min(buffer[2], MaxShortcuts);

  std::array<std::optional<Shortcut>, MaxShortcuts> newShortcuts;
  size_t offset = headerSize;
  for (uint8_t i = 0; i < count; i++) {
    if (offset + entryHeaderSize > messageSize) {
      break;
    }
    uint8_t id = buffer[offset];
    uint8_t nameLen = std::min(buffer[offset + 1], MaxNameLength);
    uint8_t flags = buffer[offset + 2];
    offset += entryHeaderSize;

    if (offset + nameLen > messageSize) {
      break;
    }

    Name name {};
    std::memcpy(name.data(), &buffer[offset], nameLen);
    name[nameLen] = '\0';
    offset += nameLen;

    newShortcuts[i] = Shortcut {id, name, (flags & flagCloseOnTrigger) != 0};
  }

  shortcuts = newShortcuts;
  NRF_LOG_INFO("Shortcut list updated : %d entries", count);
  return 0;
}
