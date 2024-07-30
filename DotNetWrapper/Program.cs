using System;
using System.Runtime.InteropServices;

class Program
{
    [DllImport("libScreenshotPlugin.dylib", CallingConvention = CallingConvention.Cdecl)]
    public static extern void CaptureScreenshot(string filename);

    static void Main(string[] args)
    {
        string folderPath = "./screenshots/";
        string filename = folderPath + "screenshot_" + DateTime.Now.ToString("yyyyMMddHHmmss") + ".png";
        Console.WriteLine("Capturing screenshot to " + filename);
        CaptureScreenshot(filename);
        Console.WriteLine("Screenshot captured.");
    }
}
