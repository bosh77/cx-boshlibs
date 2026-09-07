#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <set>

// Callback function used by EnumFontFamiliesEx
// This function is called by the system for each font found
int CALLBACK EnumFontFamExProc(
    ENUMLOGFONTEX *lpelfe,    // Pointer to logical font data
    NEWTEXTMETRICEX *lpntme,  // Pointer to physical font data
    DWORD FontType,           // Font type (TrueType, Raster, etc.)
    LPARAM lParam             // Application-defined data (pointer to our set)
) {
    // Cast the lParam back to our set of strings
    std::set<std::string>* pFontNames = (std::set<std::string>*)lParam;
    
    // Get the font face name
    std::string fontName = (char*)lpelfe->elfLogFont.lfFaceName;
    
    // Add to our set (set handles duplicates automatically)
    // We filter out names starting with @ which are usually for vertical writing
    if (!fontName.empty() && fontName[0] != '@') {
        pFontNames->insert(fontName);
    }
    
    // Return non-zero to continue enumeration
    return 1;
}

// Function to retrieve all installed system fonts
std::vector<std::string> GetSystemFonts() {
    std::set<std::string> uniqueFonts;
    
    // Get the device context for the entire screen
    HDC hdc = GetDC(NULL);
    
    // Initialize LOGFONT structure to request all fonts
    LOGFONT lf = { 0 };
    lf.lfCharSet = DEFAULT_CHARSET; // Get all character sets
    lf.lfFaceName[0] = '\0';        // Empty string matches all font names
    
    // Enumerate fonts
    EnumFontFamiliesEx(
        hdc, 
        &lf, 
        (FONTENUMPROC)EnumFontFamExProc, 
        (LPARAM)&uniqueFonts, 
        0
    );
    
    // Release the device context
    ReleaseDC(NULL, hdc);
    
    // Convert set to vector for return
    std::vector<std::string> fontList(uniqueFonts.begin(), uniqueFonts.end());
    return fontList;
}

// Main function to demonstrate usage
int main() {
    std::cout << "Retrieving list of installed Windows fonts..." << std::endl;
    
    std::vector<std::string> fonts = GetSystemFonts();
    
    // Concatenate all fonts into a single string separated by ;
    std::string fontListString = "";
    for (size_t i = 0; i < fonts.size(); ++i) {
        fontListString += fonts[i];
        if (i < fonts.size() - 1) {
            fontListString += ";";
        }
    }
    
    std::cout << "Found " << fonts.size() << " fonts." << std::endl;
    std::cout << "Full list string:" << std::endl;
    std::cout << fontListString << std::endl;
    
    // Wait for user input before closing
    std::cout << "\nPress Enter to exit...";
    std::cin.get();
    
    return 0;
}
