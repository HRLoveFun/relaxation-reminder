#pragma once

#include <windows.h>

#include <string>

namespace rr {

enum class UpdateStatus {
    UpToDate,
    NewVersion,
    Failed,
};

struct UpdateInfo {
    UpdateStatus status = UpdateStatus::Failed;
    std::wstring current_version;
    std::wstring latest_version;
};

// Queries the GitHub latest release. Blocking; call it off the UI thread.
UpdateInfo CheckForUpdate();

std::wstring BuildUpdatePrompt(const std::wstring& current_version,
                               const std::wstring& latest_version);

bool CopyTextToClipboard(HWND owner, const std::wstring& text);

}  // namespace rr
