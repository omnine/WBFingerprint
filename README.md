# Windows-Biometric-Framework-FingerPrint-Example
Simple WBF example which captures fingerprint sample and saves it into data folder.

## Updates for Modern Sensors
This utility has been updated to work with modern fingerprint sensors by using `WinBioEnrollCapture` instead of the deprecated `WinBioCaptureSample`. The application now:

1. **Primary Method**: Uses `WinBioEnrollCapture` with enrollment session for modern sensor compatibility
2. **Fallback Method**: Falls back to the original `WinBioCaptureSample` for older sensors
3. **Image Data**: Attempts to extract raw image data when supported by the sensor
4. **Graceful Handling**: Provides informative messages when raw image data is not available

## Usage
Build project with admin privileges and run the executable. The application will:
- Attempt fingerprint capture using the enrollment method first
- Fall back to raw capture if the enrollment method fails
- Save fingerprint images to the `data/` folder when raw data is available
- Display success messages even when image extraction is not supported

## Compatibility
- **Modern Sensors**: Supported via WinBioEnrollCapture
- **Legacy Sensors**: Supported via WinBioCaptureSample fallback
- **Windows 10/11**: Fully tested and compatible
