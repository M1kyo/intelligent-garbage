#include <stdio.h>
#include <stdlib.h>
#include "garbage.h"
int main(){
    char *category = NULL;
    
    garbage_init();
    category  = garbage_category(category);
    printf("category=%s\n", category);
    
    garbage_final();
    free(category);
    return 0;
}