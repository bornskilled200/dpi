#include <windows.h>
#include <setupapi.h>
#include <stdio.h>
#include <stdbool.h>
//#include <cfgmgr32.h>   // for MAX_DEVICE_ID_LEN
#define MAX_DEVICE_ID_LEN 200
#pragma comment(lib, "setupapi.lib")

#define NAME_SIZE 128

const GUID GUID_CLASS_MONITOR = {0x4d36e96e, 0xe325, 0x11ce, 0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18};

void Get2ndSlashBlock(char *sIn, const char *DeviceID) {
    char *strend = strchr(&DeviceID[9], (int) '\\');
    const char *strstart = &DeviceID[8];
    size_t size = strend - strstart;
    memcpy(sIn, strstart, size);
    sIn[size]='\0';
}

// Assumes hEDIDRegKey is valid
bool GetMonitorSizeFromEDID(const HKEY hEDIDRegKey, short *WidthMm, short *HeightMm) {
    DWORD dwType, AcutalValueNameLength = NAME_SIZE;
    TCHAR valueName[NAME_SIZE];

    BYTE EDIDdata[1024];
    DWORD edidsize = sizeof(EDIDdata);

    LONG i, retValue;
    for (i = 0, retValue = ERROR_SUCCESS; retValue != ERROR_NO_MORE_ITEMS; ++i) {
        retValue = RegEnumValue(hEDIDRegKey, i, &valueName[0],
                &AcutalValueNameLength, NULL, &dwType,
                EDIDdata, // buffer
                &edidsize); // buffer size

        if (retValue != ERROR_SUCCESS || 0 != strcmp(valueName, ("EDID")))
            continue;

        *WidthMm = ((EDIDdata[68] & 0xF0) << 4) + EDIDdata[66];
        *HeightMm = ((EDIDdata[68] & 0x0F) << 8) + EDIDdata[67];

        return true; // valid EDID found
    }

    return false; // EDID not found
}

bool GetSizeForDevID(const char *TargetDevID, short *WidthMm, short *HeightMm) {
    HDEVINFO devInfo = SetupDiGetClassDevsEx(
            &GUID_CLASS_MONITOR, //class GUID
            NULL, //enumerator
            NULL, //HWND
            DIGCF_PRESENT | DIGCF_PROFILE, // Flags //DIGCF_ALLCLASSES|
            NULL, // device info, create a new one.
            NULL, // machine name, local machine
            NULL);// reserved

    if (NULL == devInfo)
        return false;

    bool bRes = false;

    ULONG i;
    for (i = 0; ERROR_NO_MORE_ITEMS != GetLastError(); ++i) {
        SP_DEVINFO_DATA devInfoData;
        memset(&devInfoData, 0, sizeof(devInfoData));
        devInfoData.cbSize = sizeof(devInfoData);

        if (SetupDiEnumDeviceInfo(devInfo, i, &devInfoData)) {
            TCHAR Instance[MAX_DEVICE_ID_LEN];
            SetupDiGetDeviceInstanceId(devInfo, &devInfoData, Instance, MAX_PATH, NULL);

            if (strstr(Instance, TargetDevID) == 0)
                continue;

            HKEY hEDIDRegKey = SetupDiOpenDevRegKey(devInfo, &devInfoData,
                    DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);

            if (!hEDIDRegKey || (hEDIDRegKey == INVALID_HANDLE_VALUE))
                continue;

            bRes = GetMonitorSizeFromEDID(hEDIDRegKey, WidthMm, HeightMm);

            RegCloseKey(hEDIDRegKey);
        }
    }
    SetupDiDestroyDeviceInfoList(devInfo);
    return bRes;
}

HMONITOR monitors[6];
int monitor_size;

BOOL CALLBACK MyMonitorEnumProc(
        HMONITOR hMonitor,
        HDC hdcMonitor,
        LPRECT lprcMonitor,
        LPARAM dwData
) {
// Use this function to identify the monitor of interest: MONITORINFO contains the Monitor RECT.
    MONITORINFOEX mi;
    mi.cbSize = sizeof(MONITORINFOEX);

    GetMonitorInfo(hMonitor, &mi);
    OutputDebugString(mi.szDevice);

// For simplicity, we set the last monitor to be the one of interest
    monitors[monitor_size++] = hMonitor;

    return TRUE;
}

BOOL DisplayDeviceFromHMonitor(HMONITOR hMonitor, DISPLAY_DEVICE *ddMonOut) {
    MONITORINFOEX mi;
    mi.
            cbSize = sizeof(MONITORINFOEX);
    GetMonitorInfo(hMonitor,&mi);

    DISPLAY_DEVICE dd;
    dd.
            cb = sizeof(dd);
    DWORD devIdx = 0; // device index

    char *DeviceID;
    bool bFoundDevice = false;
    while (EnumDisplayDevices(0, devIdx, &dd, 0)) {
        devIdx++;
        if (0 !=strcmp(dd.DeviceName, mi.szDevice))
            continue;

        DISPLAY_DEVICE ddMon;
        ZeroMemory(&ddMon, sizeof(ddMon));
        ddMon.
                cb = sizeof(ddMon);
        DWORD MonIdx = 0;

        while (EnumDisplayDevices(dd.DeviceName, MonIdx, &ddMon, 0)) {
            MonIdx++;

            *ddMonOut = ddMon;
            return TRUE;

            ZeroMemory(&ddMon, sizeof(ddMon));
            ddMon.
                    cb = sizeof(ddMon);
        }

        ZeroMemory(&dd, sizeof(dd));
        dd.
                cb = sizeof(dd);
    }

    return FALSE;
}

int main(int argc, char *argv[]) {
    char DeviceID[10];
    monitor_size = 0;
    // Identify the HMONITOR of interest via the callback MyMonitorEnumProc
    EnumDisplayMonitors(NULL, NULL, MyMonitorEnumProc, NULL);

    DISPLAY_DEVICE ddMon;
    if (monitor_size == 0)
        return 1;

    int i;
    for (i = 0; i < monitor_size; i++) {
        if (FALSE == DisplayDeviceFromHMonitor(monitors[i], &ddMon))
            return 1;

        Get2ndSlashBlock(DeviceID, ddMon.DeviceID);

        short WidthMm, HeightMm;
        bool bFoundDevice = GetSizeForDevID(DeviceID, &WidthMm, &HeightMm);
        printf("%s: width: %d, height: %d\n", ddMon.DeviceString, WidthMm, HeightMm);
    }
    return 0;
}