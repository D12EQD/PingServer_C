#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>

#define list_ptr(val) ((List *)(val))
#define list_is_empty(val) ((!((list_ptr(val)))->begin))

struct ListNode_s{
    struct ListNode_s* prev;
    struct ListNode_s* next;
};

struct List_s{
    struct ListNode_s * begin; // 指向开始的位置
    struct ListNode_s * end; // 指向最后一个位置
};


typedef struct ListNode_s ListNode;
typedef struct List_s List;

/* Interface */
#define list_insert_front(l, n) list_insert_front_imple((List* )l, (ListNode *)n)
#define list_insert_back(l, n) list_insert_back_imple((List* )l, (ListNode *)n)

// 遍历整个链表找到node 在node节点后插入n这个节点
#define list_insert_after(l, n, node) list_insert_after_imple((List* )l, (ListNode *)n, (ListNode *) node) 

// 遍历整个链表找到node 在node节点前插入n这个节点
#define list_insert_before(l, n, node) list_insert_before_imple((List* )l, (ListNode *)n, (ListNode *) node)
#define list_delete(l, node) list_delete_imple((List* )l, (ListNode *) node) 
#define link_node_delete(l, n) link_node_delete_imple((List *)l, (ListNode *) n)
#define link_list_init(l) link_list_init_imple((List *)l)
#define link_list_free(l) link_list_free_imple((List *)l)

/* This file was automatically generated.  Do not edit! */
bool list_delete_imple(List *l,ListNode *node);
bool list_delete_back(List *l);
bool list_delete_front(List *l);
bool list_insert_before_imple(List *l,ListNode *n,ListNode *node);
bool list_insert_after_imple(List *l,ListNode *n,ListNode *node);
bool list_insert_back_imple(List *l,ListNode *n);
bool list_insert_front_imple(List *l,ListNode *n);
void link_node_delete_imple(List*l, ListNode *n);

void link_list_init_imple(List *l);
void link_list_free_imple(List *l);