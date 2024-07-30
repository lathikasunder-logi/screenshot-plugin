#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <ApplicationServices/ApplicationServices.h>
#endif

#include <iostream>
#include <string>
#include <ctime>
#include <vector>

std::string getCurrentTime() {
    std::time_t now = std::time(nullptr);
    char buf[100];
    std::strftime(buf, sizeof(buf), "%Y%m%d%H%M%S", std::localtime(&now));
    return std::string(buf);
}

void saveScreenshot(const std::string& filename, int width, int height, unsigned char* data) {
    if (!stbi_write_png(filename.c_str(), width, height, 4, data, width * 4)) {
        std::cerr << "Failed to save screenshot to " << filename << std::endl;
    } else {
        std::cout << "Screenshot saved to " << filename << std::endl;
    }
}

#if defined(_WIN32)
void captureScreenshot(HMONITOR hMonitor, const std::string& filepath) {
    MONITORINFO mi;
    mi.cbSize = sizeof(MONITORINFO);
    GetMonitorInfo(hMonitor, &mi);
    int width = mi.rcMonitor.right - mi.rcMonitor.left;
    int height = mi.rcMonitor.bottom - mi.rcMonitor.top;

    HDC hScreen = GetDC(NULL);
    HDC hDC = CreateCompatibleDC(hScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, width, height);
    SelectObject(hDC, hBitmap);
    BitBlt(hDC, 0, 0, width, height, hScreen, mi.rcMonitor.left, mi.rcMonitor.top, SRCCOPY);
    ReleaseDC(NULL, hScreen);

    BITMAPINFOHEADER bi = { sizeof(BITMAPINFOHEADER), width, -height, 1, 32, BI_RGB, 0, 0, 0, 0, 0 };
    unsigned char* data = new unsigned char[width * height * 4];
    GetDIBits(hDC, hBitmap, 0, height, data, (BITMAPINFO*)&bi, DIB_RGB_COLORS);
    
    saveScreenshot(filepath, width, height, data);
    
    DeleteObject(hBitmap);
    DeleteDC(hDC);
    delete[] data;
}

BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
    std::vector<std::string>& filepaths = *reinterpret_cast<std::vector<std::string>*>(dwData);
    std::string filepath = filepaths.back();
    filepaths.pop_back();
    captureScreenshot(hMonitor, filepath);
    return TRUE;
}

#elif defined(__APPLE__)
void captureScreenshot(CGDirectDisplayID displayId, const std::string& filepath) {
    CGImageRef screenshot = CGDisplayCreateImage(displayId);
    int width = static_cast<int>(CGImageGetWidth(screenshot));
    int height = static_cast<int>(CGImageGetHeight(screenshot));
    
    CFDataRef dataRef = CGDataProviderCopyData(CGImageGetDataProvider(screenshot));
    const UInt8* data = CFDataGetBytePtr(dataRef);
    
    unsigned char* imgData = new unsigned char[width * height * 4];
    memcpy(imgData, data, width * height * 4);
    
    saveScreenshot(filepath, width, height, imgData);
    
    CFRelease(dataRef);
    CGImageRelease(screenshot);
    delete[] imgData;
}
#endif

extern "C" {
    void CaptureScreenshot(const char* baseFilename) {
        std::string baseFilepath(baseFilename);
        std::vector<std::string> filepaths;

        #if defined(_WIN32)
        int i = 0;
        filepaths.push_back(baseFilepath + "_monitor" + std::to_string(i++) + ".png");
        EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, reinterpret_cast<LPARAM>(&filepaths));
        #elif defined(__APPLE__)
        uint32_t displayCount;
        CGGetActiveDisplayList(0, NULL, &displayCount);
        CGDirectDisplayID displays[displayCount];
        CGGetActiveDisplayList(displayCount, displays, &displayCount);
        
        for (uint32_t i = 0; i < displayCount; ++i) {
            std::string filepath = baseFilepath + "_display" + std::to_string(i) + ".png";
            captureScreenshot(displays[i], filepath);
        }
        #else
        std::cerr << "Unsupported platform." << std::endl;
        #endif
    }
}
