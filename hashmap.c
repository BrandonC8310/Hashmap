#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include "hashmap.h"


void node_destruct(struct hash_map* map, struct map_node* map_node) {
    map->key_destruct(map_node->key);
    map->value_destruct(map_node->value);
    free(map_node);
}
struct hash_map* hash_map_new(size_t (*hash)(void*), int (*cmp)(void*,void*),
                              void (*key_destruct)(void*), void (*value_destruct)(void*)) {
    if (hash == NULL || cmp == NULL || key_destruct == NULL || value_destruct == NULL)
        return NULL;
    hash_map* map = (hash_map*)malloc(sizeof(hash_map));
    map->n = 0;
    map->N = 16;
    map->cmp = cmp;
    map->hash = hash;
    map->key_destruct = key_destruct;
    map->value_destruct = value_destruct;
    map->buckets = (map_node**)malloc(sizeof(map_node*) * map->N);
    for (int i = 0; i < map->N; i++) {
        map->buckets[i] = NULL;
    }
    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);
    map->lock = mutex;
    return map;
}
void rehash(struct hash_map* map) {
    map->buckets = (map_node**)realloc(map->buckets, sizeof(map_node*) * map->N * 2);
    for (int i = map->N; i < map->N * 2; i++) {
        map->buckets[i] = NULL;
    }
    for (int i = 0; i < map->N; i++) {
        if (map->buckets[i] == NULL) {
            continue;
        }
        map_node* head = map->buckets[i];
        while ((map->hash(head->key) % (map->N * 2) != i)) {
            map_node* temp = head->next;

            //rehash_add(map, map->buckets, head, &map->n);
            unsigned long index = map->hash(head->key) % (map->N * 2);
            map_node *entry = map->buckets[index];
            if (entry == NULL) {
                // doesnt exist
//                map->n++;
                map->buckets[index] = head;
                head->next = NULL;
            } else {
                while (entry->next != NULL) {
                    entry = entry->next;
                }
                entry->next = head;
                head->next = NULL;
            }

            head = temp;
            if (head == NULL) {
                // all removed
                map->buckets[i] = NULL;
//                map->n--;
                break;
            }
        }
        if (head == NULL) {
            continue;
        }
        // find the first remaining map_node -> head & prev
        map->buckets[i] = head;
        map_node* prev = head;
        map_node* curr = head->next;
        while (curr != NULL) {
            if (map->hash(curr->key) % (map->N * 2) != i) {
                prev->next = curr->next;
                map_node* temp = curr->next;

                //rehash_add(map, map->buckets, curr, &map->n);
                unsigned long index = map->hash(curr->key) % (map->N * 2);
                map_node *entry = map->buckets[index];
                if (entry == NULL) {
                    // doesnt exist
//                    map->n++;
                    map->buckets[index] = curr;
                    curr->next = NULL;
                } else {
                    while (entry->next != NULL) {
                        entry = entry->next;
                    }
                    entry->next = curr;
                    curr->next = NULL;
                }

                curr = temp;
            } else {
                prev = curr;
                curr = curr->next;
            }
        }
    }
    map->N = map->N * 2;
}


void hash_map_put_entry_move(struct hash_map* map, void* k, void* v) {
    if (k == NULL || v == NULL) {
        return;
    }
    pthread_mutex_lock(&map->lock);

    if ((double)map->n / (double)map->N >= 0.75) {
        rehash(map);
    }

    unsigned long index = map->hash(k) % map->N;
    map_node* entry = map->buckets[index];
    if (entry == NULL) {
        // doesnt exist
        map->n++;
        entry = (map_node*)malloc(sizeof(map_node));
        entry->key = k;
        entry->value = v;
        entry->next = NULL;
        map->buckets[index] = entry;
    } else if (map->cmp(entry->key, k) == 1) {
        //replace first
        map->key_destruct(entry->key);
        map->value_destruct(entry->value);
        entry->key = k;
        entry->value = v;
    } else {
        // separate chaining
        while (entry->next != NULL) {
            if (map->cmp(entry->next->key, k) == 1) {
                //replace
                map->key_destruct(entry->next->key);
                map->value_destruct(entry->next->value);
                entry->next->key = k;
                entry->next->value = v;
                pthread_mutex_unlock(&map->lock);
                return;
            }
            entry = entry->next;
        }
        map->n++;
        map_node* new_entry = (map_node*)malloc(sizeof(map_node));
        new_entry->key = k;
        new_entry->value = v;
        new_entry->next = NULL;
        entry->next = new_entry;
    }
    pthread_mutex_unlock(&map->lock);
}

void hash_map_remove_entry(struct hash_map* map, void* k) {
    if (k == NULL) {
        return;
    }
    pthread_mutex_lock(&map->lock);

    unsigned long index = map->hash(k) % map->N;
    map_node* entry = map->buckets[index];
    if (entry == NULL) {
        // doesn't exist
        pthread_mutex_unlock(&map->lock);
        return;
    }
    if (map->cmp(entry->key, k) == 1) {
        map->buckets[index] = entry->next;
        node_destruct(map, entry);
        map->n--;
        pthread_mutex_unlock(&map->lock);
        return;
    }
    map_node* temp = entry;
    entry = entry->next;
    while (entry != NULL) {
        if (map->cmp(entry->key, k) == 1) {
            temp->next = entry->next;
            node_destruct(map, entry);
            map->n--;
            pthread_mutex_unlock(&map->lock);
            return;
        }
        temp = entry;
        entry = entry->next;
    }
    pthread_mutex_unlock(&map->lock);
}

void* hash_map_get_value_ref(struct hash_map* map, void* k) {
    if (k == NULL) {
        return NULL;
    }
    unsigned long index = map->hash(k) % map->N;
    map_node* entry = map->buckets[index];
    if (entry == NULL) {
        // doesn't exist
        return NULL;
    }
    while (map->cmp(entry->key, k) != 1) {
        entry = entry->next;
        if (entry == NULL) {
            return NULL;
        }
    }
    return entry->value;
}

void hash_map_destroy(struct hash_map* map) {
    for (int i = 0; i < map->N; i++) {
        if (map->buckets[i] == NULL) {
            continue;
        }
        map_node* n = map->buckets[i];
        while (n != NULL) {
            map_node* temp = n->next;
            node_destruct(map, n);
            n = temp;
        }
    }
    free(map->buckets);
    pthread_mutex_destroy(&map->lock);
    free(map);
}


void print_map(hash_map* map) {
    puts("START");
    for (int i = 0; i < map->N; i++) {
        if (map->buckets[i] == NULL) {
            printf("\t%i\t---\n", i);
        } else {
            printf("\t%i\t", i);
            map_node* temp = map->buckets[i];
            while (temp != NULL) {
                printf("<%s, %d> - ", (char*)temp->key, *((int*)temp->value));
                temp = temp->next;
            }
            printf("\n");
        }
    }
    puts("END");
    printf("n = %d | N = %d\n "
           "LOAD FACTOR: %f\n", map->n, map->N, (double)map->n / (double)map->N);
}