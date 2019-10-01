//David Cantador Piedras 51120722W
//David Davó Laviña 02581158Y
#include <stdio.h>

typedef struct {
    char data[4096];
    int key;
} item;

item array[] = {
    {"bill", 3},
    {"neil", 4},
    {"john", 2},
    {"rick", 5},
    {"alex", 1},
};

void sort(item *a, int n) {
    int i = 0, j = 0;
    int s = 1;
    item* p;

    for(; i < n && s != 0; i++) {
        s = 0;
        p = a+i;
        j = n-1-i;
        do {
            if( p->key > (p+j)->key) {
                item t = *p;
                *p  = *(p+j);
                *(p+j) = t;
                s++;
            }
        } while ( --j >= 0 );
    }
}

int main() {
    int i;
    sort(array,5);
    for(i = 0; i < 5; i++)
        printf("array[%d] = {%s, %d}\n",
                i, array[i].data, array[i].key);
    return 0;
}
