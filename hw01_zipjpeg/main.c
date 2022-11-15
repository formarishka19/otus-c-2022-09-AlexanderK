#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 2000000

typedef char* string;
struct LFH
{
    // Обязательная сигнатура, равна 0x04034b50
    uint32_t signature;
    // Минимальная версия для распаковки
    uint16_t versionToExtract;
    // Битовый флаг
    uint16_t generalPurposeBitFlag;
    // Метод сжатия (0 - без сжатия, 8 - deflate)
    uint16_t compressionMethod;
    // Время модификации файла
    uint16_t modificationTime;
    // Дата модификации файла
    uint16_t modificationDate;
    // Контрольная сумма
    uint32_t crc32;
    // Сжатый размер
    uint32_t compressedSize;
    // Несжатый размер
    uint32_t uncompressedSize;
    // Длина название файла
    uint16_t filenameLength;
    // Длина поля с дополнительными данными
    uint16_t extraFieldLength;
    // Название файла (размером filenameLength)
    // uint8_t *filename;
    // Дополнительные данные (размером extraFieldLength)
    // uint8_t *extraField;
// };
} __attribute__((packed)) localFileHeader;

const int offsetToFNLen = 26;

int getFileSize(FILE *fp) {
    fseek(fp, 0L, SEEK_END);
    int size = ftell(fp);
    fseek(fp, 0L, SEEK_SET);
    return size;
}

size_t searchSignature( \
    unsigned char *source, \
    unsigned char *signature, \
    size_t sourceLength, \
    size_t signatureLength, \
    int searchDirection, \
    size_t startOffset \
) {
    if (searchDirection) {
        if (startOffset < sourceLength) {
            size_t i = startOffset, k;
            while (i < (sourceLength - startOffset - signatureLength + 1)) {
                k = 0;
                while ((source[i + k] == signature[k]) && (k < signatureLength)) {
                    k++;
                }
                if (k == signatureLength) {
                    return i;
                }
                i++;
            }
        };
    }
    else {
        size_t k, i = sourceLength - signatureLength;
        while (i >= 0) {
            k = 0;
            while ((source[i + k] == signature[k]) && (k < signatureLength)) {
                k++;
            }
            if (k == signatureLength) {
                return i;
            };
            i--;
        }
    }
    return sourceLength;
}


void getFileBytes(FILE *fp, unsigned char *filebytes, int len) {
    for (int i = 0; i < len; i++) {
        filebytes[i] = fgetc(fp);
    }
}

int main(int argc, char *argv[]) {
    FILE *fp;
    size_t fileLength;
    size_t bufferOffset = 0;
    unsigned char BUFFER[BUFFER_SIZE] = {"\x0"};
    unsigned char ZIP_SIGNATURE[] = "\x50\x4b\x03\x04";

    char* CUR_FILE_NAME = argv[1];
    (void)argc;
    
    fp = fopen(CUR_FILE_NAME, "rb");
    fileLength = getFileSize(fp);
    if (fileLength > BUFFER_SIZE) {
        fclose(fp);
        printf("File is too big, max size is %d bytes", BUFFER_SIZE);
        return -1;
    }
    else{
        getFileBytes(fp, BUFFER, fileLength);
    }
    fclose(fp);

    size_t zipSignatureFirstPosition = searchSignature(BUFFER, ZIP_SIGNATURE, sizeof(BUFFER), sizeof(ZIP_SIGNATURE) - 1, 1, bufferOffset);
    if (zipSignatureFirstPosition < sizeof(BUFFER)) {
        printf("File %s contains zip, including:\n", CUR_FILE_NAME);
        size_t signaturePosition;
        while (1) {
            signaturePosition = searchSignature(BUFFER, ZIP_SIGNATURE, sizeof(BUFFER), sizeof(ZIP_SIGNATURE) - 1, 1, bufferOffset);
            if (signaturePosition == sizeof(BUFFER)) {
                break;
            }
            memcpy(&localFileHeader, BUFFER + signaturePosition, sizeof(localFileHeader));
            unsigned char hex_text[] = {BUFFER[signaturePosition + offsetToFNLen], BUFFER[signaturePosition + offsetToFNLen + 1]};
            unsigned int fileNameLen = (unsigned int) (*hex_text);
            bufferOffset += signaturePosition + offsetToFNLen + 4 + fileNameLen;
            if (BUFFER[sizeof(localFileHeader) + localFileHeader.filenameLength - 1] == '/' && localFileHeader.compressedSize == 0 && localFileHeader.crc32 == 0)
            {
                continue;
            }
            else {
                printf("%.*s\n", fileNameLen, BUFFER + signaturePosition + offsetToFNLen + 4);
            }
        }
    } else {
        printf("File %s doen't contain zip\n", CUR_FILE_NAME);
    }
	return 0;
}
