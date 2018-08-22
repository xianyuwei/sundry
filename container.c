#include<stdio.h>
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#define  container_of(ptr, type, member) ({                      \
                                  const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
                                   (type *)( (char *)__mptr - offsetof(type,member) );})

struct container {
    int a;
    char *str;
    int b;
};


void get_struct_by_p(char **p){//必须是结构体成员的地址
    struct container  *test = container_of(p, struct container, str);
    printf("------------:%d-----------:%d-----------:%s\n", test->a, test->b, test->str);
}

void get_struct_by_int(int *a){
    struct container  *test = container_of(a, struct container, a);
    printf("------------:%d-----------:%d-----------:%s\n", test->a, test->b, test->str);
}

int main(){
    struct container  test = {
        .a = 1,
        .b = 21,
        .str = "abc"
    };
    get_struct_by_p(&test.str);
    get_struct_by_int(&test.a);
    return 0;
}
