#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

long int data[] = {4, 8, 15, 16, 23, 42};
long int data_length = (sizeof(data) / sizeof(long int));
const char* int_format = "%ld ";


struct NODE {
    int data;
    struct NODE *next;
};

struct NODE *add_element(int value, struct NODE *next_element) {
    struct NODE *result = (struct NODE *)calloc(1, sizeof(struct NODE));

    if (!result) {
        exit(1);
    }
    result->data = value;
    result->next = next_element;
    return result;
}

void m(struct NODE *head) {
    struct NODE *node = head;
    while (node)
    {
        printf(int_format, node->data);
        node = node->next;
    }
}

int p(int value) {
    return value & 1;
}

struct NODE* f(struct NODE *head, int (*f)(int)) {
    struct NODE *node = head;
    struct NODE *new_node = NULL;

    while (node)
    {
        if (f(node->data)) {
            new_node = add_element(node->data, new_node);
        }
        node = node->next;
    }
    return new_node;
}


int main() {
    struct NODE *element = NULL;
    while (data[--data_length]) {
        element = add_element(data[data_length], element);
    }
    m(element);
    puts("");
    m(f(element, p));
    puts("");
    free(element);
    return 0;
}
