#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define COMBO(x, y) ((x << 8) | y)
#define CYR(x) (((x >= 0xD090 && x <= 0xD0BF) || (x >= 0xD180 && x <= 0xD18F) || x == 0xD081 || x == 0xD191))

typedef struct HT_ITEM ht_item;
typedef struct HASH_TABLE h_table;

const int alphabet_size = 53639;
const unsigned int table_size = 200000; 
const unsigned int hash_size = 199967;

struct HT_ITEM {
    unsigned int value;
    char* key;
};

struct HASH_TABLE {
    unsigned long size;
    unsigned long count;
    int collisions;
    ht_item **items;
};

unsigned long raise_power(unsigned int x, int *power)
{
    unsigned long result = 1;
    for (int i = 1; i <= *power; i++ ) {
        result *= x;
    }
    return(result);
}

unsigned long get_hash(char* word, int word_size) {
    unsigned long hash = 0;
    unsigned int cur_ch;
    unsigned int combo_ch;
    for (int i = 0; i < word_size; i++) {
        cur_ch = word[i];
        if (i < (word_size - 1)) {
            combo_ch = COMBO((word[i] & 0x000000ff), (word[i+1] & 0x000000ff));
            if (CYR(combo_ch)) {
                cur_ch = combo_ch;
                i++;
            }
        }
        hash += (cur_ch * raise_power(alphabet_size, &i));
        hash = hash % hash_size;
    }
    if (hash < table_size) {
        return hash;
    }
    else {
        printf("runtime error, small table size %u for hash %lu", table_size, hash);
        exit(1);
    }
    
}

// init and free HASH_TABLE

ht_item *create_item(char* key) {
    ht_item* item = (ht_item* ) malloc(sizeof(ht_item));
    if (!item) {
        puts("error allocating dynamic message for item of hash table");
        exit(1);
    }
    item->key = (char* ) malloc(strlen(key) + 1);
    if (!item->key) {
        puts("error allocating dynamic message for item key");
        exit(1);
    }
    strcpy(item->key, key);
    item->value = 0;
    return item;
}

h_table *create_table(unsigned long size) {
    h_table *table = (h_table* ) malloc(sizeof(h_table));
    if (!table) {
        puts("error allocating dynamic message for hash table");
        exit(1);
    }
    // allocate array of pointers to items structures
    ht_item **items = malloc(size * sizeof(ht_item *));
    // allocate structs and have the array point to them
    for (unsigned long x=0; x<table_size; x++) {
        items[x] = malloc(sizeof(struct HT_ITEM));
        if (!items[x]) {
            puts("error allocating dynamic message for item of hash table");
            exit(1);
        }
        items[x] = create_item("");
    }
    table->items = items;

    table->size = size;
    table->count = 0;
    table->collisions = 0;

    return table;
}

void free_h_item(ht_item* item) {
    free(item->key);
    free(item);
}

void free_h_table(h_table* table) {
    for (size_t i = 0; i < table->size; i++) {
        free_h_item(table->items[i]);
    }
    free(table->items);
    free(table);
}

// insert and print items

void insert_h_item(h_table* table, char* key) {
    unsigned long index = get_hash(key, strlen(key));
    // printf("%lu\n",index);
    if (table->items[index]->value == 0) {
        if (table->count >= table->size) {
            puts("runtime error. hash table is full. Exiting...");
            free_h_table(table);
            exit(1);
        }
        strcpy(table->items[index]->key, key);
        table->items[index]->value++;
        table->count++;
    }
    else {
        if (strcmp(table->items[index]->key, key) != 0) {
            puts("COLLISION IN HASH TABLE!");
            printf("word exists %s, new word %s hash %lu\n", table->items[index]->key, key, index);
            table->collisions++;
            // free_h_table(table);
            // exit(1);
        }  
        else {
            table->items[index]->value++;
        }
        
    }
}

void print_h_table(h_table* table) {
    int words_count = 0;
    // exit(1);
    for (size_t i = 0; i < table->size; i++) {
        if (table->items[i]->value != 0) {
            words_count++;
            
        }
    }
    puts("------ STATISTICS -------");
    printf("words counted %d\n", words_count);
    printf("coillision count %d\n", table->collisions);
    puts("------------------------");
    if (table->collisions > 0) {
        puts("BAD HASH FUNCTION!!!");
    }
    puts("------ WORDS COUNTS -------");

    // sorting results...

    ht_item* words[words_count];
    ht_item* temp;
    int clean_counter = 0;

    for (size_t k = 0; k < table->size; k++){
        if (table->items[k]->value != 0) {
            words[clean_counter] = table->items[k];
            clean_counter++;
        }
    }
    for (int i = 0; i < clean_counter; i++) {
		for(int j = i + 1; j < clean_counter; j++) {
			if (words[i]->value < words[j]->value) {
                temp = words[j];
                words[j] = words[i];
                words[i] = temp;
            }
		}
	}

    //printing..
    for (int i = 0; i < clean_counter; i++) {
        printf("%d %s\n", words[i]->value, words[i]->key);
    }
}


void checkArgs(int* argc, char* argv[]) {
    if (*argc != 2) {
        printf("\n\n --- ! ОШИБКА ЗАПУСКА ----\n\n"
        "Программа принимает на вход только один аргумент - путь до исходного файла в кодировке UTF8 (рус. или анг. язык)\n\n"
        "------ Синтаксис ------\n"
        "%s <source filepath>\n"
        "-----------------------\n\n"
        "Пример запуска\n"
        "%s ./files/test.txt \n\n"
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

size_t clean_text(FILE *fp, size_t* fsize, unsigned char* buffer_out) {
    size_t k = 0;
    unsigned char ch, next_ch;
    for (size_t i = 0; i < *fsize; i++) {
        ch = fgetc(fp);
        if (ch < 0x7F) {
            if ((ch >= 0x41 && ch <= 0x5A) || (ch >= 0x61 && ch <= 0x7A) || ch == 0x20) {
                buffer_out[k] = ch;
            }
            else {
                buffer_out[k] = 0x20;
            }
        }
        else {
            if (ch >= 0xD0 && ch <= 0xD1) {
                next_ch = fgetc(fp);
                if  (CYR(COMBO(ch, next_ch))){
                    buffer_out[k] = ch;
                    buffer_out[++k] = next_ch;
                }
            }
            else {
                buffer_out[k] = 0x20;
            }
        }
        k++;
    }
    return k;
}



int main(int argc, char* argv[]) {
    checkArgs(&argc, argv);
    (void)argc;

    FILE *fpin, *fpout, *fptemp;
    char* CUR_FILE_NAME = argv[1];
    // char* CUR_FILE_NAME = "./files/test.txt";
    char* TEMP_FILE_NAME = "./1.txt";

    if ((fpin=fopen(CUR_FILE_NAME, "rb") ) == NULL) {
        printf("Error opening file %s\n", CUR_FILE_NAME);
        exit (1);
    }

    size_t fsize = getFileSize(fpin);
    unsigned char* buffer_cleaned = (unsigned char* ) malloc(fsize * sizeof(char));
    if (!buffer_cleaned) {
        printf("Error dynamic memory allocation\n");
        exit (1);
    }
    size_t out_size = clean_text(fpin, &fsize, buffer_cleaned);
    fclose(fpin);

    if ((fpout=fopen(TEMP_FILE_NAME, "wb") ) == NULL) {
        printf("Error creating temporary file %s\n", TEMP_FILE_NAME);
        exit (1);
    }

    size_t x = 0;
    while (x < out_size) {
        fputc(buffer_cleaned[x], fpout);
        x++;
    }
    fclose(fpout);
    free(buffer_cleaned);
    
    if ((fptemp=fopen(TEMP_FILE_NAME, "r") ) == NULL) {
        printf("Error opening temporary file %s\n", TEMP_FILE_NAME);
        exit (1);
    }
    fsize = getFileSize(fptemp);

    h_table* hashtable = create_table(table_size);
    
    char word[out_size];
    int result;
    while(1) {
        result = fscanf(fptemp, "%s", word);
        if (result < 1) break;
        insert_h_item(hashtable, word);
    }
    fclose(fptemp);
    remove(TEMP_FILE_NAME);
    
    puts("printing...");
    
    print_h_table(hashtable);
    free_h_table(hashtable);

    return 0;
}
