using System;
using System.Runtime.InteropServices;
using System.Text.Json;

class Program
{
    // Ensure the name of the DLL matches exactly.
    [DllImport("plugin.dll", CallingConvention = CallingConvention.StdCall)]
    public static extern void CaptureScreenshot(string filename,bool isPip);

    static void Main(string[] args)
    {
        bool isPip=false;
        string jsonFilePath = "events.json"; // Path to your JSON file
        // Read the JSON file
        string jsonString = File.ReadAllText(jsonFilePath);

        // Deserialize the JSON into a dynamic object
        var jsonObject = JsonSerializer.Deserialize<JsonDocument>(jsonString);

        // Access the isPip property
        if (jsonObject != null && jsonObject.RootElement.TryGetProperty("isPip", out var isPipElement))
        {
            isPip = isPipElement.GetBoolean();
            Console.WriteLine($"isPip: {isPip}");
        }
        else
        {
            Console.WriteLine("Property 'isPip' not found in the JSON file.");
        }
        // Construct the filename for the screenshot
        string tempPath=Path.GetTempPath();
        string filenameWithPath = Path.Combine(tempPath,"screenshot_" + DateTime.Now.ToString("yyyyMMddHHmmss"));
        Console.WriteLine("Capturing screenshot to " + filenameWithPath);
        
        // Call the external function
        CaptureScreenshot(filenameWithPath,isPip);
        Console.WriteLine("Screenshot captured.");
    }
}
