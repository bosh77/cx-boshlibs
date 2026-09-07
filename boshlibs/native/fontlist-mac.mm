#include <iostream>
#include <vector>
#include <string>
#import <Cocoa/Cocoa.h>

// Function to retrieve all installed system fonts on macOS using Cocoa/AppKit
std::vector<std::string> GetSystemFonts() {
    std::vector<std::string> fontList;
    
    // Objective-C autorelease pool block to manage memory
    @autoreleasepool {
        // Get the shared font manager instance
        NSFontManager *fontManager = [NSFontManager sharedFontManager];
        
        // Get array of available font families
        // returns an NSArray of NSStrings
        NSArray *families = [fontManager availableFontFamilies];
        
        // Iterate through the NSArray
        for (NSString *family in families) {
            // Convert NSString to std::string (UTF-8)
            if (family) {
                fontList.push_back([family UTF8String]);
            }
        }
    }
    
    return fontList;
}

int main() {
    std::cout << "Retrieving list of installed macOS fonts..." << std::endl;
    
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
    
    return 0;
}
