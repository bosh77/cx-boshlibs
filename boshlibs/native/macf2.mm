#import "ViewController.h"

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    // Inizializza il nome del font con un valore predefinito
    self.selectedFontName = @"Helvetica";
}

- (IBAction)showFontPanel:(id)sender {
    NSFontManager *fontManager = [NSFontManager sharedFontManager];
    // Imposta il tuo oggetto come delegato/target per ricevere le notifiche di cambio font
    [fontManager setTarget:self];
    
    // Assicurati che il tuo oggetto sia il first responder per ricevere l'evento changeFont:
    [[self.view window] makeFirstResponder:self];

    // Ottieni il pannello dei font condiviso e mostralo
    NSFontPanel *fontPanel = [NSFontPanel sharedFontPanel];
    [fontPanel orderFront:nil]; // Mostra la finestra
}

// Questo metodo viene chiamato automaticamente dal NSFontManager quando il font viene cambiato nel pannello
- (void)changeFont:(NSFontManager *)sender {
    // Il sender è NSFontManager. Usa il suo metodo convertFont: per ottenere il nuovo font selezionato.
    // Puoi passare il font attualmente selezionato per applicare le modifiche (es. solo dimensione o stile)
    NSFont *newFont = [sender convertFont:[NSFont fontWithName:self.selectedFontName size:12.0]];
    
    // Ottieni il nome PostScript del font e assegnalo alla variabile stringa
    self.selectedFontName = [newFont postScriptName];
    
    // Opzionale: ottieni anche la dimensione del font
    CGFloat fontSize = [newFont pointSize];
    
    NSLog(@"Nome del font selezionato: %@, Dimensione: %f", self.selectedFontName, fontSize);
    
    // Qui puoi aggiornare la tua UI, ad es. un NSTextField
    // [myTextField setFont:newFont];
}

@end
