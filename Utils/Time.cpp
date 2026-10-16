#include "Time.hpp"

void Time::Sleep(unsigned long long intervals) {
    HANDLE hSleepTimer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (!hSleepTimer) { return; }

    LARGE_INTEGER dueTime{};
    dueTime.QuadPart = intervals * -1LL;

    if (SetWaitableTimer(hSleepTimer, &dueTime, 0, NULL, NULL, FALSE)) {
        WaitForSingleObject(hSleepTimer, INFINITE);
    }

    CloseHandle(hSleepTimer);
}


HANDLE Sync::handles[2] = { nullptr };

bool Sync::Initialize() {
    handles[0] = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!handles[0]) { return false; }

    handles[1] = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!handles[1]) { return false; }

    return true;
}

void Sync::SyncVideo() {
    if (Config::user.audio.captureSys || Config::user.audio.captureMic) {
        SetEvent(handles[0]);
        WaitForMultipleObjects(2, handles, TRUE, INFINITE);
        ResetEvent(handles[0]);
    }
}

void Sync::SyncAudio() {
    SetEvent(handles[1]);
    WaitForMultipleObjects(2, handles, TRUE, INFINITE);
    ResetEvent(handles[1]);
}