#include <windows.h>
#include <setupapi.h>
#include <stdio.h>
#include <wchar.h>
//#include <cfgmgr32.h>   // for MAX_DEVICE_ID_LEN
#define MAX_DEVICE_ID_LEN 200
//#pragma comment(lib, "setupapi.lib")

#define NAME_SIZE 128

#ifndef DISPLAY_DEVICE_ACTIVE
#define DISPLAY_DEVICE_ACTIVE 0x00000001
#endif

void Get2ndSlashBlock(wchar_t *sIn, const wchar_t *DeviceID) {
    const wchar_t *strend = wcschr(&DeviceID[9], L'\\');
    const wchar_t *strstart = &DeviceID[8];
    size_t size = strend - strstart;
    wmemcpy(sIn, strstart, size);
    sIn[size]=L'\0';
}
//
// Assumes hEDIDRegKey is valid
int GetMonitorSizeFromEDID(const HKEY hEDIDRegKey, short *WidthMm, short *HeightMm) {
    DWORD dwType, AcutalValueNameLength = NAME_SIZE;
    wchar_t valueName[NAME_SIZE];

    BYTE EDIDdata[1024];
    DWORD edidsize = sizeof(EDIDdata);

    DWORD i;
    LONG retValue;
    for (i = 0, retValue = ERROR_SUCCESS; retValue != ERROR_NO_MORE_ITEMS; ++i) {
        retValue = RegEnumValueW(hEDIDRegKey, i, &valueName[0],
                &AcutalValueNameLength, NULL, &dwType,
                EDIDdata, // buffer
                &edidsize); // buffer size

        if (retValue != ERROR_SUCCESS || 0 != wcscmp(valueName, L"EDID"))
            continue;

        *WidthMm = (short) (((EDIDdata[68] & 0xF0) << 4) + EDIDdata[66]);
        *HeightMm = (short) (((EDIDdata[68] & 0x0F) << 8) + EDIDdata[67]);

        return 1; // valid EDID found
    }

    return 0; // EDID not found
}

int GetSizeForDevID(const wchar_t *TargetDevID, short *WidthMm, short *HeightMm) {
	int bRes;
    const GUID GUID_CLASS_MONITOR = {0x4d36e96e, 0xe325, 0x11ce, 0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18};
    unsigned long i;
    HDEVINFO devInfo = SetupDiGetClassDevsEx(
            &GUID_CLASS_MONITOR, //class GUID
            NULL, //enumerator
            NULL, //HWND
            DIGCF_PRESENT | DIGCF_PROFILE, // Flags //DIGCF_ALLCLASSES|
            NULL, // device info, create a new one.
            NULL, // machine name, local machine
            NULL);// reserved

    if (NULL == devInfo)
        return 0;

	bRes=0;
    for (i = 0; ERROR_NO_MORE_ITEMS != GetLastError(); ++i) {
        SP_DEVINFO_DATA devInfoData;
        memset(&devInfoData, 0, sizeof(devInfoData));
        devInfoData.cbSize = sizeof(devInfoData);

        if (SetupDiEnumDeviceInfo(devInfo, i, &devInfoData)) {
            WCHAR Instance[MAX_DEVICE_ID_LEN];
			HKEY hEDIDRegKey;
            SetupDiGetDeviceInstanceIdW(devInfo, &devInfoData, Instance, MAX_PATH, NULL);

            if (wcsstr(Instance, TargetDevID) == 0)
                continue;

            hEDIDRegKey = SetupDiOpenDevRegKey(devInfo, &devInfoData,
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

struct DisplayInfo{
    char name[20];
    short physicalWidth_mm;
    short physicalHeight_mm;
};

struct DisplayInfo* getDisplayInfos(int *count)
{
    int size = 0, found = 0;
    struct DisplayInfo *monitors= NULL;
    DWORD adapterIndex, displayIndex;

    *count = 0;

    for (adapterIndex = 0;  ;  adapterIndex++)
    {
        DISPLAY_DEVICEW adapter;

        ZeroMemory(&adapter, sizeof(DISPLAY_DEVICEW));
        adapter.cb = sizeof(DISPLAY_DEVICEW);

        if (!EnumDisplayDevicesW(NULL, adapterIndex, &adapter, 0))
            break;

        if (!(adapter.StateFlags & DISPLAY_DEVICE_ACTIVE))
            continue;

        for (displayIndex = 0;  ;  displayIndex++)
        {
            DISPLAY_DEVICEW display;
            struct DisplayInfo displayInfo;
            wchar_t key[20];

            ZeroMemory(&display, sizeof(DISPLAY_DEVICEW));
            display.cb = sizeof(DISPLAY_DEVICEW);

            if (!EnumDisplayDevicesW(adapter.DeviceName, displayIndex, &display, 0))
                break;

            if (found == size)
            {
                size += 4;
                monitors = realloc(monitors, sizeof(struct DisplayInfo) * size);
            }

            wcstombs(displayInfo.name, display.DeviceString, 20);
            displayInfo.name[20-1] = L'\0';
            Get2ndSlashBlock(key, display.DeviceID);
            GetSizeForDevID(key, &displayInfo.physicalWidth_mm, &displayInfo.physicalHeight_mm);
            monitors[found] = displayInfo;

            found++;
        }
    }

    *count = found;
    return monitors;
}

int main(int argc, char *argv[]) {
    int                size;
    struct DisplayInfo *displayInfos = getDisplayInfos(&size);

    int i;
    for (i = 0; i < size; i++) {
        printf("%s: width: %d, height: %d\n", displayInfos[i].name, displayInfos[i].physicalWidth_mm, displayInfos[i].physicalHeight_mm);
    }
    free(displayInfos);
    return 0;
}