#include <stdio.h>
#include <assert.h>
#include "ds/linklist.h"

#define N 10000

int cnt[N];
void *temp_ptr[N];

typedef struct node_int{
    link_list_base_node_t listnode;
    int val;
}node_int_t;

node_int_t* node_int_new(int val){
    node_int_t* n = (node_int_t*)malloc(sizeof(node_int_t));
    n->val = val;
    return n;
}

int calc_count(link_list_t *list){
    int count = 0;
    for (link_list_base_node_t *i = list->begin; i; i = i -> next){
        node_int_t * node = (node_int_t *)i;
        assert(node->val >= 0 && node->val <= N - 1);
        count ++;
    }
    return count;
}

void test1(){
    link_list_t *list = (link_list_t *)malloc(sizeof(link_list_t ));
    link_list_init(list);

    for (int i = 0; i < N / 2; i ++){
        temp_ptr[i] = (void *)node_int_new(i);
        if (i % 4 == 0){
            assert(list_insert_back(list, temp_ptr[i]));
        }else if (i % 4 == 1){
            assert(list_insert_front(list, temp_ptr[i]));
        }else if (i % 4 == 2){
            assert(
                list_insert_after(list, temp_ptr[i], list->begin)
            );
        }else{
            assert(
                list_insert_before(list, temp_ptr[i], list->end)
            );
        }
    }

    for (int i = N / 2; i < N; i ++){
        temp_ptr[i] = (void *)node_int_new(i);
        if (i & 1){
            assert(list_insert_before(list, temp_ptr[i], temp_ptr[rand() % (i - 1)]));
        }else{
            assert(list_insert_after(list, temp_ptr[i], temp_ptr[rand() % (i - 1)]));
        }      
    }

    assert(calc_count(list) == N);

    void *temp = NULL;
    for (int i = 0; i < N; i ++){
        int x = rand() % N; 
        int y = rand() % N; 

        temp = temp_ptr[x];
        temp_ptr[x] = temp_ptr[y];
        temp_ptr[y] = temp;
    }

    for (int i = 0; i < N / 2; i ++){
        assert(list_delete(list, temp_ptr[i]));
    }

    for (int i = N / 2; i < N; i ++){
        if (i & 1) assert(list_delete_back(list));
        else assert(list_delete_front(list));
    }

    for (int i = 0; i < N; i ++) free(temp_ptr[i]);

    // 第二次
    for (int i = 0; i < N / 2; i ++){
        temp_ptr[i] = (void *)node_int_new(i);
        if (i % 4 == 0){
            assert(list_insert_back(list, temp_ptr[i]));
        }else if (i % 4 == 1){
            assert(list_insert_front(list, temp_ptr[i]));
        }else if (i % 4 == 2){
            assert(
                list_insert_after(list, temp_ptr[i], list->begin)
            );
        }else{
            assert(
                list_insert_before(list, temp_ptr[i], list->end)
            );
        }
    }

    for (int i = N / 2; i < N; i ++){
        temp_ptr[i] = (void *)node_int_new(i);
        if (i & 1){
            assert(list_insert_before(list, temp_ptr[i], temp_ptr[rand() % (i - 1)]));
        }else{
            assert(list_insert_after(list, temp_ptr[i], temp_ptr[rand() % (i - 1)]));
        }      
    }

    assert(calc_count(list) == N);

    for (int i = 0; i < N; i ++){
        int x = rand() % N; 
        int y = rand() % N; 

        temp = temp_ptr[x];
        temp_ptr[x] = temp_ptr[y];
        temp_ptr[y] = temp;
    }

    for (int i = 0; i < N / 2; i ++){
        assert(list_delete(list, temp_ptr[i]));
    }

    for (int i = N / 2; i < N; i ++){
        if (i & 1) assert(list_delete_back(list));
        else assert(list_delete_front(list));
    }


    for (int i = 0; i < N; i ++) free(temp_ptr[i]);
    free(list);
}

int main(){
    srand((int)15179416016);
    test1(); 
    printf("test1 pass\n");
}