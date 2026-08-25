#include "hardwarescope/etw_fps_monitor.hpp"

#include "hardwarescope/game_detector.hpp"

#include <evntcons.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <cwchar>
#include <mutex>
#include <vector>

namespace hardwarescope {
namespace {

constexpr GUID kDxgiProvider{0xCA11C036U, 0x0102U, 0x4A2DU, {0xA6U, 0xADU, 0xF0U, 0x3CU, 0xFEU, 0xD5U, 0xD3U, 0xC9U}};
constexpr GUID kD3d9Provider{0x783ACA0AU, 0x790EU, 0x4D7FU, {0x84U, 0x51U, 0xAAU, 0x85U, 0x05U, 0x11U, 0xC6U, 0xB9U}};
constexpr GUID kDxgKrnlProvider{0x802EC45AU, 0x1E99U, 0x4B83U, {0x99U, 0x20U, 0x87U, 0xC9U, 0x82U, 0x77U, 0xBAU, 0x9DU}};
constexpr std::uint16_t kDxgiPresentStart = 0x002AU;
constexpr std::uint16_t kDxgiPresentMultiplaneOverlayStart = 0x0037U;
constexpr std::uint16_t kD3d9PresentStart = 0x0001U;
constexpr std::uint16_t kDxgKrnlPresentHistoryStart = 0x00ABU;
constexpr std::uint16_t kDxgKrnlPresentInfo = 0x00B8U;
constexpr std::uint64_t kPresentKeyword = 0x8000'0000'0000'0002ULL;
constexpr std::uint64_t kDxgKrnlPresentKeyword = 0x0800'0001ULL;

bool SameGuid(const GUID& left, const GUID& right) noexcept {
    return std::memcmp(&left, &right, sizeof(GUID)) == 0;
}

std::vector<std::byte> TracePropertiesBuffer(const wchar_t* const name) {
    const auto name_bytes = (wcslen(name) + 1U) * sizeof(wchar_t);
    std::vector<std::byte> storage(sizeof(EVENT_TRACE_PROPERTIES) + name_bytes);
    auto* const properties = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(storage.data());
    properties->Wnode.BufferSize = static_cast<ULONG>(storage.size());
    properties->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    properties->Wnode.ClientContext = 1U;
    properties->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
    properties->BufferSize = 64U;
    properties->FlushTimer = 1U;
    properties->MinimumBuffers = 4U;
    properties->MaximumBuffers = 16U;
    properties->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
    std::memcpy(storage.data() + properties->LoggerNameOffset, name, name_bytes);
    return storage;
}

} // namespace

EtwFpsMonitor::~EtwFpsMonitor() {
    Stop();
}

void EtwFpsMonitor::Start(const bool enabled, const bool game_only, const std::uint32_t smoothing_milliseconds) {
    Stop();
    enabled_ = enabled;
    game_only_ = game_only;
    tracker_.SetSmoothingMilliseconds(smoothing_milliseconds);
    if (!enabled_) return;
    controller_thread_ = std::jthread([this](const std::stop_token token) { RunController(token); });
}

void EtwFpsMonitor::Stop() noexcept {
    if (controller_thread_.joinable()) {
        controller_thread_.request_stop();
        controller_thread_.join();
    }
    StopCapture();
    tracker_.Clear();
    enabled_ = false;
}

void EtwFpsMonitor::RunController(const std::stop_token stop_token) noexcept {
    static_cast<void>(SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL));
    std::condition_variable_any wake;
    std::mutex wake_mutex;
    const auto own_process = GetCurrentProcessId();
    while (!stop_token.stop_requested()) {
        GameProcess candidate{};
        if (game_only_) candidate = FindGameProcess(own_process, target_process_id_.load(std::memory_order_acquire));
        const auto desired_process = game_only_ ? candidate.process_id : 0U;
        if (game_only_ && desired_process == 0U) {
            StopCapture();
        } else if (!capture_active_.load(std::memory_order_acquire)
            || desired_process != target_process_id_.load(std::memory_order_acquire)) {
            StartCapture(desired_process, game_only_ ? candidate.application.data() : L"3D application");
        }
        std::unique_lock lock(wake_mutex);
        static_cast<void>(wake.wait_for(lock, stop_token, std::chrono::seconds{1}, [] { return false; }));
    }
}

void EtwFpsMonitor::StartCapture(const std::uint32_t process_id, const wchar_t* const application) {
    StopCapture();
    tracker_.Clear();
    target_process_id_.store(process_id, std::memory_order_release);
    AcquireSRWLockExclusive(&target_lock_);
    static_cast<void>(wcsncpy_s(target_application_.data(), target_application_.size(), application, _TRUNCATE));
    ReleaseSRWLockExclusive(&target_lock_);
    capture_thread_ = std::jthread([this](const std::stop_token token) { CaptureLoop(token); });
}

void EtwFpsMonitor::StopCapture() noexcept {
    if (capture_thread_.joinable()) {
        capture_thread_.request_stop();
        const auto handle = session_handle_.load(std::memory_order_acquire);
        if (handle != 0U) {
            auto storage = TracePropertiesBuffer(session_name_.data());
            static_cast<void>(ControlTraceW(handle, session_name_.data(), reinterpret_cast<EVENT_TRACE_PROPERTIES*>(storage.data()), EVENT_TRACE_CONTROL_STOP));
        }
        capture_thread_.join();
    }
    session_handle_.store(0U, std::memory_order_release);
    capture_active_.store(false, std::memory_order_release);
    target_process_id_.store(0U, std::memory_order_release);
}

void EtwFpsMonitor::CaptureLoop(const std::stop_token stop_token) noexcept {
    static_cast<void>(SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL));
    static_cast<void>(swprintf_s(session_name_.data(), session_name_.size(), L"HardwareScopeNativeFPS-%lu", GetCurrentProcessId()));
    auto storage = TracePropertiesBuffer(session_name_.data());
    auto* const properties = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(storage.data());
    TRACEHANDLE session{};
    if (StartTraceW(&session, session_name_.data(), properties) != ERROR_SUCCESS) return;
    session_handle_.store(session, std::memory_order_release);
    ENABLE_TRACE_PARAMETERS parameters{};
    parameters.Version = ENABLE_TRACE_PARAMETERS_VERSION_2;
    auto status = EnableTraceEx2(session, &kDxgiProvider, EVENT_CONTROL_CODE_ENABLE_PROVIDER, TRACE_LEVEL_VERBOSE, kPresentKeyword, kPresentKeyword, 0U, &parameters);
    if (status == ERROR_SUCCESS) status = EnableTraceEx2(session, &kD3d9Provider, EVENT_CONTROL_CODE_ENABLE_PROVIDER, TRACE_LEVEL_VERBOSE, kPresentKeyword, kPresentKeyword, 0U, &parameters);
    if (status == ERROR_SUCCESS) status = EnableTraceEx2(session, &kDxgKrnlProvider, EVENT_CONTROL_CODE_ENABLE_PROVIDER, TRACE_LEVEL_VERBOSE, kDxgKrnlPresentKeyword, kDxgKrnlPresentKeyword, 0U, &parameters);
    if (status != ERROR_SUCCESS || stop_token.stop_requested()) {
        static_cast<void>(ControlTraceW(session, session_name_.data(), properties, EVENT_TRACE_CONTROL_STOP));
        session_handle_.store(0U, std::memory_order_release);
        return;
    }

    EVENT_TRACE_LOGFILEW log{};
    log.LoggerName = session_name_.data();
    log.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD | PROCESS_TRACE_MODE_RAW_TIMESTAMP;
    log.EventRecordCallback = &EtwFpsMonitor::EventRecordCallback;
    log.Context = this;
    auto trace = OpenTraceW(&log);
    if (trace == INVALID_PROCESSTRACE_HANDLE) {
        static_cast<void>(ControlTraceW(session, session_name_.data(), properties, EVENT_TRACE_CONTROL_STOP));
        session_handle_.store(0U, std::memory_order_release);
        return;
    }
    capture_active_.store(true, std::memory_order_release);
    static_cast<void>(ProcessTrace(&trace, 1U, nullptr, nullptr));
    static_cast<void>(CloseTrace(trace));
    capture_active_.store(false, std::memory_order_release);
    session_handle_.store(0U, std::memory_order_release);
}

void WINAPI EtwFpsMonitor::EventRecordCallback(EVENT_RECORD* const event) noexcept {
    if (event == nullptr || event->UserContext == nullptr) return;
    static_cast<EtwFpsMonitor*>(event->UserContext)->HandleEvent(*event);
}

void EtwFpsMonitor::HandleEvent(const EVENT_RECORD& event) noexcept {
    const auto id = event.EventHeader.EventDescriptor.Id;
    const auto target = target_process_id_.load(std::memory_order_acquire);
    const auto dxgi = SameGuid(event.EventHeader.ProviderId, kDxgiProvider)
        && (id == kDxgiPresentStart || id == kDxgiPresentMultiplaneOverlayStart);
    const auto d3d9 = SameGuid(event.EventHeader.ProviderId, kD3d9Provider) && id == kD3d9PresentStart;
    const auto kernel = SameGuid(event.EventHeader.ProviderId, kDxgKrnlProvider)
        && (id == kDxgKrnlPresentHistoryStart || id == kDxgKrnlPresentInfo);
    if (!dxgi && !d3d9 && !kernel) return;
    const auto process_id = event.EventHeader.ProcessId;
    if (process_id == 0U || process_id == GetCurrentProcessId() || (target != 0U && process_id != target)) return;
    std::uint64_t swap_chain = kernel ? 1U : 0U;
    if (!kernel && event.UserData != nullptr && event.UserDataLength >= sizeof(swap_chain)) std::memcpy(&swap_chain, event.UserData, sizeof(swap_chain));
    LARGE_INTEGER frequency{};
    LARGE_INTEGER callback{};
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&callback);
    if (frequency.QuadPart <= 0) return;
    const auto event_milliseconds = static_cast<double>(event.EventHeader.TimeStamp.QuadPart) * 1'000.0 / static_cast<double>(frequency.QuadPart);
    std::array<wchar_t, 64U> application{};
    AcquireSRWLockShared(&target_lock_);
    application = target_application_;
    ReleaseSRWLockShared(&target_lock_);
    tracker_.RecordPresent(process_id, swap_chain, event_milliseconds, callback.QuadPart, application.data(), game_only_);
}

FrameRateReading EtwFpsMonitor::Snapshot() const noexcept {
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    DWORD foreground_process{};
    static_cast<void>(GetWindowThreadProcessId(GetForegroundWindow(), &foreground_process));
    return tracker_.Snapshot(now.QuadPart, foreground_process, game_only_);
}

} // namespace hardwarescope
