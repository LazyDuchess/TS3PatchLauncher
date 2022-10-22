// SmoothPatchLauncher.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <sstream>
#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <Psapi.h>
#include <TlHelp32.h>

#include <ShlObj.h>    // Shell API
#include <Propkey.h>   // PKEY_* constants
#include <atlbase.h>   // CComPtr, CComHeapPtr
#include <io.h>
#include <fcntl.h>
#include <string>
#include <system_error>
#include "config.h"
#include "filesys.h"

//#define CONSOLE

VOID startup(LPCTSTR lpApplicationName)
{
    // additional information
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    // set the size of the structures
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // start the program up
    CreateProcess(lpApplicationName,   // the path
        NULL,        // Command line
        NULL,           // Process handle not inheritable
        NULL,           // Thread handle not inheritable
        FALSE,          // Set handle inheritance to FALSE
        0,              // No creation flags
        NULL,           // Use parent's environment block
        NULL,           // Use parent's starting directory 
        &si,            // Pointer to STARTUPINFO structure
        &pi             // Pointer to PROCESS_INFORMATION structure (removed extra parentheses)
    );
    // Close process and thread handles. 
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

// Wrapper for SHCreateItemFromParsingName(), IShellItem2::GetString()
// Throws std::system_error in case of any error.
std::wstring GetShellPropStringFromPath(LPCWSTR pPath, PROPERTYKEY const& key)
{
    // Use CComPtr to automatically release the IShellItem2 interface when the function returns
    // or an exception is thrown.
    CComPtr<IShellItem2> pItem;
    HRESULT hr = SHCreateItemFromParsingName(pPath, nullptr, IID_PPV_ARGS(&pItem));
    if (FAILED(hr))
        throw std::system_error(hr, std::system_category(), "SHCreateItemFromParsingName() failed");

    // Use CComHeapPtr to automatically release the string allocated by the shell when the function returns
    // or an exception is thrown (calls CoTaskMemFree).
    CComHeapPtr<WCHAR> pValue;
    hr = pItem->GetString(key, &pValue);
    if (FAILED(hr))
        throw std::system_error(hr, std::system_category(), "IShellItem2::GetString() failed");

    // Copy to wstring for convenience
    return std::wstring(pValue);
}

std::wstring gameName = L"TS3.exe";
std::wstring launcherName = L"Sims3Launcher.exe";
std::wstring launcherDesc = L"Sims3Launcher";
std::wstring platformName = L"EADesktop.exe";

#ifndef CONSOLE
int WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow)
#endif
#ifdef CONSOLE
int main()
#endif
{
    CoInitialize(nullptr);

    bool cfgResult = false;

    ConfigObject config = ConfigObject(FileSys::GetAbsolutePathAuto(L"TS3PatchLauncher.cfg"), cfgResult);

    if (cfgResult)
    {
        gameName = config.GetWString(L"GameName", gameName);
        launcherName = config.GetWString(L"LauncherName", launcherName);
        launcherDesc = config.GetWString(L"LauncherDescription", launcherDesc);
        platformName = config.GetWString(L"PlatformName", platformName);
    }

    bool launcherIsGame = false;
    bool managedbyLauncher = false;
    int nArgs;
    LPWSTR *argList = CommandLineToArgvW(GetCommandLine(),&nArgs);
    for (int i = 0; i < nArgs; i++)
    {
        if (wcscmp(argList[i], L"-launcher") == 0)
            managedbyLauncher = true;
    }
    unsigned int i;
    DWORD aProcesses[1024], cbNeeded, cProcesses;
    wchar_t modName[MAX_PATH];
    GetModuleFileName(NULL, modName, MAX_PATH);
    std::wstring::size_type pos = std::wstring(modName).find_last_of(L"\\/");
    auto folder = std::wstring(modName).substr(0, pos);
    wchar_t wcs[MAX_PATH];
    wchar_t dllPath[MAX_PATH];
    wchar_t launcherPath[MAX_PATH];

    wcscpy_s(wcs, folder.c_str());
    wcscat_s(wcs, L"\\");
    wcscat_s(wcs, gameName.c_str());

    wcscpy_s(launcherPath, folder.c_str());
    wcscat_s(launcherPath, L"\\");
    wcscat_s(launcherPath, launcherName.c_str());

        std::wstring launcherDesc = GetShellPropStringFromPath(launcherPath, PKEY_FileDescription);
        wprintf(launcherDesc.c_str());
        if (wcscmp(launcherDesc.c_str(), launcherDesc.c_str()) != 0)
        {
            launcherIsGame = true;
            wprintf(L"Launcher is game!");
        }
        else
            wprintf(L"Launcher is not game.");
    GetFullPathName(L"TS3Patch.asi", MAX_PATH, dllPath, NULL);
    bool gameRunning = false;
    if (EnumProcesses(aProcesses, sizeof(aProcesses), &cbNeeded))
    {
        cProcesses = cbNeeded / sizeof(DWORD);
        for (i = 0; i < cProcesses; i++)
        {
            if (aProcesses[i] != 0)
            {
                TCHAR szProcessName[MAX_PATH] = TEXT("<unknown>");

                HANDLE hProcess = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION,
                    FALSE, aProcesses[i]);

                if (NULL != hProcess)
                {
                    HMODULE hMod;
                    DWORD cbNeeded;

                    if (EnumProcessModules(hProcess, &hMod, sizeof(hMod),
                        &cbNeeded))
                    {
                        GetModuleBaseName(hProcess, hMod, szProcessName,
                            sizeof(szProcessName) / sizeof(TCHAR));
                        HANDLE hand;
                        if (wcsstr(szProcessName, L"TS3PatchLauncher") != NULL && GetProcessId(hProcess) != GetCurrentProcessId())
                        {
                            TerminateProcess(hProcess, 1);
                        }
                        if (!wcscmp(szProcessName, gameName.c_str()) /* || !wcscmp(szProcessName, L"TS3W.exe")*/ || !wcscmp(szProcessName, launcherName.c_str()))
                            gameRunning = true;
                    }
                }
            }
        }
    }
    if (!gameRunning)
        startup(launcherPath);
    
    Sleep(5000);
    bool launcherOpen = false;
    bool gameOpen = false;
    bool originOpen = false;
    bool launcherAndGameOpened = false;

    bool gameOpenThisTick = false;
    bool launcherOpenThisTick = false;
    bool originOpenThisTick = false;

    if (managedbyLauncher)
        launcherOpen = true;

    std::wstring pattern(L"*.asi");
    WIN32_FIND_DATA data;
    HANDLE hFind;
    std::vector<std::wstring> v;
    if ((hFind = FindFirstFile(pattern.c_str(), &data)) != INVALID_HANDLE_VALUE) {
        do {
            wchar_t fullPath[MAX_PATH];
            wcscpy_s(fullPath, folder.c_str());
            wcscat_s(fullPath, L"\\");
            wcscat_s(fullPath, data.cFileName);
            v.push_back(fullPath);
            wprintf(fullPath);
            wprintf(L"\n");
        } while (FindNextFile(hFind, &data) != 0);
        FindClose(hFind);
    }
    wchar_t currentPath[MAX_PATH];
    unsigned int n;
    while (true)
    {
        gameOpenThisTick = false;
        launcherOpenThisTick = false;
        originOpenThisTick = false;

        if (EnumProcesses(aProcesses, sizeof(aProcesses), &cbNeeded))
        {
            
            cProcesses = cbNeeded / sizeof(DWORD);
            for (i = 0; i < cProcesses; i++)
            {
                if (aProcesses[i] != 0)
                {
                    TCHAR szProcessName[MAX_PATH] = TEXT("<unknown>");

                    HANDLE hProcess = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION,
                        FALSE, aProcesses[i]);

                    if (NULL != hProcess)
                    {
                        HMODULE hMod;
                        DWORD cbNeeded;

                        if (EnumProcessModules(hProcess, &hMod, sizeof(hMod),
                            &cbNeeded))
                        {
                            GetModuleBaseName(hProcess, hMod, szProcessName,
                                sizeof(szProcessName) / sizeof(TCHAR));
                            HANDLE hand;
                            if (!wcscmp(szProcessName, launcherName.c_str()))
                            {
                                if (!launcherOpen && !managedbyLauncher && launcherIsGame)
                                {
                                    Sleep(100);
                                    for (n = 0; n < v.size(); n++)
                                    {

                                        wcscpy_s(currentPath, v.at(n).c_str());
                                        LPVOID allocatedMem = VirtualAllocEx(hProcess, NULL, sizeof(currentPath), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

                                        WriteProcessMemory(hProcess, allocatedMem, currentPath, sizeof(currentPath), NULL);

                                        CreateRemoteThread(hProcess, 0, 0, (LPTHREAD_START_ROUTINE)LoadLibrary, allocatedMem, 0, 0);

                                    }
                                }
                                launcherOpen = true;
                                launcherOpenThisTick = true;
                            }
                            if (!wcscmp(szProcessName, gameName.c_str())/* || !wcscmp(szProcessName, L"TS3W.exe")*/)
                            {
                                if (!gameOpen)
                                {
                                    Sleep(100);
                                    
                                    for (n = 0; n < v.size(); n++)
                                    {
                                        
                                        wcscpy_s(currentPath, v.at(n).c_str());
                                        LPVOID allocatedMem = VirtualAllocEx(hProcess, NULL, sizeof(currentPath), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

                                        WriteProcessMemory(hProcess, allocatedMem, currentPath, sizeof(currentPath), NULL);

                                        CreateRemoteThread(hProcess, 0, 0, (LPTHREAD_START_ROUTINE)LoadLibrary, allocatedMem, 0, 0);
                                        
                                    }

                                }
                                if (launcherOpen)
                                    launcherAndGameOpened = true;
                                gameOpen = true;
                                gameOpenThisTick = true;
                            }
                            if (!wcscmp(szProcessName, platformName.c_str()))
                            {
                                originOpen = true;
                                originOpenThisTick = true;
                            }
                            
                            
                        }
                        CloseHandle(hProcess);
                    }
                }
            }
        }
        if (originOpen)
        {
            if (!gameOpenThisTick)
                gameOpen = false;
            if (!originOpenThisTick)
                return 0;
            if (!launcherOpenThisTick && launcherAndGameOpened == false && launcherOpen == true)
            {
                return 0;
            }
            if (!gameOpenThisTick && !launcherOpenThisTick && launcherAndGameOpened == true)
            {
                return 0;
            }
        }
        Sleep(100);
    }
    return 0;
}