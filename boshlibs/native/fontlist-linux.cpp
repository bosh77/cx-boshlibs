#include <iostream>
#include <vector>
#include <string>
#include <set>
#include <fontconfig/fontconfig.h>

// Function to retrieve all installed system fonts on Linux using Fontconfig
std::vector<std::string> GetSystemFonts() {
    std::set<std::string> uniqueFonts;
    
    // Initialize Fontconfig library
    if (!FcInit()) {
        std::cerr << "Can't init font config library" << std::endl;
        return std::vector<std::string>();
    }

    // Create a pattern to match all fonts
    FcPattern* pat = FcPatternCreate();
    
    // Create an object set that specifies we want the family name
    FcObjectSet* os = FcObjectSetBuild(FC_FAMILY, (char *)0);
    
    // Get the list of fonts matching the pattern
    FcFontSet* fs = FcFontList(0, pat, os);
    
    if (fs) {
        for (int i = 0; i < fs->nfont; i++) {
            FcPattern* font = fs->fonts[i];
            FcChar8* family = 0;
            
            // Get the family name
            if (FcPatternGetString(font, FC_FAMILY, 0, &family) == FcResultMatch) {
                // Add to set (handles duplicates automatically)
                if (family) {
                    uniqueFonts.insert(std::string((char*)family));
                }
            }
        }
        // Cleanup font set
        FcFontSetDestroy(fs);
    }
    
    // Cleanup
    FcObjectSetDestroy(os);
    FcPatternDestroy(pat);
    // Note: FcFini() is not strictly necessary in simple programs but good practice if checking for leaks
    // FcFini(); 

    // Convert set to vector for return
    std::vector<std::string> fontList(uniqueFonts.begin(), uniqueFonts.end());
    return fontList;
}

int main() {
    std::cout << "Retrieving list of installed Linux fonts..." << std::endl;
    
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
    
    // Wait for user input before closing (optional on Linux terminal, but keeping consistency)
    std::cout << "\nPress Enter to exit...";
    std::cin.get();
    
    return 0;
}
