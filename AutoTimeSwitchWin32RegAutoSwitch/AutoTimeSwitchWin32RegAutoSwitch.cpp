// ToggleAutoTime.cpp
// Build with: cl /std:c++20 /EHsc ToggleAutoTime.cpp
#include <windows.h>
#include <iostream>
#include <string>
#include <thread>

static constexpr auto REG_PATH_TIME = R"(SYSTEM\CurrentControlSet\Services\W32Time\Parameters)";
static constexpr auto REG_VAL_TYPE = "Type";
static constexpr auto REG_PATH_TZAUTO = R"(SYSTEM\CurrentControlSet\Services\tzautoupdate)";
static constexpr auto REG_VAL_START = "Start";
static constexpr DWORD TZ_ON = 3;
static constexpr DWORD TZ_OFF = 4;

// Restart W32Time service to apply changes
void restartW32TimeService() {
    SC_HANDLE scm = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) return;
    SC_HANDLE svc = OpenService(scm, TEXT("W32Time"), SERVICE_STOP | SERVICE_START | SERVICE_QUERY_STATUS);
    if (!svc) { CloseServiceHandle(scm); return; }

    SERVICE_STATUS status{};
    // stop
    ControlService(svc, SERVICE_CONTROL_STOP, &status);
    // wait up to 5s for stopped
    for (int i = 0; i < 50; ++i) {
        QueryServiceStatus(svc, &status);
        if (status.dwCurrentState == SERVICE_STOPPED) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    // start
    StartService(svc, 0, nullptr);

    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
}

bool setRegistryString(HKEY root, const char* path, const char* name, const char* value) {
    HKEY hKey;
    if (RegOpenKeyExA(root, path, 0, KEY_SET_VALUE | KEY_WOW64_64KEY, &hKey) != ERROR_SUCCESS)
        return false;
    bool ok = RegSetValueExA(hKey, name, 0, REG_SZ,
        reinterpret_cast<const BYTE*>(value),
        static_cast<DWORD>(strlen(value) + 1)) == ERROR_SUCCESS;
    RegCloseKey(hKey);
    return ok;
}

bool setRegistryDword(HKEY root, const char* path, const char* name, DWORD data) {
    HKEY hKey;
    if (RegOpenKeyExA(root, path, 0, KEY_SET_VALUE | KEY_WOW64_64KEY, &hKey) != ERROR_SUCCESS)
        return false;
    bool ok = RegSetValueExA(hKey, name, 0, REG_DWORD,
        reinterpret_cast<const BYTE*>(&data),
        sizeof(data)) == ERROR_SUCCESS;
    RegCloseKey(hKey);
    return ok;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << "Usage: ToggleAutoTime [on|off]\n";
        return 1;
    }
    std::string cmd = argv[1];
    bool enable = (cmd == "on" || cmd == "1");

    // 1) Toggle automatic time
    const char* typeVal = enable ? "NTP" : "NoSync";
    if (!setRegistryString(HKEY_LOCAL_MACHINE, REG_PATH_TIME, REG_VAL_TYPE, typeVal)) {
        std::cerr << "Error: could not set registry for auto-time.\n";
        return 1;
    }

    // 2) (Optional) Toggle automatic time zone
    DWORD tzVal = enable ? TZ_ON : TZ_OFF;
    setRegistryDword(HKEY_LOCAL_MACHINE, REG_PATH_TZAUTO, REG_VAL_START, tzVal);

    // 3) Restart the Windows Time service so it picks up the change
    restartW32TimeService();

    std::cout << "Automatic time has been " << (enable ? "ENABLED" : "DISABLED") << ".\n";
    return 0;
}
