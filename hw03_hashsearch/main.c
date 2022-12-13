#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <math.h>

#define COMBO(x, y) ((x << 8) | y)
#define CYR(x) (((x >= 0xD090 && x <= 0xD0BF) || (x >= 0xD180 && x <= 0xD18F) || x == 0xD081 || x == 0xD191))

typedef struct HT_ITEM ht_item;
typedef struct HASH_TABLE h_table;

//-------- config --------
const unsigned long init_table_size = 128;
const int table_fill_limit_percent = 75;
//------------------------

const unsigned int hash_size = 199967;
const int alphabet_size = 53639;

struct HT_ITEM {
    unsigned int value;
    char* key;
};

struct HASH_TABLE {
    unsigned long size;
    unsigned long count;
    int collisions;
    int realloc_count;
    ht_item **items;
};

unsigned long get_prime_mod(unsigned long n)
{
    if (n & 1)
        n -= 2;
    else
        n--;
  
    unsigned long i, j;
    for (i = n; i >= 2; i -= 2) {
        if (i % 2 == 0)
            continue;
        for (j = 3; j <= sqrt(i); j += 2) {
            if (i % j == 0)
                break;
        }
        if (j > sqrt(i))
            return i;
    }
    return 2;
}

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
    return hash;
}

unsigned long get_index(unsigned long *hash, unsigned long *step, unsigned long *mod) {
    unsigned long i1 = *hash % *mod;
    unsigned long i2 = 1 + *hash % (*mod - 1);
    return ((i1 + (*step * i2)) % *mod);
}

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
    for (unsigned long x=0; x<size; x++) {
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
    table->realloc_count = 0;

    return table;
}

void expand_table(h_table *table, const unsigned long *block_size) {

    ht_item **items = realloc(table->items, (table->size + *block_size) * sizeof(ht_item *));
    if (!items) {
        puts("error allocating dynamic message for item of hash table");
        exit(1);
    }
    for (unsigned long x=table->size; x<(table->size + *block_size); x++) {
        items[x] = malloc(sizeof(struct HT_ITEM));
        if (!items[x]) {
            puts("error allocating dynamic message for item of hash table");
            exit(1);
        }
        items[x] = create_item("");
    }
    table->items = items;
    table->size += *block_size;
}

void free_h_table(h_table* table) {
    for (size_t i = 0; i < table->size; i++) {
        free(table->items[i]);
    }
    free(table->items);
    free(table);
}

void rellocate_table(h_table *table, unsigned long *mod, unsigned long *old_size) {
    unsigned long step = 0;
    unsigned long index;
    unsigned long hash;
    int inserted = 0;
    int cur_value;
    for (unsigned long x=0; x<*old_size; x++) {
        if (table->items[x]->value != 0) {
            char* cur_key = (char* ) malloc(strlen(table->items[x]->key) + 1);
            if (!cur_key) {
                puts("realloc error malloc for temp key");
                free_h_table(table);
                exit(1);
            }
            strcpy(cur_key, table->items[x]->key);
            cur_value = table->items[x]->value;
            strcpy(table->items[x]->key, "");
            table->items[x]->value = 0;
            hash = get_hash(cur_key, strlen(cur_key));
            while (!inserted || step > (*mod - 1) ) {
                index = get_index(&hash, &step, mod);
                if (table->items[index]->value == 0) {
                    strcpy(table->items[index]->key, cur_key);
                    table->items[index]->value = cur_value;
                    inserted = 1;
                }
                else {
                    step++;
                }
            }
            inserted = 0;
            free(cur_key);
        }
        step = 0;
        
    }
    table->realloc_count++;
}

void insert_h_item(h_table *table, char *key, unsigned long *mod) {
    unsigned long hash;
    unsigned long step = 0;
    unsigned long index;
    int inserted = 0;
    hash = get_hash(key, strlen(key));
    while (!inserted || step > (*mod - 1) ) {
        index = get_index(&hash, &step, mod);
        if (table->items[index]->value == 0) {
            strcpy(table->items[index]->key, key);
            table->items[index]->value++;
            table->count++;
            inserted = 1;
        }
        else {
            if (strcmp(table->items[index]->key, key) != 0) {
                table->collisions++;
                step++;
            }
            else {
                table->items[index]->value++;
                inserted = 1;
            }
        }
    }
    if (step > (*mod - 1)) {
        puts("hash table overflowed, exiting");
        free_h_table(table);
        exit(1);
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
    printf("resolved collisions count %d\n", table->collisions);
    printf("hash table was expanded %d times\n", table->realloc_count);
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


int main(int argc, char* argv[]) {
    checkArgs(&argc, argv);
    (void)argc;

    FILE *fpin;
    char* CUR_FILE_NAME = argv[1];

    if ((fpin=fopen(CUR_FILE_NAME, "r") ) == NULL) {
        printf("Error opening file %s\n", CUR_FILE_NAME);
        exit (1);
    }
    size_t fsize = getFileSize(fpin);
    
    char buff[fsize];
    char sep[20]="()/., \n:;-[]-";
    char *istr;

    unsigned long mod = get_prime_mod(init_table_size);
    unsigned long oldsize;
    h_table* hashtable = create_table(init_table_size);

    while (fgets(buff, sizeof(buff), fpin) != 0) {
        istr = strtok (buff,sep);
        while (istr != NULL)
        {
            if (strcmp(istr, "\x0d") != 0) {
                if (hashtable->count >= (hashtable->size * table_fill_limit_percent / 100.0)) {
                    oldsize = hashtable->size;
                    expand_table(hashtable, &init_table_size);
                    mod = get_prime_mod(hashtable->size);
                    rellocate_table(hashtable, &mod, &oldsize);
                }
                insert_h_item(hashtable, istr, &mod);
            }
            istr = strtok(NULL,sep);
        }
    }
    fclose(fpin);
    print_h_table(hashtable);
    free_h_table(hashtable);

    return 0;
}
