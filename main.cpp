/*
 * Author: Tomáš Růžička, t_ruzicka (at) email.cz
 * 2015
 */

#include "headers.h"

// Simple function to test basic biometric functionality
HRESULT TestBasicBiometric()
{
  HRESULT hr = S_OK;
  WINBIO_SESSION_HANDLE sessionHandle = NULL;
  
  std::cout << "Testing basic biometric session...\n";
  
  // Try the absolute simplest session possible
  hr = WinBioOpenSession(
    WINBIO_TYPE_FINGERPRINT,
    WINBIO_POOL_SYSTEM,
    WINBIO_FLAG_BASIC,
    NULL,
    0,
    NULL,
    &sessionHandle
    );

  if (SUCCEEDED(hr))
  {
    std::cout << "SUCCESS: Basic biometric session opened!\n";
    std::cout << "Your fingerprint sensor is working with Windows Biometric Framework.\n";
    
    if(sessionHandle != NULL)
    {
      WinBioCloseSession(sessionHandle);
    }
    return S_OK;
  }
  else
  {
    std::cout << "FAILED: Basic session failed. hr = 0x" << std::hex << hr << std::dec << "\n";
    
    // Try with different flags
    hr = WinBioOpenSession(
      WINBIO_TYPE_FINGERPRINT,
      WINBIO_POOL_SYSTEM,
      0, // No flags
      NULL,
      0,
      NULL,
      &sessionHandle
      );
      
    if (SUCCEEDED(hr))
    {
      std::cout << "SUCCESS: Session opened with no flags!\n";
      if(sessionHandle != NULL)
      {
        WinBioCloseSession(sessionHandle);
      }
      return S_OK;
    }
    else
    {
      std::cout << "FAILED: Session with no flags also failed. hr = 0x" << std::hex << hr << std::dec << "\n";
    }
  }
  
  return hr;
}

HRESULT CheckBiometricService()
{
  // Try to enumerate available biometric units to check if service is running
  WINBIO_UNIT_SCHEMA* unitSchemaArray = NULL;
  SIZE_T unitCount = 0;
  
  HRESULT hr = WinBioEnumBiometricUnits(
    WINBIO_TYPE_FINGERPRINT,
    &unitSchemaArray,
    &unitCount
  );
  
  if (SUCCEEDED(hr))
  {
    std::cout << "Found " << unitCount << " biometric unit(s)\n";
    if (unitSchemaArray != NULL)
    {
      WinBioFree(unitSchemaArray);
    }
  }
  else
  {
    std::cout << "WinBioEnumBiometricUnits failed. hr = 0x" << std::hex << hr << std::dec << "\n";
    std::cout << "This may indicate the Windows Biometric Service is not running or no sensors are available.\n";
  }
  
  return hr;
}

// Function to check Windows Biometric Service status
void CheckServiceStatus()
{
  std::cout << "\nDiagnostic Information:\n";
  std::cout << "======================\n";
  
  // Try to get service status
  SC_HANDLE scManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
  if (scManager)
  {
    SC_HANDLE service = OpenService(scManager, L"WbioSrvc", SERVICE_QUERY_STATUS);
    if (service)
    {
      SERVICE_STATUS status;
      if (QueryServiceStatus(service, &status))
      {
        std::cout << "Windows Biometric Service status: ";
        switch (status.dwCurrentState)
        {
          case SERVICE_RUNNING:
            std::cout << "RUNNING\n";
            break;
          case SERVICE_STOPPED:
            std::cout << "STOPPED (This is the problem!)\n";
            break;
          case SERVICE_START_PENDING:
            std::cout << "STARTING\n";
            break;
          case SERVICE_STOP_PENDING:
            std::cout << "STOPPING\n";
            break;
          default:
            std::cout << "UNKNOWN STATE (" << status.dwCurrentState << ")\n";
            break;
        }
      }
      CloseServiceHandle(service);
    }
    else
    {
      std::cout << "Could not access Windows Biometric Service\n";
    }
    CloseServiceHandle(scManager);
  }
  else
  {
    std::cout << "Could not connect to Service Control Manager\n";
  }
  
  std::cout << "\n";
}

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

  // First check if biometric service is available
  std::cout << "Checking biometric service availability...\n";
  hr = CheckBiometricService();
  
  if (FAILED(hr))
  {
    std::cout << "Biometric service check failed. Attempting to continue anyway...\n";
  }

  // Try the simplest possible approach first - basic session without specific flags
  std::cout << "Attempting basic session opening...\n";

  hr = WinBioOpenSession(
    WINBIO_TYPE_FINGERPRINT,    // Service provider
    WINBIO_POOL_SYSTEM,         // Pool type - try system first
    WINBIO_FLAG_BASIC,          // Access: Most basic flag
    NULL,                       // Array of biometric unit IDs
    0,                          // Count of biometric unit IDs
    NULL,                       // Database ID - NULL for automatic
    &sessionHandle              // [out] Session handle
    );

  if(FAILED(hr))
  {
    std::cout << "WinBioOpenSession (basic system) failed. hr = 0x" << std::hex << hr << std::dec << "\n";
    
    // Try with specific unit array instead of NULL
    WINBIO_UNIT_ID unitArray[16];
    SIZE_T unitCount = 0;
    
    // Get available units first
    WINBIO_UNIT_SCHEMA* unitSchemaArray = NULL;
    SIZE_T schemaCount = 0;
    
    hr = WinBioEnumBiometricUnits(WINBIO_TYPE_FINGERPRINT, &unitSchemaArray, &schemaCount);
    if (SUCCEEDED(hr) && schemaCount > 0)
    {
      std::cout << "Found " << schemaCount << " units, trying specific unit array...\n";
      
      // Copy unit IDs to our array
      for (SIZE_T i = 0; i < schemaCount && i < 16; i++)
      {
        unitArray[i] = unitSchemaArray[i].UnitId;
        unitCount++;
      }
      
      if (unitSchemaArray != NULL)
      {
        WinBioFree(unitSchemaArray);
      }
      
      // Try opening session with specific units
      hr = WinBioOpenSession(
        WINBIO_TYPE_FINGERPRINT,    // Service provider
        WINBIO_POOL_SYSTEM,         // Pool type
        WINBIO_FLAG_BASIC,          // Access: Basic
        unitArray,                  // Array of biometric unit IDs
        unitCount,                  // Count of biometric unit IDs
        NULL,                       // Database ID
        &sessionHandle              // [out] Session handle
        );
    }
    
    if(FAILED(hr))
    {
      std::cout << "All basic session attempts failed. Trying raw capture method...\n";
      goto TryRawCapture;
    }
  }

  std::cout << "Basic session opened successfully!\n";

  // Now try a simple identify operation instead of enrollment
  WINBIO_IDENTITY identity = {};
  WINBIO_BIOMETRIC_SUBTYPE subFactor = 0;
  BOOLEAN isMatch = FALSE;
  
  std::cout << "Please place finger on sensor for identification...\n";
  
  hr = WinBioIdentify(
    sessionHandle,
    &unitId,
    &identity,
    &subFactor,
    &rejectDetail
    );

  if(SUCCEEDED(hr))
  {
    std::cout << "Fingerprint identification successful on unit " << unitId << "!\n";
  }
  else
  {
    if(hr == WINBIO_E_BAD_CAPTURE)
      std::cout << "Bad capture during identification; reason: " << rejectDetail << "\n";
    else if(hr == WINBIO_E_NO_MATCH)
      std::cout << "No match found (this is expected for new fingerprints)\n";
    else
      std::cout << "WinBioIdentify failed. hr = 0x" << std::hex << hr << std::dec << "\n";
  }

  if(sessionHandle != NULL)
  {
    WinBioCloseSession(sessionHandle);
    sessionHandle = NULL;
  }

  // Now try to get raw data for image extraction
  std::cout << "Attempting to capture raw fingerprint data for image...\n";
  goto TryRawCapture;

TryRawCapture:
  // Original raw capture method as fallback
  std::cout << "Trying original WinBioCaptureSample method...\n";

  hr = WinBioOpenSession(
    WINBIO_TYPE_FINGERPRINT,    // Service provider
    WINBIO_POOL_PRIVATE,        // Pool type - use private pool
    WINBIO_FLAG_RAW,            // Access: Capture raw data
    NULL,                       // Array of biometric unit IDs
    0,                          // Count of biometric unit IDs
    NULL,                       // Database ID (NULL for private pool)
    &sessionHandle              // [out] Session handle
    );

  if(FAILED(hr))
  {
    std::cout << "WinBioOpenSession (raw private) failed. hr = 0x" << std::hex << hr << std::dec << "\n";
    
    // Try with system pool as last resort
    std::cout << "Trying with system pool...\n";
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
      std::cout << "WinBioOpenSession (raw system) failed. hr = 0x" << std::hex << hr << std::dec << "\n";
      std::cout << "All session opening methods failed. Please:\n";
      std::cout << "1. Ensure you're running as administrator\n";
      std::cout << "2. Check that Windows Biometric Service is running\n";
      std::cout << "3. Verify that a fingerprint sensor is connected and recognized\n";
      return hr;
    }
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

ProcessImageData:
  std::cout << "Swipe processed - Unit ID: " << unitId << "\n";
  std::cout << "Captured " << sampleSize << " bytes.\n";

  if(sample != NULL)
  {
    PWINBIO_BIR_HEADER BirHeader = (PWINBIO_BIR_HEADER)(((PBYTE)sample) + sample->HeaderBlock.Offset);
    PWINBIO_BDB_ANSI_381_HEADER AnsiBdbHeader = (PWINBIO_BDB_ANSI_381_HEADER)(((PBYTE)sample) + sample->StandardDataBlock.Offset);
    PWINBIO_BDB_ANSI_381_RECORD AnsiBdbRecord = (PWINBIO_BDB_ANSI_381_RECORD)(((PBYTE)AnsiBdbHeader) + sizeof(WINBIO_BDB_ANSI_381_HEADER));

    DWORD width = AnsiBdbRecord->HorizontalLineLength; // Width of image in pixels
    DWORD height = AnsiBdbRecord->VerticalLineLength; // Height of image in pixels

    std::cout << "Image resolution: " << width << " x " << height << "\n";

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
  std::cout << "Windows Biometric Framework Fingerprint Capture Utility\n";
  std::cout << "======================================================\n";
  std::cout << "Note: This application requires administrator privileges and\n";
  std::cout << "      the Windows Biometric Service to be running.\n\n";
  
  // First test basic functionality
  std::cout << "Step 1: Testing basic biometric functionality...\n";
  HRESULT testResult = TestBasicBiometric();
  
  if (FAILED(testResult))
  {
    CheckServiceStatus();
    
    std::cout << "\nBasic biometric test failed. Common solutions:\n";
    std::cout << "1. Run this application as Administrator\n";
    std::cout << "2. Start the Windows Biometric Service:\n";
    std::cout << "   - Press Win+R, type 'services.msc', press Enter\n";
    std::cout << "   - Find 'Windows Biometric Service'\n";
    std::cout << "   - Right-click and select 'Start' if it's stopped\n";
    std::cout << "3. Check Device Manager for fingerprint sensor issues\n";
    std::cout << "4. Ensure Windows Hello is set up in Settings > Accounts > Sign-in options\n\n";
    std::cout << "Press any key to continue anyway...\n";
    std::cin.get();
  }
  else
  {
    std::cout << "\nStep 2: Proceeding with fingerprint capture...\n";
  }
  
  CreateDirectoryA("data", NULL);
  
  while(!FAILED(CaptureSample()))
  {
    std::cout << "\nPress any key to capture another sample, or Ctrl+C to exit...\n";
    std::cin.get();
  }
  
  std::cout << "\nApplication ended due to error or user termination.\n";
  return 0;
}