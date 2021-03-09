#ifndef HASHMAP_H
#define HASHMAP_H

#include <stdlib.h>
#include <pthread.h>

typedef struct map_node map_node;
struct map_node {
    void* key;
    void* value;
    struct map_node* next;
};

typedef struct hash_map hash_map;
struct hash_map { //Modify this!
    struct map_node** buckets;
    int n;
    int N;
    void (*key_destruct)(void*);
    void (*value_destruct)(void*);
    int (*cmp)(void*,void*);
    size_t (*hash)(void*);
    pthread_mutex_t lock;
};




struct hash_map* hash_map_new(size_t (*hash)(void*), int (*cmp)(void*,void*),
                              void (*key_destruct)(void*), void (*value_destruct)(void*));

void hash_map_put_entry_move(struct hash_map* map, void* k, void* v);

void hash_map_remove_entry(struct hash_map* map, void* k);

void* hash_map_get_value_ref(struct hash_map* map, void* k);

void hash_map_destroy(struct hash_map* map);

#endif