using System;
using System.Runtime.InteropServices;

class Program
{
    // Ensure the name of the DLL matches exactly.
    [DllImport("plugin.dll", CallingConvention = CallingConvention.StdCall)]
    public static extern void CaptureScreenshot(string filename);

    static void Main(string[] args)
    {
        // Construct the filename for the screenshot
        string filename = "screenshot_" + DateTime.Now.ToString("yyyyMMddHHmmss") + ".png";
        Console.WriteLine("Capturing screenshot to " + filename);
        
        // Call the external function
        CaptureScreenshot(filename);
        Console.WriteLine("Screenshot captured.");
    }
}
