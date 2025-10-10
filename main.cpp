/*
 * Author: Tomáš Růžička, t_ruzicka (at) email.cz
 * 2015
 */

#include "headers.h"
#include "BioHelper.h"

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

// Setup private pool by installing database and configuring sensor
HRESULT SetupPrivatePool()
{
  std::cout << "\n=== Setting up Private Pool ===\n";
  
  // Check if already installed
  if (IsPrivatePoolDatabaseInstalled())
  {
    std::cout << "Private database already installed. Skipping setup.\n";
    return S_OK;
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
    
    if (SUCCEEDED(hr))
    {
      // Configure the sensor for the private pool
      std::cout << "Configuring sensor for private pool...\n";
      derivedConfig.DatabaseId = PRIVATE_POOL_DATABASE_ID;
      hr = BioHelper::RegisterPrivateConfiguration(selectedUnit, &derivedConfig);
      
      if (SUCCEEDED(hr))
      {
        std::cout << "Private pool setup completed successfully!\n";
        std::cout << "Sensor: " << selectedUnit->Description << " (" << selectedUnit->Manufacturer << ")\n";
      }
      else
      {
        std::cout << "Failed to register private configuration. hr = 0x" << std::hex << hr << std::dec << "\n";
        
        // Clean up database if sensor config failed
        BioHelper::UnregisterDatabase((WINBIO_UUID*)&PRIVATE_POOL_DATABASE_ID);
      }
    }
    else
    {
      std::cout << "Failed to register private database. hr = 0x" << std::hex << hr << std::dec << "\n";
    }
  }
  else
  {
    std::cout << "Failed to create compatible configuration. hr = 0x" << std::hex << hr << std::dec << "\n";
  }
  
  WinBioFree(unitArray);
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

// Test private pool functionality
HRESULT TestPrivatePool()
{
  std::cout << "\n=== Testing Private Pool ===\n";
  
  // Check if private pool is set up
  if (!IsPrivatePoolDatabaseInstalled())
  {
    std::cout << "Private pool database not found. Setting up...\n";
    HRESULT setupHr = SetupPrivatePool();
    if (FAILED(setupHr))
    {
      std::cout << "Failed to setup private pool. Cannot proceed with test.\n";
      return setupHr;
    }
  }
  
  HRESULT hr = S_OK;
  WINBIO_SESSION_HANDLE sessionHandle = NULL;
  
  std::cout << "Opening private pool session...\n";
  
  // Open session with private pool
  hr = WinBioOpenSession(
    WINBIO_TYPE_FINGERPRINT,
    WINBIO_POOL_PRIVATE,
    WINBIO_FLAG_DEFAULT,
    NULL,
    0,
    (WINBIO_UUID*)&PRIVATE_POOL_DATABASE_ID,
    &sessionHandle
    );

  if (FAILED(hr))
  {
    std::cout << "Failed to open private pool session. hr = 0x" << std::hex << hr << std::dec << "\n";
    std::cout << "This may indicate:\n";
    std::cout << "1. Private pool setup is incomplete\n";
    std::cout << "2. Windows Biometric Service needs to be restarted\n";
    std::cout << "3. Insufficient privileges (run as administrator)\n";
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
    std::cout << "5. Exit\n";
    std::cout << "Choose an option (1-5): ";
    
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
      break;
    }
    else
    {
      std::cout << "Invalid choice. Please enter 1-5.\n";
    }
    
    if (choice != "5")
    {
      std::cout << "\nPress Enter to return to menu...";
      std::cin.get();
    }
  }
  
  std::cout << "\nThank you for using the Windows Biometric Framework Test Utility!\n";
  return 0;
}