/*
* 支持多态功能的链表，可以插入任意大小的内容
* 内存管理：
*   - 注意没有分配内存的能力，需要用户根据自己需求分配内存节点并且插入链表当中
*   - 包括一个free link_list功能
*/

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include "ds/linklist.h"
#include "other/debug.h"

/*
* 注意该函数只是初始化不创建
*/
void link_list_init(link_list_t *l){
    if (!l){
        DEBUG(DEBUG_FLAG_ALL, "Serious bug, link_list is null\n");
        return;
    }

    l->begin = NULL;
    l->end = NULL;
    l->now = NULL;
    return;
}   

/*
* 插入链表首部
* begin -> begin_next
* n -> begin -> begin_next
*/
bool _list_insert_front(link_list_t *l, link_list_base_node_t *n){
    if (l->begin == NULL && l->end == NULL){
        // 空链表
        l->begin = n;
        l->end = n;

        n->prev = NULL;
        n->next = NULL;
        return true;
    }
    
    link_list_base_node_t * b = l->begin;
    n->next = b;    
    b->prev = n;    
    l->begin = n;   
    n->prev = NULL; 

    return true;
}

/*
* 插入数据到尾部
* end -> NULL 
* end -> n -> NULL 
*/
bool _list_insert_back(link_list_t *l, link_list_base_node_t * n){
    if (!l) return false;
    if (l->begin == NULL && l->end == NULL){
        // 空链表
        l->begin = n;
        l->end = n;

        n->prev = NULL;
        n->next = NULL;

        // printf("[list_insert_back] ");
        return true;
    }

    link_list_base_node_t * e = l->end;
    l -> end = n;

    // e -> prev = e -> prev; 
    e -> next = n;

    n -> prev = e;
    n -> next = NULL;

    return true;
}

/*
* 遍历链表中的节点，比较地址相同的node地址并且在next插入该节点
*/
bool _list_insert_after(link_list_t *l, link_list_base_node_t * n, link_list_base_node_t * node){
    if (list_is_empty(l)){
        return false;
    }

    for (link_list_base_node_t * i = l->begin; i != NULL; i = i->next){
        if (i == node){
            if (i == l->end) return list_insert_back(l, n);
            
            /*
                i - i_next
                i - n - i_next
            */
            
            n->next = i->next;
            n->prev = i;

            if (i->next) i->next->prev = n;
            i->next = n;

            
            return true;
        }
    }

    // printf("not find\n");
    return false;
}


/*
* 遍历链表中的节点，比较地址相同的node地址并且在prev插入该节点
*/
bool _list_insert_before(link_list_t *l, link_list_base_node_t * n, link_list_base_node_t * node){
    if (list_is_empty(l)) return false;

    for (link_list_base_node_t * i = l->begin; i != NULL; i = i->next){
        if (i == node){
            if (i == l->begin) return list_insert_front(l, n);
            
            /*
                i_prve - i 
                i_prve - n - i 
            */

            n->next = i;
            n->prev = i->prev;

            i->prev->next = n;
            i->prev = n;
            // i->next = i->next;
            
            return true;
        }
    }

    return false;
}

bool list_delete_front(link_list_t * l){
    if (list_is_empty(l)) return false;

    if (l->begin == l->end){
        l->begin->next = NULL;
        l->begin->prev = NULL;

        l->begin = NULL;
        l->end = NULL;
        return true;
    }

    link_list_base_node_t *b = l->begin;
    l->begin = b->next;
    b->next->prev = NULL;

    b->next = NULL;
    b->prev = NULL;
    return true;
}

bool list_delete_back(link_list_t * l){
    if (list_is_empty(l)) return false;

    if (l->begin == l->end){
        l->begin->next = NULL;
        l->begin->prev = NULL;

        l->begin = NULL;
        l->end = NULL;
        return true;
    }

    link_list_base_node_t * e = l->end;
    e->prev->next = NULL;
    l->end = e->prev;

    e->next = NULL;
    e->prev = NULL;
    return true;
}

bool _list_delete(link_list_t * l, link_list_base_node_t * node){
    if (list_is_empty(l)) return false;
    if (node == l->begin) return list_delete_front(l);
    if (node == l->end) return list_delete_back(l);

    for (link_list_base_node_t * i = l->begin; i; i = i -> next){
        if (i == node){
            // i->next - i - i->prev
            i->prev->next = i->next;  
            i->next->prev = i->prev;  
            i->prev = NULL;
            i->next = NULL;

            return true;
        }
    }

    return false;
}

/*
* 释放整个链表节点，并且释放链表本身
*/
void link_list_free(link_list_t *l){
    link_list_base_node_t *cur = l->begin;
    while (cur) {
        link_list_base_node_t *next = cur->next;
        free(cur);
        cur = next;
    }
    free(l);
}

/*
* 指定node，从当前链表中直接删除
*/
void _link_node_delete(link_list_t *l, link_list_base_node_t *node) {
    if (!l || !node) return;
    
    if (l->now == node) {
        l->now = node->next ? node->next : node->prev;
    }
    
    if (l->begin == node) {
        l->begin = node->next;
    }
    
    if (l->end == node) {
        l->end = node->prev;
    }
    
    if (node->next) node->next->prev = node->prev;
    if (node->prev) node->prev->next = node->next;
    
    node->next = NULL;
    node->prev = NULL;
}