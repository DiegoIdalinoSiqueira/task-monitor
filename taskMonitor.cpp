#include <thread>
#include <iostream>
#include <fstream>
#include <Windows.h>
#include <psapi.h>
#include <string>
#include <map>
#include <mutex>
#include <vector>
#include <codecvt>
#include <locale>
#include <shobjidl.h>

HHOOK hook = NULL;
std::mutex block;
std::vector<std::thread> threadsVector;

std::map<std::string, int> tasksTimeout;
std::map<std::wstring, std::map<DWORD, int>> mostRecentInteractionTaskTime;
std::map<int, std::wstring> tasksOnMonitoring;

struct PIDexe
{
    std::wstring exe;
    DWORD PID;
};

PIDexe getCurrentForegroundWindow();

LRESULT CALLBACK globalListenKeyboardCallback(int nCodigo, WPARAM wParam, LPARAM lParam)
{
    if (nCodigo >= 0 && wParam == WM_KEYDOWN)
    {
        PIDexe PidExeForegroundWindow = getCurrentForegroundWindow();

        if (PidExeForegroundWindow.exe != L"" && PidExeForegroundWindow.PID != 0)
        {
            block.lock();
            ++mostRecentInteractionTaskTime[PidExeForegroundWindow.exe][PidExeForegroundWindow.PID];
            block.unlock();
        }
    }
    return CallNextHookEx(hook, nCodigo, wParam, lParam);
}

int globalListenKeyboard()
{
    hook = SetWindowsHookEx(WH_KEYBOARD_LL, globalListenKeyboardCallback, GetModuleHandle(NULL), 0);
    MSG msg;

    while (GetMessage(&msg, NULL, 0, 0) != 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    UnhookWindowsHookEx(hook);
    return 0;
}

LRESULT CALLBACK globalListenMouseCallback(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0 && (wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN))
    {
        PIDexe PidExeForegroundWindow = getCurrentForegroundWindow();

        if (PidExeForegroundWindow.exe != L"" && PidExeForegroundWindow.PID != 0)
        {
            block.lock();
            ++mostRecentInteractionTaskTime[PidExeForegroundWindow.exe][PidExeForegroundWindow.PID];
            block.unlock();
        }
    }
    return CallNextHookEx(hook, nCode, wParam, lParam);
}

int globalListenMouse()
{
    hook = SetWindowsHookEx(WH_MOUSE_LL, globalListenMouseCallback, GetModuleHandle(NULL), 0);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) != 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    UnhookWindowsHookEx(hook);
    return 0;
}

PIDexe getCurrentForegroundWindow()
{
    HWND foregroundWindow = GetForegroundWindow();
    PIDexe pidexe;

    if (foregroundWindow != NULL)
    {
        wchar_t windowTitle[512];
        if (GetWindowTextW(foregroundWindow, windowTitle, sizeof(windowTitle) / sizeof(windowTitle[0])) > 0)
        {
            DWORD PIDprocess;
            GetWindowThreadProcessId(foregroundWindow, &PIDprocess);

            HANDLE windowProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, PIDprocess);

            WCHAR windowExe[2048];
            K32GetModuleFileNameExW(windowProcess, nullptr, windowExe, 2048);
            CloseHandle(windowProcess);
            std::wstring janelaExeW(windowExe);

            pidexe.exe = janelaExeW;
            pidexe.PID = PIDprocess;
            return pidexe;
        }
    }
    pidexe.exe = L"";
    pidexe.PID = 0;
    return pidexe;
}

void verifyTerminateInactiveProcess(HWND window, DWORD PID, int* mostRecentTimeInteractionPointer, int MaxTimeInactivityAllowed)
{
    int mostRecentTimeInteractionValue = *mostRecentTimeInteractionPointer;
    int countTime = 0;

    MaxTimeInactivityAllowed = MaxTimeInactivityAllowed * 2;

    while (countTime <= MaxTimeInactivityAllowed)
    {
        ++countTime;
        Sleep(500);

        if (mostRecentTimeInteractionValue != *mostRecentTimeInteractionPointer)
        {
            mostRecentTimeInteractionValue = *mostRecentTimeInteractionPointer;
            countTime = 0;
        }
        if (!IsWindow(window))
        {
            tasksOnMonitoring[PID] = L"";
            return;
        }
    }
    HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, PID);
    TerminateProcess(process, 0);
    CloseHandle(process);

    tasksOnMonitoring[PID] = L"";
    return;
}

BOOL CALLBACK getAllProcessHasWindow(HWND window, LPARAM localParam)
{
    if (IsWindowVisible(window))
    {
        DWORD PIDprocess = 0;
        if (GetWindowThreadProcessId(window, &PIDprocess) != 0)
        {
            WCHAR windowTitle[2048] = L"";

            if (GetWindowTextW(window, windowTitle, sizeof(windowTitle) / sizeof(windowTitle[0])) > 0 && PIDprocess != 0)
            {
                HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, PIDprocess);
                WCHAR windowExe[2048] = L"";

                K32GetModuleFileNameExW(hProcess, nullptr, windowExe, 2048);

                std::wstring ignorePaths[5];
                ignorePaths[0] = L"ApplicationFrameHost.exe";
                ignorePaths[1] = L"explorer.exe";
                ignorePaths[2] = L"C:\\Windows\\";
                ignorePaths[3] = L"C:\\Program Files\\WindowsApps\\";
                std::wstring copyWindowExe = windowExe;
                
                for (int i = 0; i <= 3; ++i)
                {
                    if (copyWindowExe.find(ignorePaths[i]) != std::wstring::npos)
                    {
                        CloseHandle(hProcess);
                        return TRUE;
                    }
                }
                std::wstring_convert<std::codecvt_utf8<wchar_t>> convertStrToWstr;
                std::wstring exeWstr;
                std::string exeStr;

                for (const auto& exe : tasksTimeout)
                {
                    exeStr = exe.first;
                    exeWstr = convertStrToWstr.from_bytes(exe.first);

                    if (exeWstr == copyWindowExe)
                    {
                        for (const auto& PIDexe : tasksOnMonitoring)
                        {
                            if (PIDexe.first == PIDprocess && (PIDexe.second == copyWindowExe || PIDexe.second != L""))
                            {
                                CloseHandle(hProcess);
                                return TRUE;
                            }
                        }
                        tasksOnMonitoring[PIDprocess] = copyWindowExe;
                        mostRecentInteractionTaskTime[copyWindowExe][PIDprocess] = 0;
                        threadsVector.push_back(std::thread(verifyTerminateInactiveProcess, window, PIDprocess, &mostRecentInteractionTaskTime[copyWindowExe][PIDprocess], tasksTimeout[exeStr]));
                        threadsVector.back().detach();

                        CloseHandle(hProcess);
                        return TRUE;
                    }
                }
                CloseHandle(hProcess);
                return TRUE;
            }
        }
    }
    return TRUE;
}

void fillFields()
{
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (SUCCEEDED(hr))
    {
        IFileOpenDialog* pFileOpen;

        hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));

        if (SUCCEEDED(hr))
        {
            hr = pFileOpen->Show(NULL);

            if (SUCCEEDED(hr))
            {
                IShellItem* pItem;
                hr = pFileOpen->GetResult(&pItem);
                if (SUCCEEDED(hr))
                {
                    PWSTR pszFilePath;
                    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);

                    if (SUCCEEDED(hr))
                    {
                        std::wstring time = L"";
                        std::cout << "Type timeout to the task(seconds): ";
                        std::wcin >> time;

                        std::wofstream monitoringPrograms;
                        monitoringPrograms.open("programs.txt", std::ios::app);
                        monitoringPrograms << pszFilePath << "=" << time << ";";
                        monitoringPrograms.close();

                        CoTaskMemFree(pszFilePath);
                    }
                    pItem->Release();
                }
            }
            pFileOpen->Release();
        }
    }
    CoUninitialize();
}

void comandLineInterface()
{
    char option;
    std::string line = "";
    size_t delimiterIndex;


    while (true)
    {
        std::cout << "1 - Register new task to monitoring." << std::endl << "2 - Edit timeout of a task." << std::endl;
        std::cout << "3 - Show tasks registered." << std::endl << "4 - Delete tasks registered." << std::endl;
        std::cout << "5 - Start monitoring." << std::endl << std::endl << "Type a option number: ";
        std::cin >> option;

        if (option == '1')
        {
            fillFields();
            system("cls");
            std::cout << "Program registered sucessfully!" << std::endl << std::endl;
        }
        else if (option == '2')
        {
            std::ifstream file;
            file.open("programs.txt");

            while (std::getline(file, line, ';'))
            {
                delimiterIndex = line.find('=');
                std::cout << "Program: " << line.substr(0, delimiterIndex) << " = " << line.substr(delimiterIndex + 1) << std::endl;
            }
            file.close();
        }
        else if (option == '3')
        {
            // code that show all programs registered to be monitored
        }
        else if (option == '4')
        {
            // code to delete a program of monitoring
        }
        else if (option == '5')
        {
            // code that starts the monitoring of registered programs
            std::ifstream monitoringTasksFile;
            std::string fileLine;

            monitoringTasksFile.open("programs.txt");
            size_t delimiterIndex;

            while (std::getline(monitoringTasksFile, fileLine, ';'))
            {
                delimiterIndex = fileLine.find('=');
                tasksTimeout[fileLine.substr(0, delimiterIndex)] = std::stoi(fileLine.substr(delimiterIndex + 1));
            }
            monitoringTasksFile.close();

            threadsVector.push_back(std::thread(globalListenKeyboard));
            threadsVector.back().detach();
            threadsVector.push_back(std::thread(globalListenMouse));
            threadsVector.back().detach();

            while (true)
            {
                EnumWindows(getAllProcessHasWindow, 0);
                Sleep(1000);
            }
        }
        else
        {
            system("cls");
            std::wcout << "Invalid option, select a valid number" << std::endl << std::endl;
        }
    }
    return;
}

int main()
{
    comandLineInterface();
    return 0;
}
