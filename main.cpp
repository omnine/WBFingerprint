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
    WINBIO_FLAG_DEFAULT,          // Access: Most basic flag
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
        WINBIO_FLAG_DEFAULT,          // Access: Basic
        unitArray,                  // Array of biometric unit IDs
        unitCount,                  // Count of biometric unit IDs
        NULL,                       // Database ID
        &sessionHandle              // [out] Session handle
        );
    }
    
    if(FAILED(hr))
    {
      std::cout << "All basic session attempts failed.\n";
      std::cout << "Please ensure:\n";
      std::cout << "1. You're running as administrator\n";
      std::cout << "2. Windows Biometric Service is running\n";
      std::cout << "3. Fingerprint sensor is connected and recognized\n";
      return hr;
    }
  }

  std::cout << "Basic session opened successfully!\n";



    // Locate a sensor.
    std::cout << "Locate a sensor by swiping your finger on it...\n";
    hr = WinBioLocateSensor( sessionHandle, &unitId);
    if (FAILED(hr))
    {
        std::cout << "WinBioLocateSensor failed. hr = 0x" << std::hex << hr << std::dec << "\n";
        
        if(sessionHandle != NULL)
        {
          WinBioCloseSession(sessionHandle);
          sessionHandle = NULL;
        }
        return hr;
    }


  // Now try enrollment operations
  std::cout << "Starting enrollment process...\n";
  
  // Begin enrollment
  hr = WinBioEnrollBegin(
    sessionHandle,
    WINBIO_ANSI_381_POS_RH_MIDDLE_FINGER,  // Sub-factor (finger position)
    unitId
    );

  if(FAILED(hr))
  {
    std::cout << "WinBioEnrollBegin failed. hr = 0x" << std::hex << hr << std::dec << "\n";
    
    if(sessionHandle != NULL)
    {
      WinBioCloseSession(sessionHandle);
      sessionHandle = NULL;
    }
    
    std::cout << "Enrollment begin failed. Cannot proceed without enrollment capability.\n";
    return hr;
  }

  std::cout << "Enrollment begun successfully. Please place finger on sensor...\n";

  // Capture enrollment sample
  hr = WinBioEnrollCapture(
    sessionHandle,
    &rejectDetail
    );

  if(FAILED(hr))
  {
    if(hr == WINBIO_E_BAD_CAPTURE)
      std::cout << "Bad capture during enrollment; reason: " << rejectDetail << "\n";
    else
      std::cout << "WinBioEnrollCapture failed. hr = 0x" << std::hex << hr << std::dec << "\n";

    // Cancel enrollment on failure
    WinBioEnrollDiscard(sessionHandle);

    if(sessionHandle != NULL)
    {
      WinBioCloseSession(sessionHandle);
      sessionHandle = NULL;
    }

    std::cout << "Enrollment capture failed. Cannot proceed.\n";
    return hr;
  }

  std::cout << "Enrollment capture successful!\n";

  // Commit the enrollment to get the template
WINBIO_IDENTITY identity = {0};
  BOOLEAN isNewTemplate = TRUE;
  
  // Initialize identity for anonymous enrollment
//  identity.Type = WINBIO_ID_TYPE_WILDCARD;
  
  std::cout << "Committing enrollment...\n";
  hr = WinBioEnrollCommit(
    sessionHandle,
    &identity,
    &isNewTemplate
    );

  if(FAILED(hr))
  {
    std::cout << "WinBioEnrollCommit failed. hr = 0x" << std::hex << hr << std::dec << "\n";
    
    // Provide specific error message for duplicate enrollment
    if (hr == 0x8009801c) // WINBIO_E_DUPLICATE_ENROLLMENT
    {
      std::cout << "Error: This fingerprint is already enrolled in the database.\n";
      std::cout << "This means the sensor successfully recognized your fingerprint!\n";
    }
    
    // Cancel enrollment on failure
    WinBioEnrollDiscard(sessionHandle);

    if(sessionHandle != NULL)
    {
      WinBioCloseSession(sessionHandle);
      sessionHandle = NULL;
    }

    // For duplicate enrollment, consider this a success since it means recognition worked
    if (hr == 0x8009801c) // WINBIO_E_DUPLICATE_ENROLLMENT
    {
      std::cout << "Fingerprint successfully recognized (already enrolled).\n";
      return S_OK;
    }
    
    std::cout << "Enrollment commit failed. Cannot proceed.\n";
    return hr;
  }

  std::cout << "Enrollment committed successfully! New template: " << (isNewTemplate ? "Yes" : "No") << "\n";

  // Discard the enrollment (we don't want to actually save it)
  WinBioEnrollDiscard(sessionHandle);

  if(sessionHandle != NULL)
  {
    WinBioCloseSession(sessionHandle);
    sessionHandle = NULL;
  }

  std::cout << "Fingerprint capture completed successfully using enrollment method.\n";
  return S_OK;
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