#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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
// };
} __attribute__((packed)) localFileHeader;

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
        printf("File %s contains zip, including:\n", CUR_FILE_NAME);
        size_t signaturePosition;
        while (1) {
            signaturePosition = searchSignature(BUFFER, ZIP_SIGNATURE, &fileLength, sizeof(ZIP_SIGNATURE) - 1, &bufferOffset);
            if (signaturePosition == fileLength) {
                break;
            }
            memcpy(&localFileHeader, BUFFER + signaturePosition, sizeof(localFileHeader));
            unsigned char hex_text[] = {BUFFER[signaturePosition + offsetToFNLen], BUFFER[signaturePosition + offsetToFNLen + 1]};
            unsigned int fileNameLen = (unsigned int) (*hex_text);
            bufferOffset = signaturePosition + offsetToFNLen + 4 + fileNameLen;
            if (BUFFER[sizeof(localFileHeader) + localFileHeader.filenameLength - 1] == '/' && localFileHeader.compressedSize == 0 && localFileHeader.crc32 == 0)
            {
                continue;
            }
            else {
                printf("%.*s\n", fileNameLen, BUFFER + signaturePosition + offsetToFNLen + 4);
            }
            
        }
    } else {
        printf("File %s doesn't contain zip\n", CUR_FILE_NAME);
    }
    free(BUFFER);
	return 0;
}
