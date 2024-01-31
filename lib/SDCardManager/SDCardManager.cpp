#include "SDCardManager.h"

#ifndef DEBUG_VD
#define DEBUG_VD 0
#endif
#if !defined(PRINTF) 
#define PRINTF(...) { Serial.printf(__VA_ARGS__); }
#endif
#if !defined(PRINTLN) 
#define PRINTLN(...) { Serial.println(__VA_ARGS__); }
#endif
#if !defined(PRINT)
#define PRINT(...) { Serial.print(__VA_ARGS__); }
#endif


SDCardManager::SDCardManager(uint8_t csPin) : _csPin(csPin) {
}

void SDCardManager::begin() {
    if(!SD.begin(_csPin)){
        PRINTLN("Card Mount Failed");
        return;
    }
    uint8_t cardType = SD.cardType();

    if(cardType == CARD_NONE){
        PRINTLN("No SD card attached");
        return;
    }

    PRINT("SD Card Type: ");
    if(cardType == CARD_MMC){
        PRINTLN("MMC");
    } else if(cardType == CARD_SD){
        PRINTLN("SDSC");
    } else if(cardType == CARD_SDHC){
        PRINTLN("SDHC");
    } else {
        PRINTLN("UNKNOWN");
    }

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    PRINTF("SD Card Size: %lluMB\n", cardSize);
}

void SDCardManager::listDir(const char * dirname, uint8_t levels) {
    // Implementation of listDir
    PRINTF("Listing directory: %s\n", dirname);

    File root = SD.open(dirname);
    if(!root){
        PRINTLN("Failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        PRINTLN("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            PRINT("  DIR : ");
            PRINTLN(file.name());
            if(levels){
                listDir(file.name(), levels -1);
            }
        } else {
            PRINT("  FILE: ");
            PRINT(file.name());
            PRINT("  SIZE: ");
            PRINTLN(file.size());
        }
        file = root.openNextFile();
    }
}

void SDCardManager::createDir(const char * path) {
    // Implementation of createDir
    PRINTF("Creating Dir: %s\n", path);
    if(SD.mkdir(path)){
        PRINTLN("Dir created");
    } else {
        PRINTLN("mkdir failed");
    }
}

void SDCardManager::removeDir(const char * path) {
    // Implementation of removeDir
    PRINTF("Removing Dir: %s\n", path);
    if(SD.rmdir(path)){
        PRINTLN("Dir removed");
    } else {
        PRINTLN("rmdir failed");
    }
}

void SDCardManager::readFile(const char * path) {
    // Implementation of readFile
    PRINTF("Reading file: %s\n", path);

    File file = SD.open(path);
    if(!file){
        PRINTLN("Failed to open file for reading");
        return;
    }

    PRINT("Read from file: ");
    while(file.available()){
        Serial.write(file.read());
    }
    file.close();
}

void SDCardManager::writeFile(const char * path, const char * message) {
    // Implementation of writeFile
    PRINTF("Writing file: %s\n", path);

    File file = SD.open(path, FILE_WRITE);
    if(!file){
        PRINTLN("Failed to open file for writing");
        return;
    }
    if(file.print(message)){
        PRINTLN("File written");
    } else {
        PRINTLN("Write failed");
    }
    file.close();
}

void SDCardManager::appendFile(const char * path, const char * message) {
    // Implementation of appendFile
    PRINTF("Appending to file: %s\n", path);

    File file = SD.open(path, FILE_APPEND);
    if(!file){
        PRINTLN("Failed to open file for appending");
        return;
    }
    if(file.print(message)){
        PRINTLN("Message appended");
    } else {
        PRINTLN("Append failed");
    }
    file.close();
}

void appendFile(String path, String message){
    // Implementation of appendFile
    PRINTF("Appending to file: %s\n", path.c_str());

    File file = SD.open(path.c_str(), FILE_APPEND);
    if(!file){
        PRINTLN("Failed to open file for appending");
        return;
    }
    if(file.print(message)){
        PRINTLN("Message appended");
    } else {
        PRINTLN("Append failed");
    }
    file.close();
}

void SDCardManager::renameFile(const char * path1, const char * path2) {
    // Implementation of renameFile
    PRINTF("Renaming file %s to %s\n", path1, path2);
    if (SD.rename(path1, path2)) {
        PRINTLN("File renamed");
    } else {
        PRINTLN("Rename failed");
    }
}

void SDCardManager::deleteFile(const char * path) {
    // Implementation of deleteFile
    PRINTF("Deleting file: %s\n", path);
    if(SD.remove(path)){
        PRINTLN("File deleted");
    } else {
        PRINTLN("Delete failed");
    }
}

void SDCardManager::testFileIO(const char * path) {
    // Implementation of testFileIO
    File file = SD.open(path);
    static uint8_t buf[512];
    size_t len = 0;
    uint32_t start = millis();
    uint32_t end = start;
    if(file){
        len = file.size();
        size_t flen = len;
        start = millis();
        while(len){
            size_t toRead = len;
            if(toRead > 512){
                toRead = 512;
            }
            file.read(buf, toRead);
            len -= toRead;
        }
        end = millis() - start;
        PRINTF("%u bytes read for %u ms\n", flen, end);
        file.close();
    } else {
        PRINTLN("Failed to open file for reading");
    }

    file = SD.open(path, FILE_WRITE);
    if(!file){
        PRINTLN("Failed to open file for writing");
        return;
    }

    size_t i;
    start = millis();
    for(i=0; i<2048; i++){
        file.write(buf, 512);
    }
    end = millis() - start;
    PRINTF("%u bytes written for %u ms\n", 2048 * 512, end);
    file.close();
}

bool SDCardManager::isCardPresent() {
    return SD.begin(_csPin);
}

String SDCardManager::getNextFileName(const char* filePrefix, const char* fileSuffix) {
    int maxFileNumber = 0;
    File root = SD.open("/");
    File file = root.openNextFile();
    while(file) {
        String fileName = file.name();
        if (fileName.startsWith(filePrefix) && fileName.endsWith(fileSuffix)) {
            int fileNumber = fileName.substring(strlen(filePrefix), fileName.length() - strlen(fileSuffix)).toInt();
            if (fileNumber > maxFileNumber) {
                maxFileNumber = fileNumber;
            }
        }
        file = root.openNextFile();
    }

    // Generate next file name
    String nextFileName = String(filePrefix) + String(maxFileNumber + 1, DEC) + String(fileSuffix);
    return nextFileName;
}