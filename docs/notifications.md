# Notifications

Temporary messages appear above rows and free-layout dashboards. They do not
replace the saved dashboard or scene. Values and automatic page rotation keep
updating underneath. Page transition effects are skipped while a notification
is visible, so they cannot overwrite the overlay. Manual navigation still works.

## Send through the local API

`POST /api/v1/notifications` is password protected by default. It accepts the
panel/API password through HTTP Basic authentication or a Bearer token.

```json
{
  "title": "Charging complete",
  "message": "The battery is ready",
  "icon": "check",
  "severity": "success",
  "durationSeconds": 12
}
```

Provide `title`, `message`, or both. Other fields are optional:

| Field | Default | Values |
| --- | --- | --- |
| `title` | Empty | Up to 96 UTF-8 bytes |
| `message` | Empty | Up to 384 UTF-8 bytes |
| `icon` | `auto` | `auto`, `none`, `bell`, `info`, `check`, `warning`, `error`, `power`, `door` |
| `severity` | `info` | `info`, `success`, `warning`, `error`, `critical` |
| `durationSeconds` | `8` | Whole seconds from 1 to 300 |
| `position` | Display setting | An advertised position |

Text wraps within the panel. Excess text ends with an ellipsis rather than
overflowing. The built-in Inter Tight font includes Latin extended characters.
Icons are built-in drawings, not remote images or arbitrary SVG. Severity changes
the accent and automatic icon, not arrival order.

Accepted requests return `202`. Invalid fields return `422`, malformed JSON
returns `400`, and a request body over 4096 bytes returns `413`. The queue holds
three messages including visible and waiting messages. A full queue returns `429`; insufficient
rendering memory returns `503`. Neither replaces an existing message.

`DELETE /api/v1/notifications` dismisses all visible and waiting messages.
It uses the same authentication. Messages are temporary and do not survive reboot.
An off screen (or zero brightness) rejects new messages with `409`, without
waking the screen. This is a display feature, not a guaranteed alarm channel.

## Display settings

Device web panel: **Display > Notifications > Default position**. Save to persist
the setting. **Test notification** previews the selected position without saving it.

**Visible at once** sets a limit of 1-3 messages, with 3 as the default. Each visible
message has its own timer and disappears independently. At a limit of 1, messages
are displayed sequentially. Waiting time does not consume their visible duration.
Remaining messages move into the available space. Text wraps or uses an ellipsis
to keep the stack within the screen, with more compact typography when needed.

**Protect notification API** controls authentication for sending and clearing
notifications only. It defaults to on and remains independent of panel protection.
When enabled, a panel/API password must be configured in **Security**. Without one,
requests are rejected with `403`, never accepted with an empty password. When
disabled, anyone who can reach the device can send and clear notifications.
Other endpoint permissions are unchanged. Setup mode blocks notification endpoints
regardless of this toggle. HTTP does not encrypt credentials on the network.

Save settings to persist placement, visible count and protection across restarts.
The API equivalent is `PUT /api/v1/display` with `notificationPosition`,
`notificationMaxVisible` (integer 1-3) and `notificationAuthEnabled` (boolean).
These non-secret settings are also exposed in `GET /api/v1/status`.

Small displays support `top` and `bottom`. Screens at least 320 pixels wide and
240 pixels high also expose `left`, `right`, `top_left`, `top_right`, `bottom_left`
and `bottom_right`. Capabilities come from `notificationPositions` in
`GET /api/v1/status`; `notificationPosition` is the saved default.

Messages slide in and out from their edge. Corners use the top or bottom edge.
The animation obeys the display refresh limit. Slow refresh rates skip the slide.
The duration starts after a fully visible frame is drawn, not when the API receives
the request. One small compositor buffer is reused; no full-screen bitmap is held.

## Home Assistant

The device has a **Notification position** configuration selector. It lists only
positions advertised by that display. Automations use the **Show notification**
action and select a Mini-Display device:

```yaml
action: mini_display.notify
data:
  device_id: your_display_device_id
  title: Charging complete
  message: The battery is ready
  icon: check
  severity: success
  duration: 12
```

Omit `position` to use the device default. **Dismiss notifications**
(`mini_display.dismiss_notifications`) takes the same `device_id`.

Notifications use the existing local API connection, independently of periodic
sensor batching. A failed notification request is not automatically replayed,
because a timeout can happen after the display has already accepted it.
