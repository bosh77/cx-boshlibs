


Array<int> _CreateText(String txt, String fo, int sz){
    
    HDC hdc = GetDC(GetActiveWindow());

    int fontSize=-sz;

    // Usa la nuova funzione per Unicode
    wstring text = convertBBStringToWide(txt);

    // text="àèìòù";  // Rimuovi questa riga di test
    
    // Utf8ToUtf16(text);
    
    string fontName = convertBBString(fo);

    // string fontName="Vineta BT";
    // // string fontName="Arial";
    // // string fontName="Times New Roman";
    // string text="PROVA CIAO!!";


    // Crea il font
    HFONT hFont = CreateFont(fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, fontName.c_str());
    
                            
                            // HFONT hFont = CreateFont(fontSize, 0, 0, 0, 0, 0, 0, 0,
                            // 0, 0, 0,
                            // 0, 0, fontName.c_str());

    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    
    // Calcola dimensioni del testo
    SIZE textSize;
    GetTextExtentPoint32W(hdc, text.c_str(), text.length(), &textSize);
    
    // Crea bitmap compatibile
    HDC hdcMem = CreateCompatibleDC(hdc);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdc, textSize.cx, textSize.cy);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);
    
    // Riempie sfondo bianco
    RECT rect = {0, 0, textSize.cx, textSize.cy};
    FillRect(hdcMem, &rect, (HBRUSH)GetStockObject(WHITE_BRUSH));
    
    HFONT oldFont = (HFONT)SelectObject(hdc, hFont);
    SelectObject(hdcMem, oldFont);

    // Disegna il testo
    SetTextColor(hdcMem, RGB(0, 0, 0));
    SetBkMode(hdcMem, TRANSPARENT);
    TextOutW(hdcMem, 0, 0, text.c_str(), text.length());
    
    
    // Ottieni dati bitmap
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = textSize.cx;
    bmi.bmiHeader.biHeight = -textSize.cy; // Top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32; // RGB
    bmi.bmiHeader.biCompression = BI_RGB;
    
    int dataSize = textSize.cx * textSize.cy * 4;
    std::vector<uint8_t> pixelData(dataSize);
    
    Array<int> arr{dataSize+2};

    // return arr;
    
    // return alloc;

    GetDIBits(hdc, hBitmap, 0, textSize.cy, pixelData.data(), &bmi, DIB_RGB_COLORS);
    
    // Cleanup
    SelectObject(hdcMem, hOldBitmap);
    SelectObject(hdc, hOldFont);
    DeleteObject(hBitmap);
    DeleteObject(hFont);
    DeleteDC(hdcMem);
    
    // Crea un nuovo BBDataBuffer
    // BBDataBuffer* alloc = new BBDataBuffer();
// int* alloc= new int[10000];



// int* output;

arr[0]=textSize.cx;
arr[1]=textSize.cy;

for (int i = 0; i < dataSize; i++) {
    arr[i+2]= pixelData[i];
    // output.push_back(pixelData[i]);
}
    
    // return Array<int>(alloc);
// arr[0]=textSize.cx;
// arr[1]=textSize.cy;
        // return dynamic_cast<array<int>>(alloc);
return arr;

};



