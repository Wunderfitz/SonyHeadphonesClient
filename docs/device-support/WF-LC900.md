WF-LC900 (LinkBuds Clip)
---

Tested on firmware 2.0.3, MDR V2 (protocol `0x03003015`), with both command tables
enabled. The device reports its model name as `LinkBuds Clip` and its series as
`ModelSeries::LINK_BUDS` (`0x60`). Packet capture:
[`tests/WF-LC900-2.0.3/`](../../tests/WF-LC900-2.0.3/).

**NOTE:** **✅**: Supported, ❌: Unsupported, **?**: Untested, **~**: Supported officially, pending implementation.

In this table **✅** means the device advertises the function, libmdr requests it, and the
device answered during initialization. The write/toggle path was not exercised feature by
feature.

| Feature                                  | Status |
|------------------------------------------|--------|
| Current Playing (req. host support)      | ✅      |
| Sound Pressure                           | ✅      |
| Battery Life (L/R + case)                | ✅      |
| Volume                                   | ✅      |
| Track Controls (play/pause, prev/next)   | ✅      |
| NC/AMB Settings                          | ❌      |
| Voice Guidance                           | ✅      |
| Voice Guidance Volume                    | ✅      |
| Multipoint Control (device change)       | ✅      |
| Fix Playback Device (Source Switch)      | ✅      |
| Equalizer                                | ✅      |
| Touch Sensor Gesture                     | ✅      |
| Power Off                                | ✅      |
| DSEE (Upscaling)                         | ✅      |
| Background Music Effect                  | ✅      |
| Bluetooth Connection Quality             | ✅      |
| Cinema Upmix                             | ❌      |
| Pause When Headphones Are Removed        | ❌      |
| Speak to Chat                            | ❌      |
| Head Gesture                             | ❌      |
| Automatic Power Off                      | ❌      |
| Adaptive Volume Control                  | **~**  |
| Sound Leakage Reduction                  | **~**  |
| Voice Contents                           | **~**  |
| Quick Access                             | **~**  |
| Auto Play                                | **~**  |
| Link Auto Switch                         | **~**  |
| Wide Area Tap (repeat-tap training)      | **~**  |

Notes:

- **NC/AMB, Speak to Chat, Head Gesture, Automatic Power Off**: the device advertises no
  corresponding function at all — it is an open-ear design with no noise cancelling.
  libmdr correctly sends no `NCASM` command during the whole session.
- **Cinema Upmix**: libmdr gates both `BGM_MODE_AND_ERRORCODE` and `UPMIX_CINEMA` on the
  single `LISTENING_OPTION` function. This device implements only the BGM half, so the
  `AUDIO_GET_PARAM UPMIX_CINEMA` request is acknowledged but never answered.
- **Pause When Headphones Are Removed**: libmdr requests
  `SYSTEM_GET_PARAM PLAYBACK_CONTROL_BY_WEARING` unconditionally. The device does not
  advertise it and acknowledges without answering.
- The device pushes an unsolicited `PLAY_NTFY_PARAM` immediately after connecting, at a
  point that varies between runs. Landing before `CONNECT_RET_PROTOCOL_INFO` used to abort
  the session, and its effect on the sequence counter used to desynchronize the exchange;
  both are handled as of the capture above.

### Advertised functions

Table 1 (30):

```
CONCIERGE_DATA                                   CONNECTION_STATUS
CODEC_INDICATOR                                  UPSCALING_INDICATOR
BLE_SETUP                                        TUTORIAL_CONTENTS_SELECT_ON_CONCIERGE
UNNECESSARY_AUTO_RECONNECTION                    PHONE_AND_CONNECTED_DEVICE_INFOMATION_FOR_CLASSIC
POWER_OFF                                        TANDEM_KEEP_ALIVE
LR_BATTERY_LEVEL_WITH_THRESHOLD                  CRADLE_BATTERY_LEVEL_WITH_THRESHOLD
FW_UPDATE_MTK_TRANSFER_WITHOUT_DISCONNECTION     PRESET_EQ
FIXED_MESSAGE                                    PLAYBACK_CONTROLLER_WITH_CALL_VOLUME_ADJUSTMENT
GATT_CONNECTABLE                                 INTEGRATED_AUTO_PLAY
ACTION_LOG_NOTIFIER                              GENERAL_SETTING_2
GENERAL_SETTING_3                                CONNECTION_MODE_SOUND_QUALITY_CONNECTION_QUALITY
UPSCALING_AUTO_OFF                               BGM_MODE_SMALL_MIDDLE_LARGE_AND_ERRORCODE
LISTENING_OPTION                                 VOICE_CONTENTS
SOUND_LEAKAGE_REDUCTION                          ASSIGNABLE_SETTING
AUTO_VOLUME                                      QUICK_ACCESS
```

Table 2 (7):

```
SOURCE_SWITCH_CONTROL
PAIRING_DEVICE_MANAGEMENT_WITH_BLUETOOTH_CLASS_OF_DEVICE_CLASSIC_BT
VOICE_GUIDANCE_SETTING_MTK_TRANSFER_WITHOUT_DISCONNECTION_SUPPORT_LANGUAGE_SWITCH_AND_VOLUME_ADJUSTMENT
SAFE_LISTENING_TWS_1
REPEAT_TAP_TRAINING_MODE
QUICK_ACCESS_EASY_SETTING
LINK_AUTO_SWITCH_FOR_HEADSETS
```
