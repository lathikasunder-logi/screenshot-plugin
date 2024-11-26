#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <curl/curl.h>
#include <nlohmann/json.hpp>

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

// for hosting

#include <fstream>

// for base64 encoding
#include <sstream>
#include <iomanip>

// Utility function to get the current time as a string
std::string getCurrentTime()
{
    std::time_t now = std::time(nullptr);
    char buf[100];
    std::strftime(buf, sizeof(buf), "%Y%m%d%H%M%S", std::localtime(&now));
    return std::string(buf);
}

// Utility function to save an image as a PNG file
void saveImage(const std::string &filename, int width, int height, unsigned char *data)
{
    if (!stbi_write_png(filename.c_str(), width, height, 4, data, width * 4))
    {
        std::cerr << "Failed to save image to " << filename << std::endl;
    }
    else
    {
        std::cout << "Image saved to " << filename << std::endl;
    }
}

void printImages(const std::vector<std::pair<int, unsigned char *>> &images)
{
    for (const auto &imagePair : images)
    {
        int width = imagePair.first;                 // Assuming the first element is width
        unsigned char *imageData = imagePair.second; // Pointer to image data

        std::cout << "Image Width: " << width
                  << ", Data Pointer: " << static_cast<void *>(imageData) << std::endl;

        // If you want to print the first few bytes of the image data
        std::cout << "First 10 bytes of image data: ";
        for (int i = 0; i < 10 && imageData[i]; ++i)
        {
            std::cout << static_cast<int>(imageData[i]) << " ";
        }
        std::cout << std::endl;
    }
}

#include <memory>
#include <cstring> // For memcpy

// Helper function to downscale an image
unsigned char* downscaleImage(unsigned char* data, int originalWidth, int originalHeight, int newWidth, int newHeight) {
    std::unique_ptr<unsigned char[]> resizedData(new unsigned char[newWidth * newHeight * 4]);
    float xRatio = static_cast<float>(originalWidth) / newWidth;
    float yRatio = static_cast<float>(originalHeight) / newHeight;

    for (int y = 0; y < newHeight; ++y) {
        for (int x = 0; x < newWidth; ++x) {
            int srcX = static_cast<int>(x * xRatio);
            int srcY = static_cast<int>(y * yRatio);
            std::memcpy(&resizedData[(y * newWidth + x) * 4],
                        &data[(srcY * originalWidth + srcX) * 4],
                        4); // Copy RGBA pixel
        }
    }
    return resizedData.release();
}

// Combine images and compress the output
void combineImages(const std::vector<std::pair<int, unsigned char*>>& images, const std::vector<int>& heights, const std::string& outputFilePath) {
    if (images.empty()) {
        std::cerr << "No images to combine.\n";
        return;
    }

    // Calculate total width and max height
    int totalWidth = 0;
    int maxHeight = 0;
    std::vector<int> scaledHeights;
    std::vector<int> scaledWidths;
    std::vector<std::unique_ptr<unsigned char[]>> scaledImages;

    const int targetWidth = 800; // Adjust to your needs
    const int targetHeight = 800;

    for (size_t i = 0; i < images.size(); ++i) {
        int originalWidth = images[i].first;
        int originalHeight = heights[i];

        // Compute new dimensions maintaining aspect ratio
        float scale = std::min(static_cast<float>(targetWidth) / originalWidth, static_cast<float>(targetHeight) / originalHeight);
        int newWidth = static_cast<int>(originalWidth * scale);
        int newHeight = static_cast<int>(originalHeight * scale);

        scaledWidths.push_back(newWidth);
        scaledHeights.push_back(newHeight);

        // Downscale image
        scaledImages.emplace_back(downscaleImage(images[i].second, originalWidth, originalHeight, newWidth, newHeight));

        totalWidth += newWidth;
        maxHeight = std::max(maxHeight, newHeight);
    }

    // Allocate memory for the combined image
    std::unique_ptr<unsigned char[]> combinedData(new unsigned char[totalWidth * maxHeight * 4]());

    // Copy scaled images into the combined image
    int offsetX = 0;
    for (size_t i = 0; i < scaledImages.size(); ++i) {
        int width = scaledWidths[i];
        int height = scaledHeights[i];
        unsigned char* data = scaledImages[i].get();
        for (int y = 0; y < height; ++y) {
            std::memcpy(combinedData.get() + (y * totalWidth + offsetX) * 4,
                        data + (y * width) * 4, width * 4);
        }
        offsetX += width;
    }

    // Save the combined image as a compressed format (e.g., JPEG or WebP)
    int quality = 90; // Adjust the quality (1-100)
    if (stbi_write_jpg(outputFilePath.c_str(), totalWidth, maxHeight, 4, combinedData.get(), quality)) {
        std::cout << "Image saved successfully as: " << outputFilePath << "\n";
    } else {
        std::cerr << "Failed to save the image.\n";
    }
}

// Function to arrange images in a Picture-in-Picture (PIP) layout
void pipImages(const std::vector<std::pair<int, unsigned char *>> &images, const std::vector<int> &heights, const std::string &outputFilePath)
{
    if (images.empty())
        return;

    const int mainIndex = 0;
    int mainWidth = images[mainIndex].first;
    int mainHeight = heights[mainIndex];

    int pipWidth = mainWidth / 3.2;
    int pipHeight = mainHeight / 3.2;

    // Calculate the total width and height needed for the final image
    int totalWidth = mainWidth;
    int totalHeight = mainHeight;

    // Create a new image data array to hold the final PIP layout
    std::unique_ptr<unsigned char[]> combinedData(new unsigned char[totalWidth * totalHeight * 4]());

    // Copy the main display image into the final image data
    std::memcpy(combinedData.get(), images[mainIndex].second, mainWidth * mainHeight * 4);

    // Calculate positions and copy PIP images into the final image
    int offsetX = mainWidth - pipWidth - 20;   // 10 pixels margin from the right edge
    int offsetY = mainHeight - pipHeight - 20; // 10 pixels margin from the bottom edge

    for (size_t i = 1; i < images.size(); ++i)
    {
        unsigned char *pipData = images[i].second;

        // Resize the PIP image to fit the 4:1 ratio size
        for (int y = 0; y < pipHeight; ++y)
        {
            for (int x = 0; x < pipWidth; ++x)
            {
                int pipX = x * images[i].first / pipWidth;
                int pipY = y * heights[i] / pipHeight;

                int srcIndex = (pipY * images[i].first + pipX) * 4;
                int dstIndex = ((y + offsetY) * totalWidth + (x + offsetX)) * 4;

                std::memcpy(&combinedData[dstIndex], &pipData[srcIndex], 4);
            }
        }

        // Update the position for the next PIP display (if any)
        offsetY -= pipHeight + 15;
    }

    // Save the final image
    saveImage(outputFilePath, totalWidth, totalHeight, combinedData.get());
}

// encode the image

static const std::string base64_chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

std::string base64_encode(unsigned char const *bytes_to_encode, unsigned int in_len)
{
    std::string ret;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];

    while (in_len--)
    {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3)
        {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; i < 4; i++)
                ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i)
    {
        for (j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (j = 0; j < i + 1; j++)
            ret += base64_chars[char_array_4[j]];

        while (i++ < 3)
            ret += '=';
    }

    return ret;
}

// Function to encode the image file as a Base64 string
std::string base64encode(const std::string &filePath)
{
    // Open the file in binary mode
    std::cout << "Base64encoding entered....\n";
    std::ifstream imageFile(filePath, std::ios::binary);
    std::cout << "image is taken into stream";

    if (!imageFile.is_open())
    {
        std::cout << "Could not open file for reading.\n";
    }

    std::cout << "Going to read img..\n";
    // Read the contents of the file
    std::vector<unsigned char> imageData((std::istreambuf_iterator<char>(imageFile)), std::istreambuf_iterator<char>());
    std::cout << "Image read...\n";
    std::cout << "Base64rncoding....\n";
    // Encode the image data to Base64
    return base64_encode(imageData.data(), imageData.size());
}

using json = nlohmann::json;

// Callback function to capture response data
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string *)userp)->append((char *)contents, size * nmemb);
    return size * nmemb;
}

// Function to upload image data to API and receive response
std::string uploadImageToAPI(const std::string &apiUrl, const std::string &imageKey, const std::string &base64Image)
{
    CURL *curl;
    CURLcode res;
    std::string readBuffer;
    std::string imageUrl;

    curl = curl_easy_init();
    if (curl)
    {
        std::string jsonPayload = "{\"contentType\":\"image/png\", \"imageDataAsBase64String\":\"" + base64Image + "\", \"key\":\"" + imageKey + "\"}";

        std::ofstream outFile("request.json");
        if (outFile.is_open())
        {
            outFile << jsonPayload;
            outFile.close();
            std::cout << "\nPayload written to request.json" << std::endl;
        }
        else
        {
            std::cerr << "\nFailed to open request.json for writing" << std::endl;
        }

        // Set the URL for the POST request
        curl_easy_setopt(curl, CURLOPT_URL, apiUrl.c_str());

        // Set the POST data
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonPayload.c_str());

        // Set headers
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, "Authorization: Bearer eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCIsImtpZCI6IjkxYWNmNWMwNmY4OThkNTIwOTFjNDdkNTkzNDU4OWQ0YmNhYjZjNjcifQ.eyJqdGkiOiIxYzVkZDhhNS1jYTZiLTQ3YTYtODFjNC0yNTJkZTQyNTU2MTAiLCJ2ZXIiOiIxLjAiLCJhbXIiOlsicHdkIl0sImlhdCI6MTczMjYxNDY2MywiZXhwIjoxNzMyNjE4MjYzLCJhdWQiOiIzNGRmY2NhZC03YTFhLTRlNGQtYWViMy0yZDljNTIxYzAzZmUiLCJpc3MiOiJodHRwczovL3NhbmRib3guYWNjb3VudHMubG9naS5jb20iLCJzdWIiOiJlMzdmMTllMy0xNWI3LTRmNzQtOWNhNS01NDllYzdjMDkxMmUifQ.xEIMyZhZL_S-BHvBg4-sCdnuipd5vBOLp6YjKRHhzOhPI83qvxxVq-id7DQ0Ljeh08s6cCxZ6gIfilNeND2xVHv-nFf8QXBarhkJhIdT3wk_bqsJwfHMSXiv23H1LW89dEPofS4lWJXXD6mI1FlXLKblko_A-8xIOR-0-Qvr114QmC_Ejpi_8wUDP-QSmxdAkoHn_RSe_ioURBuZ0ovh0hnP0MJUpgdaBCuJ2GzNLJowDWeNi1EfR8EL40qXnTLQ4me0XRbLvdgV7LBk1Q2dwKGQilOhzizSaxSD0Z5m05jnr0FigwLDJ-nm4wCK4dieaa_Y8PKUVJWMOmYjf5rtd6SkES6YkhUOlCVH70LQmblZ3P9yokQN5ElitQ0Ckc0FJNqozkxDE6ZiL1i5dCUGcsDYUeXow8RuXEPiHh_rIKX-ZqxPiI_uXbznSyN11WzKmr__lJRBg-QuwKxLbHMzH7CMXi5ruWqYYmp69Iow3k3KANIXx4ha90aLPeqjVO7I6Gn46Z4BuwSUzDvQw3zFxs6D1qsSyjUrDiLqLWVqjTgMVOm1iOxFc454b6tqzE7_GHGxusOSyf2iVoUR1d8UdwcY2tFLb9kbkigBSUaK8EUltLC8Yvl7KXgmxI9M7nXKarcOoxoSBvfSgaPo_oakbQGCFeku5eMjoThJCxICbAY");
        headers = curl_slist_append(headers, "Cookie: .HarmonyAuth=8E0E53BFC6342A14270F8F4B10EF5797218A34B76E4A3D0587FA2A8CD7F4D973E1E120F0C726C37FE86FB20F1106F69E7B1B45907D5D316BF419457E78A7B6B52186A07AB9F10E8C290EB966FEBBBF9133D1C8BD1136EB9266172A5B5AE8AC5CA935F899BEDBC156EA3CE4718B7C9197DE7ECFFBBCA441BCA5EDD673A41F1F6F63119395F0C4300CF721A432C208499CB4FBBB47FE20DA3AC8647FBF9D074F0CA02F6CF1; .AUTHTOKEN=eyJhdXRoIjp7ImRldmljZSI6W10sInVzZXJuYW1lIjpbImFwbStzeW5jdGVzdGRldjJAbG9naXRlY2guY29tIl0sImFjY291bnRpZCI6WyI1MjA0MCJdLCJyZW1vdGUiOltdLCJnbG9iYWxkZXZpY2V2ZXJzaW9uIjpbXSwiaG91c2Vob2xkaWQiOlsiMzg4MTAiXSwiZGF0YWNvbnNlbnQiOlsiVHJ1ZSJdfSwibWFjIjoidmJtNXdPRFIrb2Q2aFJoNjFYUVFHdEZlS3VDWnF2ZUYyTXBBa2g2UXBybz0ifQ==");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // Set up callback to capture response
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        // Perform the request
        res = curl_easy_perform(curl);

        // Check for errors
        if (res != CURLE_OK)
        {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        }
        else
        {
            std::cout << "Image uploaded successfully!" << std::endl;
        }

        try
        {
            std::cout<<readBuffer;
            json responseJson = json::parse(readBuffer);
            imageUrl = responseJson["UploadFileToS3BucketResult"];
            std::cout << "Image URL: " << imageUrl << std::endl;
        }
        catch (json::parse_error &e)
        {
            std::cerr << "JSON parse error: " << e.what() << std::endl;
        }

        // Cleanup
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }

    return imageUrl;
}

#include <curl/curl.h>
#include <nlohmann/json.hpp>
void postImageToIfttt(std::string imageUrl)
{
    CURL *curl;
    CURLcode res;

    std::cout << "POST TO IFTTT FIRED\n";
    curl = curl_easy_init();
    if (curl)
    {
        std::cout << "GOING TO FIRE TRIGGER\n";

        // URL encode the imageUrl
        char *encodedImageUrl = curl_easy_escape(curl, imageUrl.c_str(), imageUrl.length());
        if (encodedImageUrl) {
            // Construct the full URL with the encoded imageUrl
            const std::string url = "https://maker.ifttt.com/trigger/loupdeckAction/with/key/cAUpJn5crdm1oqUZQSUx7DLJLYS0bRzp7vhrqDccde_?value1=" + std::string(encodedImageUrl);
            
            // Print the constructed URL
            std::cout << "Constructed URL: " << url << std::endl;

            // Set the URL for the request
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

            // Perform the request, and res will get the return code
            res = curl_easy_perform(curl);

            // Check for errors
            if (res != CURLE_OK)
            {
                std::cout << "RESPONSE:\n";
                std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
                std::cout << res;
            }
            else
            {
                std::cout << "GET request sent successfully!" << std::endl;
            }

            // Free the URL-encoded string
            curl_free(encodedImageUrl);
        } else {
            std::cerr << "Failed to encode image URL." << std::endl;
        }

        curl_easy_cleanup(curl);
    }
    return;
}


#if defined(_WIN32)
// Convert BGRA to RGBA
void convertBGRAToRGBA(unsigned char *data, int width, int height)
{
    for (int i = 0; i < width * height; ++i)
    {
        std::swap(data[i * 4], data[i * 4 + 2]); // Swap red and blue
    }
}

// // Capture a screenshot of a monitor and return the image data
// std::pair<int, unsigned char*> captureScreenshot(HMONITOR hMonitor, int& height) {
//     MONITORINFO mi;
//     mi.cbSize = sizeof(MONITORINFO);
//     GetMonitorInfo(hMonitor, &mi);
//     int width = mi.rcMonitor.right - mi.rcMonitor.left;
//     height = mi.rcMonitor.bottom - mi.rcMonitor.top;

//     HDC hScreen = GetDC(NULL);
//     HDC hDC = CreateCompatibleDC(hScreen);
//     HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, width, height);
//     SelectObject(hDC, hBitmap);
//     BitBlt(hDC, 0, 0, width, height, hScreen, mi.rcMonitor.left, mi.rcMonitor.top, SRCCOPY);
//     ReleaseDC(NULL, hScreen);

//     BITMAPINFOHEADER bi = { sizeof(BITMAPINFOHEADER), width, -height, 1, 32, BI_RGB, 0, 0, 0, 0, 0 };
//     unsigned char* data = new unsigned char[width * height * 4];
//     GetDIBits(hDC, hBitmap, 0, height, data, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

//     DeleteObject(hBitmap);
//     DeleteDC(hDC);

//     // Convert BGRA to RGBA
//     convertBGRAToRGBA(data, width, height);

//     return std::make_pair(width, data);
//

void captureScreenshotForMonitor(HDC hScreenDC, int width, int height, unsigned char* imgData, std::vector<std::pair<int, unsigned char *>>& images, std::vector<int>& heights)
{
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);
    if (hBitmap == NULL) {
        std::cerr << "Failed to create bitmap" << std::endl;
        return;
    }

    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
    SelectObject(hMemoryDC, hBitmap);

    // Capture screen into the bitmap
    if (!BitBlt(hMemoryDC, 0, 0, width, height, hScreenDC, 0, 0, SRCCOPY)) {
        std::cerr << "Failed to copy screen to bitmap" << std::endl;
        DeleteObject(hBitmap);
        DeleteDC(hMemoryDC);
        return;
    }

    BITMAP bmp;
    GetObject(hBitmap, sizeof(BITMAP), &bmp);
    unsigned char* pixels = new unsigned char[bmp.bmWidthBytes * bmp.bmHeight];
    GetBitmapBits(hBitmap, bmp.bmWidthBytes * bmp.bmHeight, pixels);

    // Prepare image data in RGBA format
    imgData = new unsigned char[width * height * 4]; // Allocate memory for the image data
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int pixelIndex = y * bmp.bmWidthBytes + x * (bmp.bmBitsPixel / 8);
            int imgIndex = y * width + x;

            if (bmp.bmBitsPixel == 32) {
                imgData[imgIndex * 4 + 0] = pixels[pixelIndex + 2]; // Red
                imgData[imgIndex * 4 + 1] = pixels[pixelIndex + 1]; // Green
                imgData[imgIndex * 4 + 2] = pixels[pixelIndex + 0]; // Blue
                imgData[imgIndex * 4 + 3] = pixels[pixelIndex + 3]; // Alpha
            } else {
                imgData[imgIndex * 4 + 0] = pixels[pixelIndex + 2]; // Red
                imgData[imgIndex * 4 + 1] = pixels[pixelIndex + 1]; // Green
                imgData[imgIndex * 4 + 2] = pixels[pixelIndex + 0]; // Blue
                imgData[imgIndex * 4 + 3] = 255; // Fully opaque alpha
            }
        }
    }

    // Store the captured data in the images vector
    images.push_back({width, imgData});
    heights.push_back(height);

    // Cleanup
    delete[] pixels;
    DeleteObject(hBitmap);
    DeleteDC(hMemoryDC);
}
void captureScreenshot(const std::string &outputFilePath,bool isPip)
{
    // Create vectors to store image data and heights for all monitors
    std::vector<std::pair<int, unsigned char *>> images;
    std::vector<int> heights;

    // EnumDisplayDevices to get all connected monitors
    DISPLAY_DEVICE dd;
    memset(&dd, 0, sizeof(dd));
    dd.cb = sizeof(dd);

    int monitorIndex = 0;

    while (EnumDisplayDevices(NULL, monitorIndex, &dd, 0)) {
        // Get the device context of the monitor
        HDC hScreenDC = CreateDC(dd.DeviceName, NULL, NULL, NULL);
        if (hScreenDC == NULL) {
            std::cerr << "Failed to get monitor DC for monitor " << monitorIndex << std::endl;
            monitorIndex++;
            continue;
        }

        // Get the monitor's width and height
        DEVMODE devMode;
        memset(&devMode, 0, sizeof(devMode));
        devMode.dmSize = sizeof(devMode);
        if (EnumDisplaySettings(dd.DeviceName, ENUM_CURRENT_SETTINGS, &devMode)) {
            int width = devMode.dmPelsWidth;
            int height = devMode.dmPelsHeight;

            // Capture screenshot for this monitor
            unsigned char* imgData = nullptr; // Pass nullptr initially
            captureScreenshotForMonitor(hScreenDC, width, height, imgData, images, heights);
        }

        // Release the device context
        DeleteDC(hScreenDC);
        monitorIndex++;
    }

    // After collecting all monitor images, call pipImages
    if (!images.empty()) {

       isPip?pipImages(images, heights, outputFilePath):combineImages(images, heights, outputFilePath);
    } else {
        std::cerr << "No monitors found or screenshots were not captured." << std::endl;
    }
}


// Monitor enumeration callback to capture screenshots from all monitors
// BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
//     std::vector<std::pair<int, unsigned char*>>& images = *reinterpret_cast<std::vector<std::pair<int, unsigned char*>>*>(dwData);
//     std::vector<int>& heights = *reinterpret_cast<std::vector<int>*>(dwData + sizeof(std::vector<std::pair<int, unsigned char*>>));

//     std::cout<<"Screenshots captured\n";
//     int height;
//     images.push_back(captureScreenshot(hMonitor, height));
//     heights.push_back(height);
//     return TRUE;
// }

#elif defined(__APPLE__)
#include <CoreGraphics/CoreGraphics.h>
// Convert BGRA to RGBA
void convertBGRAToRGBA(unsigned char *data, int width, int height)
{
    for (int i = 0; i < width * height; ++i)
    {
        std::swap(data[i * 4], data[i * 4 + 2]); // Swap red and blue
    }
}

// Capture a screenshot of a display and return the image data
std::pair<int, unsigned char *> captureScreenshot(CGDirectDisplayID displayId, int &height)
{
    CGImageRef screenshot = CGDisplayCreateImage(displayId);
    int width = static_cast<int>(CGImageGetWidth(screenshot));
    height = static_cast<int>(CGImageGetHeight(screenshot));

    CFDataRef dataRef = CGDataProviderCopyData(CGImageGetDataProvider(screenshot));
    const UInt8 *data = CFDataGetBytePtr(dataRef);

    unsigned char *imgData = new unsigned char[width * height * 4];
    std::memcpy(imgData, data, width * height * 4);

    CFRelease(dataRef);
    CGImageRelease(screenshot);

    convertBGRAToRGBA(imgData, width, height);

    return std::make_pair(width, imgData);
}
#endif

extern "C"
{
    // Capture screenshots from all monitors and combine them into a single image
    void CaptureScreenshot(const char *baseFilename,bool isPip)
    {
        std::string baseFilepath(baseFilename);
        std::vector<std::pair<int, unsigned char *>> images;
        std::vector<int> heights;

        // std::cout << "Enter Type of Image\n1.Collage\n2.PIP: ";
        // int option;
        // std::cin >> option;

        // std::cout<<"Accumulating monitors\n";
        // #if defined(_WIN32)
        // EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, reinterpret_cast<LPARAM>(&images));
        // #elif defined(__APPLE__)
        // uint32_t displayCount;
        // CGGetActiveDisplayList(0, NULL, &displayCount);
        // CGDirectDisplayID displays[displayCount];
        // CGGetActiveDisplayList(displayCount, displays, &displayCount);

        // for (uint32_t i = 0; i < displayCount; ++i) {
        //     int height;
        //     images.push_back(captureScreenshot(displays[i], height));
        //     heights.push_back(height);
        // }
        // #else
        // std::cerr << "Unsupported platform." << std::endl;
        // return;
        // #endif

        std::string outputFilePath = baseFilepath + "_output.png";

        // if (option == 1)
        //     combineImages(images, heights, outputFilePath);
        // else if (option == 2)
        //     pipImages(images, heights, outputFilePath);

        captureScreenshot(outputFilePath,isPip);
        std::cout << "\nImages captured";
        std::string base64Image = base64encode(outputFilePath);

        std::cout << "\nImages base64 encoded";
        std::string imageKey = "/IFFTImages/" + baseFilepath;
        std::string apiUrl = "https://svcs-dev02.myharmony.com/UserAccountDirectorPlatform/UserAccountDirector.svc/json2/UploadFileToS3Bucket";
        std::cout << "\napi to be hit";
        std::string imageUrl = uploadImageToAPI(apiUrl, imageKey, base64Image);

        postImageToIfttt(imageUrl);
        std::cout << "\nposted to ifttt";

        // Cleanup
        for (auto &image : images)
        {
            delete[] image.second;
        }
    }
}