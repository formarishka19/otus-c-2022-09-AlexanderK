#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

typedef char* string;
struct LFH
{
    uint32_t signature;
    uint16_t versionToExtract;
    uint16_t generalPurposeBitFlag;
    uint16_t compressionMethod;
    uint16_t modificationTime;
    uint16_t modificationDate;
    uint32_t crc32;
    uint32_t compressedSize;
    uint32_t uncompressedSize;
    uint16_t filenameLength;
    uint16_t extraFieldLength;
    // uint8_t *filename;
// };
} __attribute__((packed)) localFileHeader;

struct CDFH
{
    uint32_t signature;
    uint16_t versionMadeBy;
    uint16_t versionToExtract;
    uint16_t generalPurposeBitFlag;
    uint16_t compressionMethod;
    uint16_t modificationTime;
    uint16_t modificationDate;
    uint32_t crc32;
    uint32_t compressedSize;
    uint32_t uncompressedSize;
    uint16_t filenameLength;
    uint16_t extraFieldLength;
    uint16_t fileCommentLength;
    uint16_t diskNumber;
    uint16_t internalFileAttributes;
    uint32_t externalFileAttributes;
    uint32_t localFileHeaderOffset;
    // filename
    // extraField
    // fileComment
} __attribute__((packed)) cdFileHeader;

struct EOCD 
{
    uint16_t diskNumber;
    uint16_t startDiskNumber;
    uint16_t numberCentralDirectoryRecord;
    uint16_t totalCentralDirectoryRecord;
    uint32_t sizeOfCentralDirectory;
    uint32_t centralDirectoryOffset;
    uint16_t commentLength;
 
} __attribute__((packed)) eocd1;

const int offsetToFNLen = 26;

void checkArgs(int *argc, string argv[]) {
    if (*argc != 2) {
        printf("\n\n --- ! ОШИБКА ЗАПУСКА ----\n\n"
        "Программа принимает на вход только один аргумент - путь до проверяемого файла\n\n"
        "------ Синтаксис ------\n"
        "%s <filepath>\n"
        "-----------------------\n\n"
        "Пример запуска\n"
        "%s ./files/Archive.zip\n\n"
        "--- Повторите запуск правильно ---\n\n", argv[0], argv[0]);
        exit (1);
    }
}

size_t getFileSize(FILE *fp) {
    struct stat finfo;
    if (!fstat(fileno(fp), &finfo)) {
        return finfo.st_size;
    }
    else {
        printf("Cannot get file info.\n");
        exit (1);
    }
}

size_t searchSignature( \
    unsigned char *source, \
    unsigned char *signature, \
    size_t *sourceLength, \
    size_t signatureLength, \
    size_t *startOffset \
) {
    if (*startOffset < *sourceLength) {
        size_t i = *startOffset;
        size_t k;
        while (i < (*sourceLength - *startOffset - signatureLength + 1)) {
            k = 0;
            while ((source[i + k] == signature[k]) && (k < signatureLength)) {
                if (k == signatureLength - 1) {
                    return i;
                } 
                k++;
            }
            i++;
        }
    }
    return *sourceLength;
}

int main(int argc, string argv[]) {

    checkArgs(&argc, argv);
    string CUR_FILE_NAME = argv[1];
    (void)argc;

    FILE *fp;
    size_t bufferOffset = 0;
    unsigned char ZIP_SIGNATURE[] = "\x50\x4b\x03\x04";
    unsigned char EOCD_SIGNATURE[] = "\x50\x4b\x05\x06";

    if ((fp = fopen(CUR_FILE_NAME, "rb")) == NULL) {
        printf("Cannot open file.\n");
        exit (1);
    }
    size_t fileLength = getFileSize(fp);
    unsigned char* BUFFER = malloc(sizeof(unsigned char [fileLength]));
    fread(BUFFER, sizeof(unsigned char), fileLength, fp);
    fclose(fp);

    size_t zipSignatureFirstPosition = searchSignature(BUFFER, ZIP_SIGNATURE, &fileLength, sizeof(ZIP_SIGNATURE) - 1, &bufferOffset);
    if (zipSignatureFirstPosition < fileLength) {
        printf("File %s contains zip, including ", CUR_FILE_NAME);
        size_t ecodSignaturePosition = searchSignature(BUFFER, EOCD_SIGNATURE, &fileLength, sizeof(EOCD_SIGNATURE) - 1, &bufferOffset);
        memcpy(&eocd1, BUFFER + ecodSignaturePosition, sizeof(eocd1));
        printf("%hu files or folders:\n", (short int) eocd1.sizeOfCentralDirectory);

        int fnl = 0;
        size_t next_cdfh_position = ecodSignaturePosition - eocd1.centralDirectoryOffset;
        for (int x = 0; x < (short int) eocd1.sizeOfCentralDirectory; x++) {
            memcpy(&cdFileHeader, BUFFER + next_cdfh_position, sizeof(cdFileHeader));
            fnl = cdFileHeader.filenameLength;
            printf("%d) %.*s\n", x + 1, fnl, BUFFER + next_cdfh_position + sizeof(cdFileHeader));
            next_cdfh_position = next_cdfh_position + sizeof(cdFileHeader) + cdFileHeader.filenameLength + cdFileHeader.extraFieldLength + cdFileHeader.fileCommentLength;
        }
    } else {
        printf("File %s doesn't contain zip\n", CUR_FILE_NAME);
    }

    free(BUFFER);
	return 0;
}
