#include <stdio.h>
#include "ds/ping_arena.h"
#include "ds/linklist.h"
#include "other/def.h"

typedef struct ID{
    ListNode node;
    int number;
}ID;

typedef struct IDList{
    List * list;
    void * alloc_ptr; // 该 IDList 通常使用连续内存，需要记录一开始存放信息的地址用来回收内存
}IDList;

void id_list_add(IDList* list, int number){
    void * node = &((ID*)(list->alloc_ptr))[number];
    list_insert_front(list->list, node);
}

int id_list_get(IDList* list){
    if (unlikely(list_is_empty(list->list))){
        return -1;
    }

    List *l = list->list;
    ListNode *head = l->begin;
    int val = ((ID *)head)->number;

    ListNode *next = head->next;
    if (next != NULL) {
        next->prev = NULL;
    } else {
        l->end = NULL;
    }
    l->begin = next;
    head->prev = NULL;
    head->next = NULL;

    return val;
}

IDList* id_list_create(int size){
    IDList * list = (IDList *)malloc(sizeof(IDList));
    ID *a = (ID *)malloc(sizeof(ID) * size);
    list->list = (List *)malloc(sizeof(List));

    list->list->begin = (void *) a;
    list->list->end = (void *) a;

    for (int i = 0; i < size; i ++){
        if (likely(i != 0 && i != size - 1)){
            a[i].node.next = (void *)(&a[i + 1]);
            a[i].node.prev = (void *)(&a[i - 1]);
        }else{
            if (i == 0){
                a[i].node.next = (void *)(&a[i + 1]);
            }else{
                a[i].node.prev = (void *)(&a[i - 1]);
            }
        }

        a[i].number = i;
    }

    list->alloc_ptr = (void *)a;
    return list;
}

void id_list_free(IDList * list){
    free(list->alloc_ptr);
    free(list->list);
    free(list);
}
