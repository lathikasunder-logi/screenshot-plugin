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

//for hosting
#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/PutObjectRequest.h>
#include <aws/core/utils/Outcome.h>
#include <aws/core/utils/memory/stl/AWSStringStream.h>
#include <fstream>

//for base64 encoding
#include <sstream>
#include <iomanip>


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
void combineImages(const std::vector<std::pair<int, unsigned char*>>& images, const std::vector<int>& heights, const std::string& outputFilePath) {
    if (images.empty()) return;

    // Calculate total width and max height for the collage
    int totalWidth = 0;
    int maxHeight = 0;
    for (size_t i = 0; i < images.size(); ++i) {
        int width = images[i].first;
        int height = heights[i];
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
    int offsetX = mainWidth - pipWidth - 20; // 10 pixels margin from the right edge
    int offsetY = mainHeight - pipHeight - 20; // 10 pixels margin from the bottom edge

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
        offsetY -= pipHeight + 15;
    }

    // Save the final image
    saveImage(outputFilePath, totalWidth, totalHeight, combinedData.get());
}

//encode the image


static const std::string base64_chars = 
             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz"
             "0123456789+/";

std::string base64_encode(unsigned char const* bytes_to_encode, unsigned int in_len) {
    std::string ret;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];

    while (in_len--) {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; i < 4; i++)
                ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i) {
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
std::string base64encode(const std::string& filePath) {
    // Open the file in binary mode
    std::ifstream imageFile(filePath, std::ios::binary);

    if (!imageFile.is_open()) {
        throw std::runtime_error("Could not open file for reading.");
    }

    // Read the contents of the file
    std::vector<unsigned char> imageData((std::istreambuf_iterator<char>(imageFile)), std::istreambuf_iterator<char>());
    
    // Encode the image data to Base64
    return base64_encode(imageData.data(), imageData.size());
}


using json = nlohmann::json;

// Callback function to capture response data
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// Function to upload image data to API and receive response
std::string uploadImageToAPI(const std::string& apiUrl, const std::string& imageKey, const std::string& base64Image) {
    CURL* curl;
    CURLcode res;
    std::string readBuffer;
    std::string imageUrl;

    curl = curl_easy_init();
    if(curl) {
        std::string jsonPayload = "{\"contentType\":\"image/png\", \"imageDataAsBase64String\":\"" + base64Image + "\", \"key\":\"" + imageKey + "\"}";
        
        std::ofstream outFile("request.json");
        if (outFile.is_open()) {
            outFile << jsonPayload;
            outFile.close();
            std::cout << "\nPayload written to request.json" << std::endl;
        } else {
            std::cerr << "\nFailed to open request.json for writing" << std::endl;
        }


        // Set the URL for the POST request
        curl_easy_setopt(curl, CURLOPT_URL, apiUrl.c_str());
        
        // Set the POST data
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonPayload.c_str());

        // Set headers
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, "Authorization: Bearer eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCIsImtpZCI6Ijg5ZDgzZjQ0NDIzZDJiNjQxYTQ3YWJiMTMyNzg2MTJiODQ2OGNkNWQifQ.eyJqdGkiOiIzOGE1OWI4Ny1iZjA0LTQ4ODMtOWRjOS02ODk3MDkzMWI1YjkiLCJ2ZXIiOiIxLjAiLCJhbXIiOlsicHdkIl0sInNjb3BlIjoiIiwiaWF0IjoxNzI0MjM1OTI0LCJleHAiOjE3MjQyMzk1MjQsImF1ZCI6IjYwZWY2MjZlLWJlNjgtNGJiOC1hMjNhLTE3MjgxZGZkYTVhNyIsImlzcyI6Imh0dHBzOi8vc2FuZGJveC5hY2NvdW50cy5sb2dpLmNvbSIsInN1YiI6IjAxZjg4NjA1LWU1MjgtNDcwMi05YzUyLTAzZTE1ODA0ODVkNSJ9.GRappO6AnhzLu9j2Ugwx4GvA6G8_a8OsgFUPLYVSVKWNIRgS9ZQPvw5nkNqjf_1VNNomE1r46e0b39W4IQweUls9htZHe4DTWPoBts74V-CMS4ONhQR5Jc4Y7uZz7ksUyRf5tSJLvasbzdTB6he34fU6j5Nn8JDKSwsNKEirs1UPaQrMHpoLI-XYr9sRMnAm5VSU2etMvqFvEWy7tscVkArE2ZrRkpxBfxyDf1mKMsE5jIipIB1o2mtJg7h1FDrKEv4Z2n-1cYWdC8BhzsMVw1k4UuykLMixlLcQQgVYeaF7Q68sQAu3sah3yDErxhRf4Xe4E6OElYhnqPZV7OlDG6GVzE076chfZ7Ufm5sxU0QSjN4_bknadg9QCpoqT0PqzJo8DCnpsgY0wcrSMggVRkZTJuYbuG_ElCIt56Jy1GjoCGHp2eHgr58o_RLHh5_9EnxAUWKhmOhk5jYc1JLdmEnoYMhR6mCe4_IlArqWii3OXRc_vJ0oJZBexBMhO391vd_viE_vNHxLKmSlBRfI3Ua2IxGOgk15WRFFLQoSP3BaG8_dJd-QqCsBJwV_vFhARFwdnl-VkfpvFObzz51Awraex6xJYBmyvt47ZrHs4Qb-VdQsA-rwSk76nzov-35qpOD02Cak8wCe2irHYKjoUnJXtikYMBBdCWiVDkrxrsU");
        headers = curl_slist_append(headers, "Cookie: .AUTHTOKEN=eyJhdXRoIjp7ImRldmljZSI6WyI4NjA2MCIsIjg3MTgzIl0sInVzZXJuYW1lIjpbImFuYW5kbmlybWFsYUBnbWFpbC5jb20iXSwiYWNjb3VudGlkIjpbIjQxNDYxIiwiNDE5NjIiLCI2MDc2NSJdLCJyZW1vdGUiOlsiNDk4MTIiLCI1MDgzMCJdLCJnbG9iYWxkZXZpY2V2ZXJzaW9uIjpbIjYyNTkzNCJdLCJob3VzZWhvbGRpZCI6WyIzMzA0MiJdLCJkYXRhY29uc2VudCI6WyJGYWxzZSJdfSwibWFjIjoiOVRzVXdrWTN3UmRlaVJyNTFjM013NE10dFJ6R3cxL2lOU2ZHby9EblA0OD0ifQ==; .HarmonyAuth=22A70627E2A1783D36B538F01F91962EC339CA296F710022E1DA7F8B5221EF9F656FEA80F7128C47D4012259D955D8A1C40DB0C2765A948E56E80694994E91BBBE20B798DBB9F0F5E7AA7D228981890CEDB4D5D258A23E1DD77A079DCF33D2AF3CBBE831E5D6F8C195C99D3A1D8576F23BAC423F4AE069C07F020DA5B00CE10D030B3006810FFC5C60999D0E611CCB4A2F4C3D57");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // Set up callback to capture response
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        // Perform the request
        res = curl_easy_perform(curl);
        
        // Check for errors
        if(res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        } else {
            std::cout << "Image uploaded successfully!" << std::endl;
        }


         try {
        json responseJson = json::parse(readBuffer);
        // std::cout<<readBuffer;
        imageUrl=responseJson["UploadFileToS3BucketResult"];
        std::cout << "Image URL: " << imageUrl << std::endl;
        
        } catch (json::parse_error& e) {
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

    // Initialize a curl session
    curl = curl_easy_init();
    if(curl) {
        // Specify the URL for the GET request
        const char* url = "https://maker.ifttt.com/trigger/loupdeckAction/with/key/cAUpJn5crdm1oqUZQSUx7DLJLYS0bRzp7vhrqDccde_?value1="+ imageUrl;

        // Set the URL for the request
        curl_easy_setopt(curl, CURLOPT_URL, url);

        // Perform the request, and res will get the return code
        res = curl_easy_perform(curl);

        // Check for errors
        if(res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        } else {
            std::cout << "GET request sent successfully!" << std::endl;
        }

        // Cleanup the curl session
        curl_easy_cleanup(curl);
    }
    return;
}


#if defined(_WIN32)
// Convert BGRA to RGBA
void convertBGRAToRGBA(unsigned char* data, int width, int height) {
    for (int i = 0; i < width * height; ++i) {
        std::swap(data[i * 4], data[i * 4 + 2]); // Swap red and blue
    }
}

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

    // Convert BGRA to RGBA
    convertBGRAToRGBA(data, width, height);

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
#include <CoreGraphics/CoreGraphics.h>
// Convert BGRA to RGBA
void convertBGRAToRGBA(unsigned char* data, int width, int height) {
    for (int i = 0; i < width * height; ++i) {
        std::swap(data[i * 4], data[i * 4 + 2]); // Swap red and blue
    }
}

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

    convertBGRAToRGBA(imgData, width, height);

    return std::make_pair(width, imgData);
}
#endif

extern "C" {
    // Capture screenshots from all monitors and combine them into a single image
    void CaptureScreenshot(const char* baseFilename) {
        std::string baseFilepath(baseFilename);
        std::vector<std::pair<int, unsigned char*>> images;
        std::vector<int> heights;

        std::cout << "Enter Type of Image\n1.Collage\n2.PIP: ";
        int option;
        std::cin >> option;
        
        #if defined(_WIN32)
        EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, reinterpret_cast<LPARAM>(&images));
        #elif defined(__APPLE__)
        uint32_t displayCount;
        CGGetActiveDisplayList(0, NULL, &displayCount);
        CGDirectDisplayID displays[displayCount];
        CGGetActiveDisplayList(displayCount, displays, &displayCount);

      

        for (uint32_t i = 0; i < displayCount; ++i) {
            int height;
            images.push_back(captureScreenshot(displays[i], height));
            heights.push_back(height);
        }
        #else
        std::cerr << "Unsupported platform." << std::endl;
        return;
        #endif

        std::string outputFilePath = baseFilepath + "_output.png";

        if (option == 1)
            combineImages(images, heights, outputFilePath);
        else if (option == 2)
            pipImages(images, heights, outputFilePath);

         

        std::string base64Image=base64encode(outputFilePath);
        std::string imageKey="/IFFTImages/"+baseFilepath;
        std::string apiUrl="https://svcs-dev02.myharmony.com/UserAccountDirectorPlatform/UserAccountDirector.svc/json2/UploadFileToS3Bucket";
        
        std::string imageUrl=uploadImageToAPI(apiUrl,imageKey, base64Image);
        postImageToIfttt(imageUrl);   
        
        // Cleanup
        for (auto& image : images) {
            delete[] image.second;
        }
    }
}
