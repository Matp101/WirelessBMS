#ifndef SDCardManager_h
#define SDCardManager_h

#include "Arduino.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"

//==============================================================================
// Editable settings

// DEBUG_VD: 0 = off, 1 = basic, 2 = verbose
#ifndef DEBUG_VD
#define DEBUG_VD 2
#endif

//==============================================================================

class SDCardManager {
public:
    SDCardManager(uint8_t csPin);
    void begin();
    void listDir(const char * dirname, uint8_t levels);
    void createDir(const char * path);
    void removeDir(const char * path);
    void readFile(const char * path);
    void writeFile(const char * path, const char * message);
    void appendFile(const char * path, const char * message);
    void appendFile(String path, String message);
    void renameFile(const char * path1, const char * path2);
    void deleteFile(const char * path);
    void testFileIO(const char * path);
    bool isCardPresent();
    String getNextFileName(const char* filePrefix, const char* fileSuffix);

private:
    uint8_t _csPin;
};

#endif
