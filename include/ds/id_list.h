#include "ds/ping_arena.h"
#include "ds/linklist.h"

typedef struct ID{
    ListNode node;
    int number;
}ID; // ID list 的 id 数据单元，继承了链表

// 从 IDList中get或者add数字，数字从 [0 - size - 1] 分布
typedef struct IDList{
    List * list;
    void * alloc_ptr; // 该 IDList 通常使用连续内存，需要记录一开始存放信息的地址用来回收内存
}IDList; 

void id_list_free(IDList *list);
IDList *id_list_create(int size);
int id_list_get(IDList *list); 
void id_list_add(IDList *list,int number);
