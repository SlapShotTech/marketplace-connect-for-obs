#include <string>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#include <algorithm>

#define BUFFER_SIZE 8000

void ToLowerCase(std::wstring &str)
{
        std::transform(str.begin(), str.end(), str.begin(), ::towlower);
}

bool IsObs64Running()
{
        HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hProcessSnap == INVALID_HANDLE_VALUE) {
                return false;
        }

        PROCESSENTRY32 processEntry;
        processEntry.dwSize = sizeof(PROCESSENTRY32);

        if (Process32First(hProcessSnap, &processEntry)) {
                do {
                        std::wstring processName(processEntry.szExeFile);
                        ToLowerCase(processName);

                        if (processName == L"obs64.exe") {
                                CloseHandle(hProcessSnap);
                                return true;
                        }
                } while (Process32Next(hProcessSnap, &processEntry));
        }

        CloseHandle(hProcessSnap);
        return false;
}

DWORD GetProcessIdFromExe(const std::wstring &exeName)
{
        PROCESSENTRY32 processEntry;
        processEntry.dwSize = sizeof(PROCESSENTRY32);

        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot == INVALID_HANDLE_VALUE) {
                std::cerr << "[ERROR]: Error taking snapshot of processes." << std::endl;
                return 0;
        }

        if (Process32First(hSnapshot, &processEntry)) {
                do {
                        if (exeName == processEntry.szExeFile) {
                                CloseHandle(hSnapshot);
                                return processEntry.th32ProcessID;
                        }
                } while (Process32Next(hSnapshot, &processEntry));
        }

        CloseHandle(hSnapshot);
        return 0;
}

BOOL CALLBACK WindowToForeground(HWND hwnd, LPARAM lParam)
{
        DWORD processId;
        GetWindowThreadProcessId(hwnd, &processId);

        if (processId == lParam) {
                wchar_t windowTitle[256];
                GetWindowTextW(hwnd, windowTitle,
                               sizeof(windowTitle) / sizeof(wchar_t));
                std::wstring title = windowTitle;
                if (title.rfind(L"OBS ", 0) == 0) {
                        bool maximized = IsZoomed(hwnd);
                        bool minimized = IsIconic(hwnd);
                        if (!minimized) {
                                ShowWindow(hwnd,
                                           maximized ? SW_SHOWMAXIMIZED : SW_SHOW);
                        } else {
                                ShowWindow(hwnd, SW_RESTORE);
                        }
                        SetForegroundWindow(hwnd);
                        return FALSE;
                }
        }
        return TRUE;
}

int send_auth_to_obs(std::string payload)
{
        int pipe_number = 0;
        std::string pipe_name = "elgato_cloud";
        std::string base_name = "\\\\.\\pipe\\" + pipe_name;
        std::string attempt_name;
        HANDLE pipe = INVALID_HANDLE_VALUE;
        SECURITY_ATTRIBUTES sa;
        SECURITY_DESCRIPTOR sd;
        InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
        SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE);
        sa.nLength = sizeof(sa);
        sa.lpSecurityDescriptor = &sd;
        sa.bInheritHandle = FALSE;

        int connect_attempts_remaining = 6;
        std::cout << "Attempting to connect to Marketplace Connect Plugin"
                  << std::flush;
        while (connect_attempts_remaining-- > 0 &&
               pipe == INVALID_HANDLE_VALUE) {
                pipe_number = 0;
                while (pipe_number < 10) {
                        attempt_name = base_name + std::to_string(pipe_number);
                        std::cout << "." << std::flush;
                        pipe = CreateFileA(attempt_name.c_str(), GENERIC_WRITE, 0,
                                           &sa, OPEN_EXISTING, 0, NULL);
                        if (pipe != INVALID_HANDLE_VALUE) {
                                std::cout << "\nSuccess" << std::endl;
                                break;
                        }
                        pipe_number++;
                }
                if (pipe == INVALID_HANDLE_VALUE) {
                        Sleep(2000);
                }
        }
        if (pipe == INVALID_HANDLE_VALUE) {
                std::cerr << "[ERROR] Could not find connection Marketplace Connect Plugin."
                          << std::endl;
                return 1;
        }
        DWORD mode = PIPE_READMODE_MESSAGE;
        auto success = SetNamedPipeHandleState(pipe, &mode, NULL, NULL);
        if (!success) {
                CloseHandle(pipe);
                std::cerr << "[ERROR] Could not negotiate connection with Marketplace Connect Plugin."
                          << std::endl;
                return 1;
        }

        DWORD written = 0;
        success = WriteFile(pipe, payload.c_str(),
                            static_cast<DWORD>(payload.size()), &written, NULL);
        if (!success || written < payload.size()) {
                std::cerr << "[ERROR] Could not send data to Marketplace Connect Plugin."
                          << std::endl;
                CloseHandle(pipe);
                return 1;
        }

        CloseHandle(pipe);
        return 0;
}

int open_obs_mp_window()
{
        int pipe_number = 0;
        std::string pipe_name = "elgato_cloud";
        std::string base_name = "\\\\.\\pipe\\" + pipe_name;
        std::string attempt_name;
        HANDLE pipe = INVALID_HANDLE_VALUE;
        SECURITY_ATTRIBUTES sa;
        SECURITY_DESCRIPTOR sd;
        InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
        SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE);
        sa.nLength = sizeof(sa);
        sa.lpSecurityDescriptor = &sd;
        sa.bInheritHandle = FALSE;

        int connect_attempts_remaining = 60;

        std::cout << "Waiting for OBS to launch" << std::flush;

        while (connect_attempts_remaining-- > 0 &&
               pipe == INVALID_HANDLE_VALUE) {
                pipe_number = 0;
                attempt_name = base_name + std::to_string(pipe_number);
                std::cout << "." << std::flush;
                pipe = CreateFileA(attempt_name.c_str(), GENERIC_WRITE, 0, &sa,
                                   OPEN_EXISTING, 0, NULL);
                if (pipe != INVALID_HANDLE_VALUE) {
                        std::cout << "\nConnected To OBS" << std::endl;
                        break;
                }
                if (pipe == INVALID_HANDLE_VALUE) {
                        Sleep(1000);
                }
        }
        if (pipe == INVALID_HANDLE_VALUE) {
                std::cerr << "[ERROR] Could not find connection Marketplace Connect Plugin."
                          << std::endl;
                return 1;
        }
        DWORD mode = PIPE_READMODE_MESSAGE;
        auto success = SetNamedPipeHandleState(pipe, &mode, NULL, NULL);
        if (!success) {
                CloseHandle(pipe);
                std::cerr << "[ERROR] Could not negotiate connection with Marketplace Connect Plugin."
                          << std::endl;
                return 1;
        }

        DWORD written = 0;
        std::string payload = "elgatolink://open";
        success = WriteFile(pipe, payload.c_str(),
                            static_cast<DWORD>(payload.size()), &written, NULL);
        if (!success || written < payload.size()) {
                std::cerr << "[ERROR] Could not send data to Marketplace Connect Plugin."
                          << std::endl;
                CloseHandle(pipe);
                return 1;
        }

        CloseHandle(pipe);
        return 0;
}

int launch_obs()
{
        DWORD buffer_size = BUFFER_SIZE;
        wchar_t buffer[BUFFER_SIZE];

        std::wstring config_path;
        std::wstring launch_path;

        buffer_size = BUFFER_SIZE;
        auto status = RegGetValueW(HKEY_CURRENT_USER, L"SOFTWARE\\elgato\\obs",
                                   L"", RRF_RT_REG_SZ, nullptr, buffer,
                                   &buffer_size);
        if (status == ERROR_SUCCESS) {
                launch_path = std::wstring(buffer, buffer_size);
        }

        bool running = IsObs64Running();

        if (!running && launch_path.size() > 0) {
                auto wd = launch_path.substr(0,
                                             launch_path.find_last_of(L"\\/"));

                STARTUPINFOW si = {sizeof(STARTUPINFO)};
                PROCESS_INFORMATION pi;

                if (CreateProcess(launch_path.c_str(), NULL, NULL, NULL, FALSE, 0,
                                  NULL, wd.c_str(), &si, &pi) == 0) {
                        std::cerr << "[ERROR] Failed to launch OBS." << std::endl;
                        return 1;
                }

                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);

                return 0;
        } else if (running) {
                DWORD processId = GetProcessIdFromExe(L"obs64.exe");
                if (processId == 0) {
                        std::cerr << "[ERROR] OBS appears to have quit."
                                  << std::endl;
                        return 1;
                }
                EnumWindows(WindowToForeground, processId);
        }
        return 0;
}

bool obs_is_running(std::wstring name)
{
        for (size_t i = 0; i < name.size(); ++i) {
                if (!iswalnum(name[i]))
                        name[i] = L'_';
        }
        HANDLE h = OpenMutexW(SYNCHRONIZE, false, name.c_str());
        return !!h;
}

int main(int argc, char *argv[])
{
        if (argc < 2) {
                std::cerr << "[ERROR] No argument provided." << std::endl;
                system("pause");
                return 1;
        }
        int resp;

        std::string payload = argv[1];

        if (payload.find("elgatolink://auth") == 0) {
                resp = send_auth_to_obs(payload);
        } else {
                resp = launch_obs();
                if (resp == 0)
                        resp = open_obs_mp_window();
        }

        if (resp == 1) {
                system("pause");
        }

        return resp;
}

#elif defined(__APPLE__)

#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>
#include <cerrno>
#include <cstdlib>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace {
constexpr const char *kPipeName = "elgato_cloud";
constexpr int kMaxPipeIndex = 10;
const std::chrono::seconds kAuthRetryDelay(2);
const std::chrono::seconds kOpenRetryDelay(1);

std::string socket_path_for_index(int index)
{
        return std::string("/tmp/") + kPipeName + std::to_string(index);
}

int connect_to_socket(const std::string &path)
{
        int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) {
                return -1;
        }

        sockaddr_un addr = {};
        addr.sun_family = AF_UNIX;
        if (path.size() >= sizeof(addr.sun_path)) {
                close(fd);
                return -1;
        }
        std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

        if (::connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == 0) {
                return fd;
        }

        close(fd);
        return -1;
}

bool send_all(int fd, const std::string &payload)
{
        const char *data = payload.data();
        size_t remaining = payload.size();
        while (remaining > 0) {
                ssize_t written = ::write(fd, data, remaining);
                if (written < 0) {
                        if (errno == EINTR) {
                                continue;
                        }
                        return false;
                }

                data += written;
                remaining -= static_cast<size_t>(written);
        }
        return true;
}

bool is_obs_running()
{
        FILE *handle = popen("pgrep -x OBS", "r");
        if (!handle) {
                return false;
        }

        char buffer[16];
        bool running = fgets(buffer, sizeof(buffer), handle) != nullptr;
        pclose(handle);
        return running;
}
} // namespace

int send_auth_to_obs(std::string payload)
{
        int attempts_remaining = 6;
        std::cout << "Attempting to connect to Marketplace Connect Plugin"
                  << std::flush;
        while (attempts_remaining-- > 0) {
                for (int pipe_number = 0; pipe_number < kMaxPipeIndex;
                     ++pipe_number) {
                        int fd = connect_to_socket(
                                socket_path_for_index(pipe_number));
                        if (fd >= 0) {
                                std::cout << "\nSuccess" << std::endl;
                                bool success = send_all(fd, payload);
                                close(fd);
                                if (!success) {
                                        std::cerr << "[ERROR] Could not send data to Marketplace Connect Plugin."
                                                  << std::endl;
                                        return 1;
                                }
                                return 0;
                        }
                        std::cout << "." << std::flush;
                }
                if (attempts_remaining >= 0) {
                        std::this_thread::sleep_for(kAuthRetryDelay);
                }
        }
        std::cout << std::endl;
        std::cerr << "[ERROR] Could not find connection Marketplace Connect Plugin."
                  << std::endl;
        return 1;
}

int open_obs_mp_window()
{
        int attempts_remaining = 60;
        std::cout << "Waiting for OBS to launch" << std::flush;
        while (attempts_remaining-- > 0) {
                for (int pipe_number = 0; pipe_number < kMaxPipeIndex;
                     ++pipe_number) {
                        int fd = connect_to_socket(
                                socket_path_for_index(pipe_number));
                        if (fd >= 0) {
                                std::cout << "\nConnected To OBS" << std::endl;
                                std::string payload = "elgatolink://open";
                                bool success = send_all(fd, payload);
                                close(fd);
                                if (!success) {
                                        std::cerr << "[ERROR] Could not send data to Marketplace Connect Plugin."
                                                  << std::endl;
                                        return 1;
                                }
                                return 0;
                        }
                }
                std::cout << "." << std::flush;
                if (attempts_remaining >= 0) {
                        std::this_thread::sleep_for(kOpenRetryDelay);
                }
        }
        std::cout << std::endl;
        std::cerr << "[ERROR] Could not find connection Marketplace Connect Plugin."
                  << std::endl;
        return 1;
}

int launch_obs()
{
        if (!is_obs_running()) {
                int status = system("open -a \"OBS\" >/dev/null 2>&1");
                if (status != 0) {
                        std::cerr << "[ERROR] Failed to launch OBS." << std::endl;
                        return 1;
                }
                return 0;
        }

        int status = system(
                "osascript -e 'tell application \"OBS\" to activate' >/dev/null 2>&1");
        if (status != 0) {
                std::cerr << "[WARN] Could not bring OBS to the foreground."
                          << std::endl;
        }
        return 0;
}

int main(int argc, char *argv[])
{
        if (argc < 2) {
                std::cerr << "[ERROR] No argument provided." << std::endl;
                return 1;
        }

        int resp = 0;
        std::string payload = argv[1];

        if (payload.find("elgatolink://auth") == 0) {
                resp = send_auth_to_obs(payload);
        } else {
                resp = launch_obs();
                if (resp == 0) {
                        resp = open_obs_mp_window();
                }
        }

        return resp;
}

#else

int main()
{
        std::cerr << "Marketplace Connect loader is only implemented for Windows and macOS." << std::endl;
        return 1;
}

#endif
