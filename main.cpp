/*
 * Author: Tomáš Růžička, t_ruzicka (at) email.cz
 * 2015
 */

#include "headers.h"
#include "BioHelper.h"

// Forward declarations
HRESULT DiagnosePrivatePoolConfiguration();
HRESULT RestartBiometricService();
HRESULT ForceReconfigurePrivatePool();
HRESULT CheckForESSensor();

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
    SC_HANDLE service = OpenService(scManager, TEXT("WbioSrvc"), SERVICE_QUERY_STATUS);
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

  std::cout << "Sensor located successfully on unit ID: " << unitId << "\n";

  // Test identification to see if any enrolled fingerprints match
  std::cout << "Testing identification - please place finger on sensor...\n";
  
  WINBIO_IDENTITY identifyIdentity = {0};
  WINBIO_BIOMETRIC_SUBTYPE identifySubFactor = 0;
  WINBIO_REJECT_DETAIL identifyRejectDetail = 0;
  WINBIO_UNIT_ID identifyUnitId = 0;
  
  hr = WinBioIdentify(
    sessionHandle,
    &identifyUnitId,
    &identifyIdentity,
    &identifySubFactor,
    &identifyRejectDetail
    );

  if (SUCCEEDED(hr))
  {
    std::cout << "Identification successful!\n";
    std::cout << "Found matching identity on unit ID: " << identifyUnitId << "\n";
    std::cout << "Identity type: " << identifyIdentity.Type << "\n";
    if (identifyIdentity.Type == WINBIO_ID_TYPE_SID)
    {
      std::cout << "Identity: Windows user account (SID)\n";
    }
    else if (identifyIdentity.Type == WINBIO_ID_TYPE_GUID)
    {
      std::cout << "Identity: GUID\n";
    }
    else
    {
      std::cout << "Identity: Other type\n";
    }
    std::cout << "Sub-factor (finger position): " << (int)identifySubFactor << "\n";
    
    // Test verification immediately against the identified fingerprint
    std::cout << "Testing verification against the identified fingerprint...\n";
    std::cout << "Please place the same finger on sensor again for verification...\n";
    
    BOOLEAN verifyMatch = FALSE;
    WINBIO_REJECT_DETAIL verifyRejectDetail = 0;
    WINBIO_UNIT_ID verifyUnitId = 0;
    
    HRESULT verifyHr = WinBioVerify(
      sessionHandle,
      &identifyIdentity,
      identifySubFactor,
      &verifyUnitId,
      &verifyMatch,
      &verifyRejectDetail
      );

    if (SUCCEEDED(verifyHr))
    {
      std::cout << "Verification successful! Match: " << (verifyMatch ? "YES" : "NO") << "\n";
      if (verifyMatch)
      {
        std::cout << "Fingerprint successfully verified against the identified enrollment!\n";
        std::cout << "Verification completed on unit ID: " << verifyUnitId << "\n";
      }
      else
      {
        std::cout << "Fingerprint did not match the identified enrollment.\n";
      }
    }
    else
    {
      if (verifyHr == WINBIO_E_BAD_CAPTURE)
        std::cout << "Bad capture during verification; reason: " << verifyRejectDetail << "\n";
      else
        std::cout << "WinBioVerify failed. hr = 0x" << std::hex << verifyHr << std::dec << "\n";
      
      std::cout << "Verification failed, but continuing...\n";
    }
  }
  else
  {
    if (hr == WINBIO_E_BAD_CAPTURE)
      std::cout << "Bad capture during identification; reason: " << identifyRejectDetail << "\n";
    else if (hr == WINBIO_E_NO_MATCH)
      std::cout << "No matching enrolled fingerprint found (this is expected for new users).\n";
    else
      std::cout << "WinBioIdentify failed. hr = 0x" << std::hex << hr << std::dec << "\n";
    
    std::cout << "Identification failed or no match found. Proceeding with enrollment...\n";
  }

  // Now try enrollment operations
  std::cout << "Starting enrollment process...\n";
  
  // Begin enrollment
  hr = WinBioEnrollBegin(
    sessionHandle,
    WINBIO_FINGER_UNSPECIFIED_POS_01,  // Sub-factor (finger position)
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
      std::cout << "Identity Type: " << identity.Type << "\n";
      return S_OK;
    }
    
    std::cout << "Enrollment commit failed. Cannot proceed.\n";
    return hr;
  }

  std::cout << "Enrollment committed successfully! New template: " << (isNewTemplate ? "Yes" : "No") << "\n";

  // Test verification with the enrolled identity
  std::cout << "Testing verification - please place the same finger on sensor again...\n";
  
  BOOLEAN match = FALSE;
  WINBIO_REJECT_DETAIL verifyRejectDetail = 0;
  WINBIO_UNIT_ID verifyUnitId = 0;
  
  hr = WinBioVerify(
    sessionHandle,
    &identity,
    WINBIO_FINGER_UNSPECIFIED_POS_01,
    &verifyUnitId,
    &match,
    &verifyRejectDetail
    );

  if (SUCCEEDED(hr))
  {
    std::cout << "Verification successful! Match: " << (match ? "YES" : "NO") << "\n";
    if (match)
    {
      std::cout << "Fingerprint successfully verified against the enrolled identity!\n";
      std::cout << "Verification completed on unit ID: " << verifyUnitId << "\n";
    }
    else
    {
      std::cout << "Fingerprint did not match the enrolled identity.\n";
    }
  }
  else
  {
    if (hr == WINBIO_E_BAD_CAPTURE)
      std::cout << "Bad capture during verification; reason: " << verifyRejectDetail << "\n";
    else
      std::cout << "WinBioVerify failed. hr = 0x" << std::hex << hr << std::dec << "\n";
    
    std::cout << "Verification failed, but enrollment was successful.\n";
  }

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

// Check if sensor is ESS (Enhanced Sign-in Security) which cannot be used for private pools
HRESULT CheckForESSensor()
{
  std::cout << "\n=== ESS Sensor Detection ===\n";
  
  WINBIO_UNIT_SCHEMA *unitArray = NULL;
  SIZE_T unitCount = 0;
  HRESULT hr = WinBioEnumBiometricUnits(WINBIO_TYPE_FINGERPRINT, &unitArray, &unitCount);
  
  if (SUCCEEDED(hr) && unitCount > 0)
  {
    for (SIZE_T i = 0; i < unitCount; ++i)
    {
      std::cout << "Sensor " << i << ": " << unitArray[i].Description << "\n";
      std::cout << "  Manufacturer: " << unitArray[i].Manufacturer << "\n";
      std::cout << "  Model: " << unitArray[i].Model << "\n";
      std::cout << "  Serial Number: " << unitArray[i].SerialNumber << "\n";
      std::cout << "  Device ID: " << unitArray[i].DeviceInstanceId << "\n";
      std::cout << "  Pool Type: 0x" << std::hex << unitArray[i].PoolType << std::dec;
      
      // Check for ESS indicators
      bool isESS = false;
      
      // Common ESS indicators in device descriptions - handle wide char conversion
      std::wstring desc = unitArray[i].Description ? unitArray[i].Description : L"";
      std::wstring mfg = unitArray[i].Manufacturer ? unitArray[i].Manufacturer : L"";
      std::wstring model = unitArray[i].Model ? unitArray[i].Model : L"";
      
      // Convert to lowercase for case-insensitive comparison
      std::transform(desc.begin(), desc.end(), desc.begin(), ::tolower);
      std::transform(mfg.begin(), mfg.end(), mfg.begin(), ::tolower);
      std::transform(model.begin(), model.end(), model.begin(), ::tolower);
      
      // ESS sensors often have these characteristics:
      if (desc.find(L"sign-in") != std::wstring::npos ||
          desc.find(L"signin") != std::wstring::npos ||
          desc.find(L"hello") != std::wstring::npos ||
          desc.find(L"security") != std::wstring::npos ||
          desc.find(L"enhanced") != std::wstring::npos ||
          model.find(L"ess") != std::wstring::npos ||
          model.find(L"hello") != std::wstring::npos)
      {
        isESS = true;
      }
      
      // Additional check: ESS sensors are typically restricted to system pool only
      if (unitArray[i].PoolType == WINBIO_POOL_SYSTEM)
      {
        std::cout << " (SYSTEM)";
        
        // Try to detect if it's an ESS sensor by attempting to read capabilities
        // ESS sensors often have restricted capabilities
        if (isESS)
        {
          std::cout << "\n  *** LIKELY ESS SENSOR DETECTED ***";
          std::cout << "\n  ESS (Enhanced Sign-in Security) sensors have restrictions:";
          std::cout << "\n  - Cannot be configured for private pools";
          std::cout << "\n  - Limited to Windows Hello authentication only";
          std::cout << "\n  - Hardware-enforced security prevents third-party access";
        }
      }
      else
      {
        std::cout << " (NON-SYSTEM)";
      }
      
      std::cout << "\n\n";
    }
    
    WinBioFree(unitArray);
    
    std::cout << "ESS Detection Summary:\n";
    std::cout << "======================\n";
    std::cout << "If your sensor is ESS (Enhanced Sign-in Security):\n";
    std::cout << "1. Private pools are NOT supported by design\n";
    std::cout << "2. The sensor is restricted to Windows Hello only\n";
    std::cout << "3. This is a hardware/firmware limitation, not a software issue\n";
    std::cout << "4. You'll need a different sensor for private pool testing\n\n";
    std::cout << "Alternative solutions:\n";
    std::cout << "- Use WINBIO_POOL_SYSTEM for basic biometric operations\n";
    std::cout << "- Purchase a non-ESS fingerprint sensor for development\n";
    std::cout << "- Use Windows Hello APIs instead of WinBio for ESS sensors\n";
  }
  else
  {
    std::cout << "Failed to enumerate sensors for ESS detection.\n";
  }
  
  return hr;
}

// Check if private pool database is already installed
bool IsPrivatePoolDatabaseInstalled()
{
  WINBIO_STORAGE_SCHEMA *storageArray = NULL;
  SIZE_T storageCount = 0;
  bool found = false;
  
  HRESULT hr = WinBioEnumDatabases( WINBIO_TYPE_FINGERPRINT, &storageArray, &storageCount );
  if (SUCCEEDED(hr))
  {
    for (SIZE_T i = 0; i < storageCount; ++i)
    {
      if (storageArray[i].DatabaseId == PRIVATE_POOL_DATABASE_ID)
      {
        found = true;
        break;
      }
    }
    WinBioFree(storageArray);
  }
  
  return found;
}

// Helper function to restart Windows Biometric Service
HRESULT RestartBiometricService()
{
  std::cout << "Attempting to restart Windows Biometric Service...\n";
  
  SC_HANDLE scManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
  if (!scManager)
  {
    std::cout << "Failed to open Service Control Manager. Error: " << GetLastError() << "\n";
    return HRESULT_FROM_WIN32(GetLastError());
  }
  
  SC_HANDLE service = OpenService(scManager, TEXT("WbioSrvc"), SERVICE_ALL_ACCESS);
  if (!service)
  {
    std::cout << "Failed to open Windows Biometric Service. Error: " << GetLastError() << "\n";
    CloseServiceHandle(scManager);
    return HRESULT_FROM_WIN32(GetLastError());
  }
  
  // Stop the service
  SERVICE_STATUS status;
  std::cout << "Stopping Windows Biometric Service...\n";
  if (ControlService(service, SERVICE_CONTROL_STOP, &status))
  {
    // Wait for service to stop
    int attempts = 0;
    while (attempts < 30) // Wait up to 30 seconds
    {
      if (QueryServiceStatus(service, &status) && 
          status.dwCurrentState == SERVICE_STOPPED)
      {
        break;
      }
      Sleep(1000);
      attempts++;
    }
    
    if (status.dwCurrentState != SERVICE_STOPPED)
    {
      std::cout << "Service did not stop within timeout period.\n";
    }
    else
    {
      std::cout << "Service stopped successfully.\n";
    }
  }
  else
  {
    DWORD error = GetLastError();
    if (error == ERROR_SERVICE_NOT_ACTIVE)
    {
      std::cout << "Service was already stopped.\n";
    }
    else
    {
      std::cout << "Failed to stop service. Error: " << error << "\n";
    }
  }
  
  // Start the service
  std::cout << "Starting Windows Biometric Service...\n";
  if (StartService(service, 0, NULL))
  {
    // Wait for service to start
    int attempts = 0;
    while (attempts < 30) // Wait up to 30 seconds
    {
      if (QueryServiceStatus(service, &status) && 
          status.dwCurrentState == SERVICE_RUNNING)
      {
        break;
      }
      Sleep(1000);
      attempts++;
    }
    
    if (status.dwCurrentState == SERVICE_RUNNING)
    {
      std::cout << "Windows Biometric Service restarted successfully!\n";
    }
    else
    {
      std::cout << "Service start timeout. Current state: " << status.dwCurrentState << "\n";
    }
  }
  else
  {
    DWORD error = GetLastError();
    std::cout << "Failed to start service. Error: " << error << "\n";
    CloseServiceHandle(service);
    CloseServiceHandle(scManager);
    return HRESULT_FROM_WIN32(error);
  }
  
  CloseServiceHandle(service);
  CloseServiceHandle(scManager);
  return S_OK;
}

// Setup private pool by installing database and configuring sensor
HRESULT SetupPrivatePool()
{
  std::cout << "\n=== Setting up Private Pool ===\n";
  
  // Check if already installed
  if (IsPrivatePoolDatabaseInstalled())
  {
    std::cout << "Private database already installed. Checking configuration...\n";
    
    // Run diagnostics to see if configuration is complete
    HRESULT diagHr = DiagnosePrivatePoolConfiguration();
    if (SUCCEEDED(diagHr))
    {
      std::cout << "Private pool appears to be properly configured.\n";
      return S_OK;
    }
    else
    {
      std::cout << "Private database found but configuration appears incomplete. Proceeding with setup...\n";
    }
  }
  
  // Enumerate available sensors
  WINBIO_UNIT_SCHEMA *unitArray = NULL;
  SIZE_T unitCount = 0;
  HRESULT hr = WinBioEnumBiometricUnits( 
                  WINBIO_TYPE_FINGERPRINT, 
                  &unitArray, 
                  &unitCount 
                  );
  if (FAILED(hr))
  {
    std::cout << "Failed to enumerate biometric units. hr = 0x" << std::hex << hr << std::dec << "\n";
    return hr;
  }
  
  if (unitCount == 0)
  {
    std::cout << "No biometric units found.\n";
    WinBioFree(unitArray);
    return E_FAIL;
  }
  
  std::cout << "Found " << unitCount << " biometric unit(s). Using the first one for private pool setup.\n";
  
  // Use the first available sensor for setup
  WINBIO_UNIT_SCHEMA* selectedUnit = &unitArray[0];
  
  // Create compatible configuration from the selected sensor
  BioHelper::POOL_CONFIGURATION derivedConfig = {};
  hr = BioHelper::CreateCompatibleConfiguration(selectedUnit, &derivedConfig);
  
  if (SUCCEEDED(hr))
  {
    // Register the private database
    WINBIO_STORAGE_SCHEMA storageSchema = {};
    storageSchema.DatabaseId = PRIVATE_POOL_DATABASE_ID;
    storageSchema.DataFormat = derivedConfig.DataFormat;
    storageSchema.Attributes = derivedConfig.DatabaseAttributes;
    
    std::cout << "Registering private database...\n";
    hr = BioHelper::RegisterDatabase(&storageSchema);
    
    if (FAILED(hr))
    {
      if (hr == 0x80098016) // WINBIO_E_DATABASE_ALREADY_EXISTS
      {
        std::cout << "Database already exists. Proceeding with sensor configuration...\n";
        hr = S_OK; // Continue with sensor configuration
      }
      else
      {
        std::cout << "Failed to register private database. hr = 0x" << std::hex << hr << std::dec << "\n";
        WinBioFree(unitArray);
        return hr;
      }
    }
    else
    {
      std::cout << "Private database registered successfully.\n";
    }
    
    // Configure the sensor for the private pool (always attempt this)
    std::cout << "Configuring sensor for private pool...\n";
    derivedConfig.DatabaseId = PRIVATE_POOL_DATABASE_ID;
    
    // First try to remove any existing configuration to avoid conflicts
    std::cout << "Removing any existing private configuration from sensor...\n";
    bool configRemoved = false;
    HRESULT removeHr = BioHelper::UnregisterPrivateConfiguration( 
                            selectedUnit, 
                            (WINBIO_UUID*)&PRIVATE_POOL_DATABASE_ID, 
                            &configRemoved 
                            );
    if (SUCCEEDED(removeHr) && configRemoved)
    {
      std::cout << "Removed existing private configuration.\n";
    }
    else if (SUCCEEDED(removeHr))
    {
      std::cout << "No existing private configuration found to remove.\n";
    }
    else
    {
      std::cout << "Warning: Failed to remove existing configuration. hr = 0x" << std::hex << removeHr << std::dec << "\n";
    }
    
    // Now try to register the new configuration
    hr = BioHelper::RegisterPrivateConfiguration(selectedUnit, &derivedConfig);
    
    if (SUCCEEDED(hr))
    {
      std::cout << "Private pool setup completed successfully!\n";
      std::cout << "Sensor: " << selectedUnit->Description << " (" << selectedUnit->Manufacturer << ")\n";
      
      // Restart the Windows Biometric Service to apply changes
      std::cout << "\nRestarting Windows Biometric Service to apply configuration...\n";
      HRESULT restartHr = RestartBiometricService();
      if (FAILED(restartHr))
      {
        std::cout << "WARNING: Failed to restart Windows Biometric Service.\n";
        std::cout << "You may need to manually restart the service or reboot for changes to take effect.\n";
      }
      else
      {
        std::cout << "Service restart completed. Private pool should be ready for use.\n";
        
        // Wait a moment for service to fully initialize
        std::cout << "Waiting for service to initialize...\n";
        Sleep(2000);
        
        // Re-run diagnostics to verify configuration
        std::cout << "\nVerifying configuration after service restart...\n";
        HRESULT verifyHr = DiagnosePrivatePoolConfiguration();
        if (FAILED(verifyHr))
        {
          std::cout << "WARNING: Configuration verification failed after setup.\n";
          std::cout << "The private pool may not be fully functional.\n";
        }
        else
        {
          std::cout << "Configuration verification successful!\n";
        }
      }
    }
    else
    {
      std::cout << "Failed to register private configuration. hr = 0x" << std::hex << hr << std::dec << "\n";
      
      if (hr == 0x80098033) // WINBIO_E_CONFIGURATION_FAILURE
      {
        std::cout << "ERROR: WINBIO_E_CONFIGURATION_FAILURE - This indicates a sensor configuration conflict.\n";
        std::cout << "SOLUTION: Try using 'Force Reconfigure Private Pool' (Option 7) which removes all\n";
        std::cout << "          existing configurations before setting up the private pool.\n";
        std::cout << "\nAlternative solutions:\n";
        std::cout << "1. Restart Windows Biometric Service (Option 6)\n";
        std::cout << "2. Clean up Private Pool (Option 4) then Setup again (Option 3)\n";
        std::cout << "3. Reboot the system to clear all biometric service state\n";
      }
      
      // Clean up database if sensor config failed (only if we just created it)
      if (hr != 0x80098016) // Don't clean up if database already existed
      {
        std::cout << "Cleaning up database registration due to sensor configuration failure...\n";
        BioHelper::UnregisterDatabase((WINBIO_UUID*)&PRIVATE_POOL_DATABASE_ID);
      }
    }
  }
  else
  {
    std::cout << "Failed to create compatible configuration. hr = 0x" << std::hex << hr << std::dec << "\n";
  }
  
  WinBioFree(unitArray);
  return hr;
}

// Force reconfigure private pool - removes and re-adds sensor configurations
HRESULT ForceReconfigurePrivatePool()
{
  std::cout << "\n=== Force Reconfiguring Private Pool ===\n";
  
  if (!IsPrivatePoolDatabaseInstalled())
  {
    std::cout << "Private database not found. Running full setup first...\n";
    return SetupPrivatePool();
  }
  
  std::cout << "Removing existing sensor configurations...\n";
  
  // Enumerate all sensors and remove any existing private configurations
  WINBIO_UNIT_SCHEMA *unitArray = NULL;
  SIZE_T unitCount = 0;
  HRESULT hr = WinBioEnumBiometricUnits( 
                  WINBIO_TYPE_FINGERPRINT, 
                  &unitArray, 
                  &unitCount 
                  );
  if (SUCCEEDED(hr) && unitCount > 0)
  {
    for (SIZE_T i = 0; i < unitCount; ++i)
    {
      bool configRemoved = false;
      HRESULT removeHr = BioHelper::UnregisterPrivateConfiguration( 
                              &unitArray[i], 
                              (WINBIO_UUID*)&PRIVATE_POOL_DATABASE_ID, 
                              &configRemoved 
                              );
      if (SUCCEEDED(removeHr) && configRemoved)
      {
        std::cout << "Removed existing configuration from: " << unitArray[i].Description << "\n";
      }
    }
    
    std::cout << "Re-adding sensor configuration for private pool...\n";
    
    // Use the first available sensor for reconfiguration
    WINBIO_UNIT_SCHEMA* selectedUnit = &unitArray[0];
    
    // Create compatible configuration
    BioHelper::POOL_CONFIGURATION derivedConfig = {};
    hr = BioHelper::CreateCompatibleConfiguration(selectedUnit, &derivedConfig);
    
    if (SUCCEEDED(hr))
    {
      // Configure the sensor for the private pool
      derivedConfig.DatabaseId = PRIVATE_POOL_DATABASE_ID;
      hr = BioHelper::RegisterPrivateConfiguration(selectedUnit, &derivedConfig);
      
      if (SUCCEEDED(hr))
      {
        std::cout << "Sensor reconfigured successfully: " << selectedUnit->Description << "\n";
        
        // Restart service to apply changes
        std::cout << "Restarting Windows Biometric Service...\n";
        HRESULT restartHr = RestartBiometricService();
        if (SUCCEEDED(restartHr))
        {
          Sleep(3000); // Wait longer for full service initialization
          
          std::cout << "Verifying reconfiguration...\n";
          HRESULT verifyHr = DiagnosePrivatePoolConfiguration();
          if (SUCCEEDED(verifyHr))
          {
            std::cout << "SUCCESS: Private pool reconfiguration completed!\n";
          }
          else
          {
            std::cout << "WARNING: Reconfiguration may not have taken effect properly.\n";
          }
        }
      }
      else
      {
        std::cout << "Failed to reconfigure sensor. hr = 0x" << std::hex << hr << std::dec << "\n";
      }
    }
    else
    {
      std::cout << "Failed to create compatible configuration. hr = 0x" << std::hex << hr << std::dec << "\n";
    }
    
    WinBioFree(unitArray);
  }
  else
  {
    std::cout << "Failed to enumerate biometric units. hr = 0x" << std::hex << hr << std::dec << "\n";
  }
  
  return hr;
}

// Clean up private pool by removing sensor configurations and database
HRESULT CleanupPrivatePool()
{
  std::cout << "\n=== Cleaning up Private Pool ===\n";
  
  // Check if database is installed
  if (!IsPrivatePoolDatabaseInstalled())
  {
    std::cout << "Private database not found. Nothing to clean up.\n";
    return S_OK;
  }
  
  // Enumerate all sensors and remove private configurations
  WINBIO_UNIT_SCHEMA *unitArray = NULL;
  SIZE_T unitCount = 0;
  HRESULT hr = WinBioEnumBiometricUnits( 
                  WINBIO_TYPE_FINGERPRINT, 
                  &unitArray, 
                  &unitCount 
                  );
  if (SUCCEEDED(hr))
  {
    for (SIZE_T i = 0; i < unitCount; ++i)
    {
      bool configRemoved = false;
      HRESULT removeHr = BioHelper::UnregisterPrivateConfiguration( 
                              &unitArray[i], 
                              (WINBIO_UUID*)&PRIVATE_POOL_DATABASE_ID, 
                              &configRemoved 
                              );
      if (SUCCEEDED(removeHr) && configRemoved)
      {
        std::cout << "Removed sensor from private pool: " << unitArray[i].Description << "\n";
      }
    }
    WinBioFree(unitArray);
  }
  
  // Remove the database
  std::cout << "Removing private database...\n";
  hr = BioHelper::UnregisterDatabase((WINBIO_UUID*)&PRIVATE_POOL_DATABASE_ID);
  
  if (SUCCEEDED(hr))
  {
    std::cout << "Private pool cleanup completed successfully!\n";
  }
  else
  {
    std::cout << "Failed to remove private database. hr = 0x" << std::hex << hr << std::dec << "\n";
  }
  
  return hr;
}

// Enhanced diagnostic function for private pool troubleshooting
HRESULT DiagnosePrivatePoolConfiguration()
{
  std::cout << "\n=== Private Pool Configuration Diagnostics ===\n";
  
  // Check if database is registered
  bool dbInstalled = IsPrivatePoolDatabaseInstalled();
  std::cout << "Private database installed: " << (dbInstalled ? "YES" : "NO") << "\n";
  
  if (!dbInstalled)
  {
    std::cout << "ERROR: Private database not found in system registry.\n";
    return E_FAIL;
  }
  
  // Enumerate all databases to verify our private database appears
  WINBIO_STORAGE_SCHEMA *storageArray = NULL;
  SIZE_T storageCount = 0;
  HRESULT hr = WinBioEnumDatabases(WINBIO_TYPE_FINGERPRINT, &storageArray, &storageCount);
  
  if (SUCCEEDED(hr))
  {
    std::cout << "Total databases found: " << storageCount << "\n";
    bool foundPrivateDb = false;
    
    for (SIZE_T i = 0; i < storageCount; ++i)
    {
      if (storageArray[i].DatabaseId == PRIVATE_POOL_DATABASE_ID)
      {
        foundPrivateDb = true;
        std::cout << "Private database found at index " << i << ":\n";
        std::cout << "  Database ID: {" << std::hex << storageArray[i].DatabaseId.Data1 << "...}\n" << std::dec;
        std::cout << "  Data Format: {" << std::hex << storageArray[i].DataFormat.Data1 << "...}\n" << std::dec;
        std::cout << "  Attributes: 0x" << std::hex << storageArray[i].Attributes << std::dec << "\n";
        std::cout << "  File Path: " << (storageArray[i].FilePath ? "Present" : "NULL") << "\n";
        std::cout << "  Connection String: " << (storageArray[i].ConnectionString ? "Present" : "NULL") << "\n";
        break;
      }
    }
    
    if (!foundPrivateDb)
    {
      std::cout << "ERROR: Private database ID not found in enumeration!\n";
      WinBioFree(storageArray);
      return E_FAIL;
    }
    
    WinBioFree(storageArray);
  }
  else
  {
    std::cout << "Failed to enumerate databases. hr = 0x" << std::hex << hr << std::dec << "\n";
    return hr;
  }
  
  // Check sensor configurations
  WINBIO_UNIT_SCHEMA *unitArray = NULL;
  SIZE_T unitCount = 0;
  hr = WinBioEnumBiometricUnits(WINBIO_TYPE_FINGERPRINT, &unitArray, &unitCount);
  
  if (SUCCEEDED(hr))
  {
    std::cout << "Checking sensor configurations for private pool...\n";
    bool foundConfiguredSensor = false;
    
    for (SIZE_T i = 0; i < unitCount; ++i)
    {
      std::cout << "Unit " << i << ": " << unitArray[i].Description << " (ID: " << unitArray[i].UnitId << ")\n";
      std::cout << "  Pool Type: 0x" << std::hex << unitArray[i].PoolType << std::dec;
      
      if (unitArray[i].PoolType == WINBIO_POOL_PRIVATE)
      {
        std::cout << " (PRIVATE)";
        foundConfiguredSensor = true;
      }
      else if (unitArray[i].PoolType == WINBIO_POOL_SYSTEM)
      {
        std::cout << " (SYSTEM)";
      }
      else
      {
        std::cout << " (UNKNOWN)";
      }
      std::cout << "\n";
    }
    
    if (!foundConfiguredSensor)
    {
      std::cout << "WARNING: No sensors configured for private pool!\n";
      std::cout << "This is the cause of WINBIO_E_CONFIGURATION_FAILURE.\n";
      std::cout << "\nTroubleshooting steps:\n";
      std::cout << "1. Run 'Setup Private Pool' (Option 3) to configure sensors\n";
      std::cout << "2. Restart Windows Biometric Service (Option 6)\n";
      std::cout << "3. If problems persist, try:\n";
      std::cout << "   - Cleanup Private Pool (Option 4)\n";
      std::cout << "   - Setup Private Pool again (Option 3)\n";
      std::cout << "   - Reboot the system\n";
      return E_FAIL;
    }
    else
    {
      std::cout << "SUCCESS: Found " << (foundConfiguredSensor ? 1 : 0) << " sensor(s) configured for private pool.\n";
    }
    
    WinBioFree(unitArray);
  }
  else
  {
    std::cout << "Failed to enumerate biometric units. hr = 0x" << std::hex << hr << std::dec << "\n";
    return hr;
  }
  
  std::cout << "Configuration diagnostics completed.\n";
  return S_OK;
}

// Test private pool functionality
HRESULT TestPrivatePool()
{
  std::cout << "\n=== Testing Private Pool ===\n";
  
  // Run diagnostics first
  HRESULT diagHr = DiagnosePrivatePoolConfiguration();
  if (FAILED(diagHr))
  {
    std::cout << "Configuration diagnostics failed. Attempting setup...\n";
    HRESULT setupHr = SetupPrivatePool();
    if (FAILED(setupHr))
    {
      std::cout << "Failed to setup private pool. Cannot proceed with test.\n";
      return setupHr;
    }
    
    // Re-run diagnostics after setup
    diagHr = DiagnosePrivatePoolConfiguration();
    if (FAILED(diagHr))
    {
      std::cout << "Setup completed but diagnostics still show issues.\n";
      std::cout << "You may need to restart the Windows Biometric Service.\n";
    }
  }
  
  // First enumerate biometric units to get unit IDs for private pool
  std::cout << "Enumerating biometric units for private pool...\n";
  WINBIO_UNIT_SCHEMA *unitSchemaArray = NULL;
  SIZE_T unitSchemaCount = 0;
  HRESULT hr = WinBioEnumBiometricUnits( 
                  WINBIO_TYPE_FINGERPRINT, 
                  &unitSchemaArray, 
                  &unitSchemaCount 
                  );
  
  if (FAILED(hr))
  {
    std::cout << "Failed to enumerate biometric units. hr = 0x" << std::hex << hr << std::dec << "\n";
    return hr;
  }
  
  if (unitSchemaCount == 0)
  {
    std::cout << "No biometric units found.\n";
    WinBioFree(unitSchemaArray);
    return E_FAIL;
  }
  
  std::cout << "Found " << unitSchemaCount << " biometric unit(s).\n";
  
  // Build unit ID array for private pool - use the first available unit
  WINBIO_UNIT_ID unitIdArray[1] = {};
  SIZE_T unitIdCount = 1;
  unitIdArray[0] = unitSchemaArray[0].UnitId;
  
  std::cout << "Using unit ID " << unitIdArray[0] << " for private pool session.\n";
  std::cout << "Sensor: " << unitSchemaArray[0].Description << " (" << unitSchemaArray[0].Manufacturer << ")\n";
  
  // Clean up unit schema array
  WinBioFree(unitSchemaArray);
  unitSchemaArray = NULL;
  
  WINBIO_SESSION_HANDLE sessionHandle = NULL;
  
  std::cout << "Opening private pool session...\n";
  
  // Open session with private pool using proper parameters
  hr = WinBioOpenSession(
    WINBIO_TYPE_FINGERPRINT,
    WINBIO_POOL_PRIVATE,
    WINBIO_FLAG_BASIC,              // Use BASIC flag as shown in Microsoft example
    unitIdArray,                    // Array of unit IDs for private pool
    unitIdCount,                    // Count of unit IDs
    (WINBIO_UUID*)&PRIVATE_POOL_DATABASE_ID,  // Database ID for private pool
    &sessionHandle
    );

  if (FAILED(hr))
  {
    std::cout << "Failed to open private pool session. hr = 0x" << std::hex << hr << std::dec << "\n";
    std::cout << "This may indicate:\n";
    std::cout << "1. Private pool setup is incomplete\n";
    std::cout << "2. Windows Biometric Service needs to be restarted\n";
    std::cout << "3. Insufficient privileges (run as administrator)\n";
    std::cout << "4. Sensor not properly configured for private pool\n";
    return hr;
  }

  std::cout << "Private pool session opened successfully!\n";
  std::cout << "This confirms that WINBIO_POOL_PRIVATE is working correctly.\n";
  
  // Test basic private pool functionality
  WINBIO_UNIT_ID unitId = 0;
  std::cout << "Testing sensor location in private pool...\n";
  std::cout << "Please touch your fingerprint sensor...\n";
  
  hr = WinBioLocateSensor(sessionHandle, &unitId);
  if (SUCCEEDED(hr))
  {
    std::cout << "Sensor located successfully in private pool! Unit ID: " << unitId << "\n";
    
    // Try a simple enrollment operation to verify the private pool works
    std::cout << "Testing enrollment capability in private pool...\n";
    std::cout << "Please place finger on sensor for enrollment test...\n";
    
    hr = WinBioEnrollBegin(
      sessionHandle,
      WINBIO_FINGER_UNSPECIFIED_POS_01,
      unitId
      );
      
    if (SUCCEEDED(hr))
    {
      std::cout << "Enrollment begin successful in private pool!\n";
      
      WINBIO_REJECT_DETAIL rejectDetail = 0;
      hr = WinBioEnrollCapture(sessionHandle, &rejectDetail);
      
      if (SUCCEEDED(hr))
      {
        std::cout << "Enrollment capture successful in private pool!\n";
        std::cout << "Private pool is fully functional for biometric operations.\n";
      }
      else
      {
        if (hr == WINBIO_E_BAD_CAPTURE)
          std::cout << "Bad capture in private pool; reason: " << rejectDetail << "\n";
        else
          std::cout << "Enrollment capture failed in private pool. hr = 0x" << std::hex << hr << std::dec << "\n";
      }
      
      // Discard the enrollment (we don't want to save test data)
      WinBioEnrollDiscard(sessionHandle);
    }
    else
    {
      std::cout << "Enrollment begin failed in private pool. hr = 0x" << std::hex << hr << std::dec << "\n";
    }
  }
  else
  {
    std::cout << "Failed to locate sensor in private pool. hr = 0x" << std::hex << hr << std::dec << "\n";
  }

  if (sessionHandle != NULL)
  {
    WinBioCloseSession(sessionHandle);
    sessionHandle = NULL;
  }

  std::cout << "Private pool test completed.\n";
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
  
  CreateDirectoryA("data", NULL);
  
  // Main menu loop
  while (true)
  {
    std::cout << "\n========================================\n";
    std::cout << "Windows Biometric Framework Test Menu\n";
    std::cout << "========================================\n";
    std::cout << "1. Test System Pool (WINBIO_POOL_SYSTEM)\n";
    std::cout << "2. Test Private Pool (WINBIO_POOL_PRIVATE)\n";
    std::cout << "3. Setup Private Pool\n";
    std::cout << "4. Cleanup Private Pool\n";
    std::cout << "5. Diagnose Private Pool Configuration\n";
    std::cout << "6. Restart Windows Biometric Service\n";
    std::cout << "7. Force Reconfigure Private Pool\n";
    std::cout << "8. Check for ESS Sensor (Enhanced Sign-in Security)\n";
    std::cout << "9. Exit\n";
    std::cout << "Choose an option (1-9): ";
    
    std::string choice;
    std::getline(std::cin, choice);
    
    if (choice == "1")
    {
      std::cout << "\n--- Testing System Pool ---\n";
      CaptureSample();
    }
    else if (choice == "2")
    {
      TestPrivatePool();
    }
    else if (choice == "3")
    {
      SetupPrivatePool();
    }
    else if (choice == "4")
    {
      CleanupPrivatePool();
    }
    else if (choice == "5")
    {
      DiagnosePrivatePoolConfiguration();
    }
    else if (choice == "6")
    {
      RestartBiometricService();
    }
    else if (choice == "7")
    {
      ForceReconfigurePrivatePool();
    }
    else if (choice == "8")
    {
      CheckForESSensor();
    }
    else if (choice == "9")
    {
      break;
    }
    else
    {
      std::cout << "Invalid choice. Please enter 1-9.\n";
    }
    
    if (choice != "9")
    {
      std::cout << "\nPress Enter to return to menu...";
      std::cin.get();
    }
  }
  
  std::cout << "\nThank you for using the Windows Biometric Framework Test Utility!\n";
  return 0;
}