#import <Cocoa/Cocoa.h>

@interface ViewController : NSViewController <NSFontChanging> // Adotta il protocollo NSFontChanging

@property (nonatomic, strong) NSString *selectedFontName;

- (IBAction)showFontPanel:(id)sender;

@end