/*
 * Author: Tomáš Růžička, t_ruzicka (at) email.cz
 * 2015
 */

#include "headers.h"

void BmpSetImageData(SBmpImage *bmp, const std::vector<uint8> &data, uint32 width, uint32 height)
{
  bmp->data.resize(data.size());

  for(uint32 y = 0; y < height; y++)
  {
    const uint32 yy = (height - y - 1) * width;

    for(uint32 x = 0; x < width; x++)
    {
      const uint32 xy = yy + x;

      bmp->data[xy].r = data[y * width + x];
      bmp->data[xy].g = data[y * width + x];
      bmp->data[xy].b = data[y * width + x];
      bmp->data[xy].a = 255;
    }
  }

  bmp->dataPaddingSize = sizeof(uint32) - ((width * NBmp::BMP_24B_COLORS) % sizeof(uint32));
  if(bmp->dataPaddingSize == sizeof(uint32))
    bmp->dataPaddingSize = 0;

  bmp->signature = NBmp::BMP_HEADER_SIGNATURE;
  bmp->fileSize = NBmp::BMP_HEADER_FILE_DATA_OFFSET + bmp->colorTable.size() * NBmp::BMP_32B_COLORS + (width * NBmp::BMP_24B_COLORS + bmp->dataPaddingSize) * height;
  bmp->reserved = NBmp::BMP_HEADER_RESERVED;
  bmp->dataOffset = NBmp::BMP_HEADER_FILE_DATA_OFFSET + bmp->colorTable.size() * NBmp::BMP_32B_COLORS;
  bmp->infoHeader.headerSize = NBmp::BMP_INFO_HEADER_SIZE;
  bmp->infoHeader.width = width;
  bmp->infoHeader.height = height;
  bmp->infoHeader.planes = NBmp::BMP_INFO_HEADER_PLANES;
  bmp->infoHeader.bitsPerPixel = NBmp::BMP_INFO_HEADER_BITS_PER_PIXEL_24;
  bmp->infoHeader.compression = NBmp::BMP_INFO_HEADER_COMPRESSION;
  bmp->infoHeader.imageSize = (width * NBmp::BMP_24B_COLORS + bmp->dataPaddingSize) * height;
  bmp->infoHeader.pixelPerMeterX = NBmp::BMP_INFO_HEADER_PIXEL_PER_METER_X;
  bmp->infoHeader.pixelPerMeterY = NBmp::BMP_INFO_HEADER_PIXEL_PER_METER_Y;
  bmp->infoHeader.colors = NBmp::BMP_INFO_HEADER_COLORS;
  bmp->infoHeader.usedColors = NBmp::BMP_INFO_HEADER_USED_COLORS;
}

void BmpSave(const SBmpImage *bmp, std::string filename)
{
  const uint32 dataPadding = 0;

  CFile file(filename);

  file.write(&bmp->signature[0], sizeof(char)* NBmp::BMP_HEADER_SIGNATURE_LENGHT);
  file.write(&bmp->fileSize, sizeof(uint32));
  file.write(&bmp->reserved, sizeof(uint32));
  file.write(&bmp->dataOffset, sizeof(uint32));
  file.write(&bmp->infoHeader.headerSize, sizeof(uint32));
  file.write(&bmp->infoHeader.width, sizeof(uint32));
  file.write(&bmp->infoHeader.height, sizeof(uint32));
  file.write(&bmp->infoHeader.planes, sizeof(uint16));
  file.write(&bmp->infoHeader.bitsPerPixel, sizeof(uint16));
  file.write(&bmp->infoHeader.compression, sizeof(uint32));
  file.write(&bmp->infoHeader.imageSize, sizeof(uint32));
  file.write(&bmp->infoHeader.pixelPerMeterX, sizeof(uint32));
  file.write(&bmp->infoHeader.pixelPerMeterY, sizeof(uint32));
  file.write(&bmp->infoHeader.colors, sizeof(uint32));
  file.write(&bmp->infoHeader.usedColors, sizeof(uint32));

  for(uint32 i = 0; i < bmp->colorTable.size(); i++)
  {
    file.write(&bmp->colorTable[i].r, sizeof(uint8));
    file.write(&bmp->colorTable[i].g, sizeof(uint8));
    file.write(&bmp->colorTable[i].b, sizeof(uint8));
    file.write(&bmp->colorTable[i].a, sizeof(uint8));
  }

  for(uint32 y = 0; y < bmp->infoHeader.height; y++)
  {
    const uint32 yy = (bmp->infoHeader.height - y - 1) * bmp->infoHeader.width;

    for(uint32 x = 0; x < bmp->infoHeader.width; x++)
    {
      const uint32 xy = yy + x;

      file.write(&bmp->data[xy].b, sizeof(uint8));
      file.write(&bmp->data[xy].g, sizeof(uint8));
      file.write(&bmp->data[xy].r, sizeof(uint8));
    }

    file.write(&dataPadding, sizeof(uint8)* bmp->dataPaddingSize);
  }

  file.close();
}

HRESULT CaptureSample()
{
  HRESULT hr = S_OK;
  WINBIO_SESSION_HANDLE sessionHandle = NULL;
  WINBIO_UNIT_ID unitId = 0;
  WINBIO_REJECT_DETAIL rejectDetail = 0;
  PWINBIO_BIR sample = NULL;
  SIZE_T sampleSize = 0;

  // Connect to the system pool. 
  hr = WinBioOpenSession(
    WINBIO_TYPE_FINGERPRINT,    // Service provider
    WINBIO_POOL_SYSTEM,         // Pool type
    WINBIO_FLAG_RAW,            // Access: Capture raw data
    NULL,                       // Array of biometric unit IDs
    0,                          // Count of biometric unit IDs
    WINBIO_DB_DEFAULT,          // Default database
    &sessionHandle              // [out] Session handle
    );

  if(FAILED(hr))
  {
    std::cout << "WinBioOpenSession failed. hr = 0x" << std::hex << hr << std::dec;
    
    // Provide more specific error information
    switch(hr)
    {
      case 0x8009802c: // WINBIO_E_ENROLLMENT_IN_PROGRESS
        std::cout << " (WINBIO_E_ENROLLMENT_IN_PROGRESS - Another biometric operation is in progress)\n";
        std::cout << "Try closing Windows Hello setup, biometric settings, or other fingerprint applications.\n";
        break;
      case 0x80090300: // WINBIO_E_UNKNOWN_ID
        std::cout << " (WINBIO_E_UNKNOWN_ID)\n";
        break;
      case 0x80090301: // WINBIO_E_CANCELED
        std::cout << " (WINBIO_E_CANCELED)\n";
        break;
      case 0x80090302: // WINBIO_E_NO_MATCH
        std::cout << " (WINBIO_E_NO_MATCH)\n";
        break;
      case 0x80090303: // WINBIO_E_TIMEOUT
        std::cout << " (WINBIO_E_TIMEOUT)\n";
        break;
      case 0x80090304: // WINBIO_E_BAD_CAPTURE
        std::cout << " (WINBIO_E_BAD_CAPTURE)\n";
        break;
      case 0x80090305: // WINBIO_E_INVALID_CONTROL_CODE
        std::cout << " (WINBIO_E_INVALID_CONTROL_CODE)\n";
        break;
      case 0x80090306: // WINBIO_E_DATA_COLLECTION_IN_PROGRESS
        std::cout << " (WINBIO_E_DATA_COLLECTION_IN_PROGRESS)\n";
        break;
      case 0x80090307: // WINBIO_E_UNSUPPORTED_DATA_FORMAT
        std::cout << " (WINBIO_E_UNSUPPORTED_DATA_FORMAT)\n";
        break;
      case 0x80090308: // WINBIO_E_UNSUPPORTED_DATA_TYPE
        std::cout << " (WINBIO_E_UNSUPPORTED_DATA_TYPE)\n";
        break;
      case 0x80090309: // WINBIO_E_UNSUPPORTED_PURPOSE
        std::cout << " (WINBIO_E_UNSUPPORTED_PURPOSE)\n";
        break;
      case 0x8009030A: // WINBIO_E_INVALID_DEVICE_STATE
        std::cout << " (WINBIO_E_INVALID_DEVICE_STATE)\n";
        break;
      case 0x8009030B: // WINBIO_E_DEVICE_BUSY
        std::cout << " (WINBIO_E_DEVICE_BUSY - Device is currently busy)\n";
        break;
      case 0x8009030C: // WINBIO_E_DATABASE_CANT_CREATE
        std::cout << " (WINBIO_E_DATABASE_CANT_CREATE)\n";
        break;
      case 0x8009030D: // WINBIO_E_DATABASE_CANT_OPEN
        std::cout << " (WINBIO_E_DATABASE_CANT_OPEN)\n";
        break;
      case 0x8009030E: // WINBIO_E_DATABASE_CANT_CLOSE
        std::cout << " (WINBIO_E_DATABASE_CANT_CLOSE)\n";
        break;
      case 0x8009030F: // WINBIO_E_DATABASE_CANT_ERASE
        std::cout << " (WINBIO_E_DATABASE_CANT_ERASE)\n";
        break;
      case 0x80090310: // WINBIO_E_DATABASE_CANT_FIND
        std::cout << " (WINBIO_E_DATABASE_CANT_FIND)\n";
        break;
      case 0x80090311: // WINBIO_E_DATABASE_ALREADY_EXISTS
        std::cout << " (WINBIO_E_DATABASE_ALREADY_EXISTS)\n";
        break;
      case 0x80090312: // WINBIO_E_DATABASE_FULL
        std::cout << " (WINBIO_E_DATABASE_FULL)\n";
        break;
      case 0x80090313: // WINBIO_E_DATABASE_LOCKED
        std::cout << " (WINBIO_E_DATABASE_LOCKED)\n";
        break;
      case 0x80090314: // WINBIO_E_DATABASE_CORRUPTED
        std::cout << " (WINBIO_E_DATABASE_CORRUPTED)\n";
        break;
      case 0x80090315: // WINBIO_E_DATABASE_NO_SUCH_RECORD
        std::cout << " (WINBIO_E_DATABASE_NO_SUCH_RECORD)\n";
        break;
      case 0x80090316: // WINBIO_E_DUPLICATE_TEMPLATE
        std::cout << " (WINBIO_E_DUPLICATE_TEMPLATE)\n";
        break;
      case 0x80090317: // WINBIO_E_INVALID_OPERATION
        std::cout << " (WINBIO_E_INVALID_OPERATION)\n";
        break;
      case 0x80090318: // WINBIO_E_SESSION_BUSY
        std::cout << " (WINBIO_E_SESSION_BUSY)\n";
        break;
      case 0x80070005: // E_ACCESSDENIED
        std::cout << " (E_ACCESSDENIED - Run as Administrator)\n";
        break;
      default:
        std::cout << " (Unknown error)\n";
        break;
    }

    if(sample != NULL)
    {
      WinBioFree(sample);
      sample = NULL;
    }

    if(sessionHandle != NULL)
    {
      WinBioCloseSession(sessionHandle);
      sessionHandle = NULL;
    }
    
    return hr;
  }

  // Capture a biometric sample.
  std::cout << "Calling WinBioCaptureSample - Swipe sensor...\n";

  hr = WinBioCaptureSample(
    sessionHandle,
    WINBIO_NO_PURPOSE_AVAILABLE,
    WINBIO_DATA_FLAG_RAW,
    &unitId,
    &sample,
    &sampleSize,
    &rejectDetail
    );

  if(FAILED(hr))
  {
    if(hr == WINBIO_E_BAD_CAPTURE)
      std:: cout << "Bad capture; reason: " << rejectDetail << "\n";
    else
      std::cout << "WinBioCaptureSample failed.hr = 0x" << std::hex << hr << std::dec << "\n";

    if(sample != NULL)
    {
      WinBioFree(sample);
      sample = NULL;
    }

    if(sessionHandle != NULL)
    {
      WinBioCloseSession(sessionHandle);
      sessionHandle = NULL;
    }

    return hr;
  }

  std::cout << "Swipe processed - Unit ID: " << unitId << "\n";
  std::cout << "Captured " << sampleSize << " bytes.\n";

  if(sample != NULL)
  {
    // Debug: Print structure offsets and sizes
    std::cout << "=== Debug Information ===\n";
    std::cout << "Sample size: " << sampleSize << " bytes\n";
    std::cout << "HeaderBlock.Offset: " << sample->HeaderBlock.Offset << "\n";
    std::cout << "HeaderBlock.Size: " << sample->HeaderBlock.Size << "\n";
    std::cout << "StandardDataBlock.Offset: " << sample->StandardDataBlock.Offset << "\n";
    std::cout << "StandardDataBlock.Size: " << sample->StandardDataBlock.Size << "\n";
    std::cout << "sizeof(WINBIO_BDB_ANSI_381_HEADER): " << sizeof(WINBIO_BDB_ANSI_381_HEADER) << "\n";
    std::cout << "sizeof(WINBIO_BDB_ANSI_381_RECORD): " << sizeof(WINBIO_BDB_ANSI_381_RECORD) << "\n";

    PWINBIO_BIR_HEADER BirHeader = (PWINBIO_BIR_HEADER)(((PBYTE)sample) + sample->HeaderBlock.Offset);
    PWINBIO_BDB_ANSI_381_HEADER AnsiBdbHeader = (PWINBIO_BDB_ANSI_381_HEADER)(((PBYTE)sample) + sample->StandardDataBlock.Offset);
    PWINBIO_BDB_ANSI_381_RECORD AnsiBdbRecord = (PWINBIO_BDB_ANSI_381_RECORD)(((PBYTE)AnsiBdbHeader) + sizeof(WINBIO_BDB_ANSI_381_HEADER));

    // Debug: Print raw bytes around the record
    std::cout << "Raw bytes at AnsiBdbRecord location (first 32 bytes): ";
    PBYTE rawBytes = (PBYTE)AnsiBdbRecord;
    for(int i = 0; i < 32 && i < (int)(sampleSize - ((PBYTE)AnsiBdbRecord - (PBYTE)sample)); i++) {
        std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)rawBytes[i] << " ";
    }
    std::cout << std::dec << "\n";

    // Debug: Print ANSI header info (basic structure info)
    std::cout << "ANSI Header location: " << (void*)AnsiBdbHeader << "\n";
    std::cout << "ANSI Record location: " << (void*)AnsiBdbRecord << "\n";
    
    // Alternative: Try manual parsing of dimensions
    // Skip the header and look for dimension data manually
    PBYTE dataStart = (PBYTE)sample + sample->StandardDataBlock.Offset;
    
    // Common fingerprint dimensions to try
    uint32 possibleDimensions[] = {256, 300, 320, 360, 400, 500, 512};
    uint32 estimatedPixels = sampleSize - sample->StandardDataBlock.Offset - 100; // Rough estimate
    
    std::cout << "Estimated pixel data size: " << estimatedPixels << "\n";
    std::cout << "Possible dimensions based on data size:\n";
    
    for(uint32 w : possibleDimensions) {
        for(uint32 h : possibleDimensions) {
            if(w * h == estimatedPixels || abs((int)(w * h - estimatedPixels)) < 1000) {
                std::cout << "  Possible: " << w << "x" << h << " (diff: " << abs((int)(w * h - estimatedPixels)) << ")\n";
            }
        }
    }

    DWORD width = AnsiBdbRecord->HorizontalLineLength; // Width of image in pixels
    DWORD height = AnsiBdbRecord->VerticalLineLength; // Height of image in pixels

    // If dimensions are 0, try to guess from data size
    if(width == 0 || height == 0) {
        std::cout << "Dimensions are 0, attempting to estimate...\n";
        
        // Common fingerprint scanner resolution
        uint32 totalPixels = estimatedPixels;
        
        // Try common aspect ratios
        for(double ratio = 0.8; ratio <= 1.5; ratio += 0.1) {
            uint32 testWidth = (uint32)sqrt(totalPixels * ratio);
            uint32 testHeight = totalPixels / testWidth;
            
            if(testWidth * testHeight == totalPixels) {
                std::cout << "Potential dimensions: " << testWidth << "x" << testHeight << " (ratio: " << ratio << ")\n";
                if(width == 0) width = testWidth;
                if(height == 0) height = testHeight;
                break;
            }
        }
        
        // Fallback to square or common sizes
        if(width == 0 || height == 0) {
            if(totalPixels == 256*360) { width = 256; height = 360; }
            else if(totalPixels == 300*300) { width = 300; height = 300; }
            else if(totalPixels == 256*256) { width = 256; height = 256; }
            else {
                width = (uint32)sqrt(totalPixels);
                height = totalPixels / width;
            }
        }
    }

    std::cout << "=== Original Values ===\n";
    std::cout << "HorizontalLineLength (raw): 0x" << std::hex << width << std::dec << " (" << width << ")\n";
    std::cout << "VerticalLineLength (raw): 0x" << std::hex << height << std::dec << " (" << height << ")\n";

    // Try reading as different endianness
    uint16* widthPtr = (uint16*)&AnsiBdbRecord->HorizontalLineLength;
    uint16* heightPtr = (uint16*)&AnsiBdbRecord->VerticalLineLength;
    uint16 widthSwapped = (((*widthPtr) & 0xFF) << 8) | (((*widthPtr) >> 8) & 0xFF);
    uint16 heightSwapped = (((*heightPtr) & 0xFF) << 8) | (((*heightPtr) >> 8) & 0xFF);
    
    std::cout << "Width (byte-swapped): " << widthSwapped << "\n";
    std::cout << "Height (byte-swapped): " << heightSwapped << "\n";
    std::cout << "=========================\n";

    PBYTE firstPixel = (PBYTE)((PBYTE)AnsiBdbRecord) + sizeof(WINBIO_BDB_ANSI_381_RECORD);

    SBmpImage bmp;
    std::vector<uint8> data(width * height);
    memcpy(&data[0], firstPixel, width * height);

    SYSTEMTIME st;
    GetSystemTime(&st);
    std::stringstream s;
    s << st.wYear << "." << st.wMonth << "." << st.wDay << "." << st.wHour << "." << st.wMinute << "." << st.wSecond << "." << st.wMilliseconds;
    std::string bmpFile = "data/fingerPrint_"+s.str()+".bmp";

    BmpSetImageData(&bmp, data, width, height);
    BmpSave(&bmp, bmpFile);
    //ShellExecuteA(NULL, NULL, bmpFile.c_str(), NULL, NULL, SW_SHOWNORMAL);

    CFile raw("rawData.bin");
    raw.write(&data[0], data.size());
    raw.close();

    WinBioFree(sample);
    sample = NULL;
  }

  if(sessionHandle != NULL)
  {
    WinBioCloseSession(sessionHandle);
    sessionHandle = NULL;
  }

  return hr;
}

int main()
{
  CreateDirectoryA("data", NULL);
  
  std::cout << "Starting fingerprint capture program...\n";
  std::cout << "Make sure to:\n";
  std::cout << "1. Run as Administrator\n";
  std::cout << "2. Close Windows Settings if biometric settings are open\n";
  std::cout << "3. Close any other fingerprint applications\n";
  std::cout << "Press Enter to continue...";
  std::cin.get();
  
  int attempts = 0;
  const int maxAttempts = 5;
  
  while(attempts < maxAttempts)
  {
    std::cout << "\nAttempt " << (attempts + 1) << " of " << maxAttempts << "...\n";
    HRESULT hr = CaptureSample();
    
    if(SUCCEEDED(hr))
    {
      std::cout << "Capture successful!\n";
      break;
    }
    else if(hr == 0x8009802c) // WINBIO_E_ENROLLMENT_IN_PROGRESS
    {
      std::cout << "Device busy. Waiting 3 seconds before retry...\n";
      Sleep(3000);
      attempts++;
    }
    else
    {
      std::cout << "Failed with different error. Stopping.\n";
      break;
    }
  }
  
  if(attempts >= maxAttempts)
  {
    std::cout << "Max attempts reached. Please try:\n";
    std::cout << "- Restarting the Windows Biometric Service\n";
    std::cout << "- Rebooting your computer\n";
    std::cout << "- Running as Administrator\n";
  }

  std::cout << "\nPress any key to exit...";
  std::cin.get();
  
  return 0;
}