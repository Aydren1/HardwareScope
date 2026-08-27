#pragma once

namespace hardwarescope {

constexpr int kCommandOpen = 1'001;
constexpr int kCommandToggleOsd = 1'002;
constexpr int kCommandExit = 1'003;
constexpr int kCommandSettings = 1'004;
constexpr int kCommandInstallUpdate = 1'005;
constexpr int kSettingsCheckUpdatesCommand = 3;
constexpr int kSettingsExportCommand = 4;
constexpr int kSettingsImportCommand = 5;
constexpr int kUpdateNowButton = 1'001;
constexpr int kUpdateLaterButton = 1'002;
constexpr int kUpdateIn24HoursRadio = 2'001;
constexpr int kUpdateIn3DaysRadio = 2'002;
constexpr int kUpdateIn1WeekRadio = 2'003;
constexpr int kSkipUpdateRadio = 2'004;
constexpr unsigned int kQueryTooltipVisibleMessage = 0x8000U + 60U;
constexpr unsigned int kArmTooltipTestMessage = 0x8000U + 61U;
constexpr unsigned int kQueryPaintP95Message = 0x8000U + 62U;
constexpr unsigned int kDisableAutomaticUpdateTestMessage = 0x8000U + 63U;
constexpr unsigned int kQuerySensorWorkerRunningMessage = 0x8000U + 64U;
constexpr unsigned int kQueryResumeWaitingMessage = 0x8000U + 65U;
constexpr unsigned int kRestoreTrayIconTestMessage = 0x8000U + 66U;
constexpr unsigned int kQueryTrayIconAddedMessage = 0x8000U + 67U;
constexpr unsigned int kApplyMainDpiTestMessage = 0x8000U + 68U;
constexpr unsigned int kApplySettingsDpiTestMessage = 0x8000U + 69U;
constexpr unsigned int kQuerySnapshotSequenceMessage = 0x8000U + 70U;
constexpr unsigned int kShowUpdatePromptTestMessage = 0x8000U + 71U;
constexpr unsigned int kQueueAutomaticUpdateNotificationTestMessage = 0x8000U + 72U;

} // namespace hardwarescope
