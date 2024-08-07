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
#include <utility> // For std::pair
#include <memory>
#include <cstring> // for memcpy

// Utility function to get the current time as a string
std::string getCurrentTime() {
    std::time_t now = std::time(nullptr);
    char buf[100];
    std::strftime(buf, sizeof(buf), "%Y%m%d%H%M%S", std::localtime(&now));
    return std::string(buf);
}

// Utility function to save an image as a PNG file
void saveImage(const std::string& filename, int width, int height, unsigned char* data) {
    if (!stbi_write_png(filename.c_str(), width, height, 4, data, width * 4)) {
        std::cerr << "Failed to save image to " << filename << std::endl;
    } else {
        std::cout << "Image saved to " << filename << std::endl;
    }
}

// Combines multiple images into a single collage
void combineImages(const std::vector<std::pair<int, unsigned char*> >& images, const std::vector<int>& heights, const std::string& outputFilePath) {
    if (images.empty()) return;

    // Calculate total width and max height for the collage
    int totalWidth = 0;
    int maxHeight = 0;
    for (const auto& image : images) {
        int width = image.first;
        int height = heights[&image - &images[0]];
        totalWidth += width;
        if (height > maxHeight) {
            maxHeight = height;
        }
    }

    // Allocate memory for the combined image
    std::unique_ptr<unsigned char[]> combinedData(new unsigned char[totalWidth * maxHeight * 4]());

    // Copy individual images into the combined image
    int offsetX = 0;
    for (size_t i = 0; i < images.size(); ++i) {
        int width = images[i].first;
        int height = heights[i];
        unsigned char* data = images[i].second;
        for (int y = 0; y < height; ++y) {
            std::memcpy(combinedData.get() + (y * totalWidth + offsetX) * 4, data + (y * width) * 4, width * 4);
        }
        offsetX += width;
    }

    // Save the combined image
    saveImage(outputFilePath, totalWidth, maxHeight, combinedData.get());
}


// Function to arrange images in a Picture-in-Picture (PIP) layout
void pipImages(const std::vector<std::pair<int, unsigned char*>>& images, const std::vector<int>& heights, const std::string& outputFilePath) {
    if (images.empty()) return;

    // Assuming the first image is the main display, and the others are the smaller PIP displays
    const int mainIndex = 0;
    int mainWidth = images[mainIndex].first;
    int mainHeight = heights[mainIndex];

    // Determine the size for the smaller PIP displays (4:1 ratio)
    int pipWidth = mainWidth / 4;
    int pipHeight = mainHeight / 4;

    // Calculate the total width and height needed for the final image
    int totalWidth = mainWidth;
    int totalHeight = mainHeight;

    // Create a new image data array to hold the final PIP layout
    std::unique_ptr<unsigned char[]> combinedData(new unsigned char[totalWidth * totalHeight * 4]());

    // Copy the main display image into the final image data
    std::memcpy(combinedData.get(), images[mainIndex].second, mainWidth * mainHeight * 4);

    // Calculate positions and copy PIP images into the final image
    int offsetX = mainWidth - pipWidth - 10; // 10 pixels margin from the right edge
    int offsetY = mainHeight - pipHeight - 10; // 10 pixels margin from the bottom edge

    for (size_t i = 1; i < images.size(); ++i) {
        unsigned char* pipData = images[i].second;

        // Resize the PIP image to fit the 4:1 ratio size
        for (int y = 0; y < pipHeight; ++y) {
            for (int x = 0; x < pipWidth; ++x) {
                int pipX = x * images[i].first / pipWidth;
                int pipY = y * heights[i] / pipHeight;

                int srcIndex = (pipY * images[i].first + pipX) * 4;
                int dstIndex = ((y + offsetY) * totalWidth + (x + offsetX)) * 4;

                std::memcpy(&combinedData[dstIndex], &pipData[srcIndex], 4);
            }
        }

        // Update the position for the next PIP display (if any)
        offsetY -= pipHeight + 10;
    }

    // Save the final image
    saveImage(outputFilePath, totalWidth, totalHeight, combinedData.get());
}


#if defined(_WIN32)
// Capture a screenshot of a monitor and return the image data
std::pair<int, unsigned char*> captureScreenshot(HMONITOR hMonitor, int& height) {
    MONITORINFO mi;
    mi.cbSize = sizeof(MONITORINFO);
    GetMonitorInfo(hMonitor, &mi);
    int width = mi.rcMonitor.right - mi.rcMonitor.left;
    height = mi.rcMonitor.bottom - mi.rcMonitor.top;

    HDC hScreen = GetDC(NULL);
    HDC hDC = CreateCompatibleDC(hScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, width, height);
    SelectObject(hDC, hBitmap);
    BitBlt(hDC, 0, 0, width, height, hScreen, mi.rcMonitor.left, mi.rcMonitor.top, SRCCOPY);
    ReleaseDC(NULL, hScreen);

    BITMAPINFOHEADER bi = { sizeof(BITMAPINFOHEADER), width, -height, 1, 32, BI_RGB, 0, 0, 0, 0, 0 };
    unsigned char* data = new unsigned char[width * height * 4];
    GetDIBits(hDC, hBitmap, 0, height, data, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

    DeleteObject(hBitmap);
    DeleteDC(hDC);

    return std::make_pair(width, data);
}

// Monitor enumeration callback to capture screenshots from all monitors
BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
    std::vector<std::pair<int, unsigned char*>>& images = *reinterpret_cast<std::vector<std::pair<int, unsigned char*>>*>(dwData);
    std::vector<int>& heights = *reinterpret_cast<std::vector<int>*>(dwData + sizeof(std::vector<std::pair<int, unsigned char*>>));
    
    int height;
    images.push_back(captureScreenshot(hMonitor, height));
    heights.push_back(height);
    return TRUE;
}

#elif defined(__APPLE__)
// Capture a screenshot of a display and return the image data
std::pair<int, unsigned char*> captureScreenshot(CGDirectDisplayID displayId, int& height) {
    CGImageRef screenshot = CGDisplayCreateImage(displayId);
    int width = static_cast<int>(CGImageGetWidth(screenshot));
    height = static_cast<int>(CGImageGetHeight(screenshot));
    
    CFDataRef dataRef = CGDataProviderCopyData(CGImageGetDataProvider(screenshot));
    const UInt8* data = CFDataGetBytePtr(dataRef);
    
    unsigned char* imgData = new unsigned char[width * height * 4];
    std::memcpy(imgData, data, width * height * 4);
    
    CFRelease(dataRef);
    CGImageRelease(screenshot);
    
    return std::make_pair(width, imgData);
}
#endif

extern "C" {
    // Capture screenshots from all monitors and combine them into a single image
    void CaptureScreenshot(const char* baseFilename) {
        std::string baseFilepath(baseFilename);
        std::vector<std::pair<int, unsigned char*> > images;
        std::vector<int> heights;

        #if defined(_WIN32)
        EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, reinterpret_cast<LPARAM>(&images));
        #elif defined(__APPLE__)
        uint32_t displayCount;
        CGGetActiveDisplayList(0, NULL, &displayCount);
        CGDirectDisplayID displays[displayCount];
        CGGetActiveDisplayList(displayCount, displays, &displayCount);
        
        std::cout<<"Enter Type of Image\n1.Collage\n2.PIP";
        int option;
        std::cin>>option;

        for (uint32_t i = 0; i < displayCount; ++i) {
            int height;
            images.push_back(captureScreenshot(displays[i], height));
            heights.push_back(height);
        }
        #else
        std::cerr << "Unsupported platform." << std::endl;
        return;
        #endif

        std::string outputFilePath = baseFilepath + "_collage.png";

        if(option==1)
        combineImages(images, heights, outputFilePath);

        if(option==2)
        pipImages(images,heights,outputFilePath);

        // Cleanup
        for (auto& image : images) {
            delete[] image.second;
        }
    }
}
