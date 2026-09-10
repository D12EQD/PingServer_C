#include "other/def.h"
#include "ds/id_list.h"

// 向id_list中增加一个数字，这个数字必须是之前id_list_get的数字
void id_list_add(IDList* list, int number){
    void * node = &((ID*)(list->alloc_ptr))[number];
    list_insert_front(list->list, node);
}

// 从id_list中获取一个数字，这个数字和之前的数字不会重复
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

// 创建一个id_list，从id_list中获取一个数字不是重复的，数字范围为 [0 ~ size - 1]
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

// 销毁一个id_list释放其中的所有空间
void id_list_free(IDList * list){
    free(list->alloc_ptr);
    free(list->list);
    free(list);
}
