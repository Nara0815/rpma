#ifndef _HOPSCOTCH_H
#define _HOPSCOTCH_H

#include <stdint.h>
#include <stdlib.h>

/* Initial size of the hopscotch table. 2^ TABLE_SIZE buckets allocated. */
#define TABLE_SIZE   21 // 2097152 buckets
/* Bitmap size used for linear probing in hopscotch hashing */
#define HOP_NUMBER   32

#define SEED    0x12345678


/*
 * Buckets
 */
struct hopscotch_bucket {
    char *key;
    char *data;
    uint32_t hopinfo;
} __attribute__ ((aligned (8)));

/*
 * Hash table of hopscotch hashing
 */
struct hopscotch_hash_table {
    size_t exponent;
    size_t keylen;
    struct hopscotch_bucket *buckets;
    int _allocated;
};

/*
* Murmur hash
*/
uint32_t MurmurOAAT_32(const char* str, uint32_t h)
{
    // One-byte-at-a-time hash based on Murmur's mix
    // Source: https://github.com/aappleby/smhasher/blob/master/src/Hashes.cpp
    for (; *str; ++str) {
        h ^= *str;
        h *= 0x5bd1e995;
        h ^= h >> 15;
    }
    return h;
}

/* djb2 hash
*/
uint32_t
djhash(uint8_t *str)
{
    uint32_t hash = 5381;
    int c;

    while (c = *str++)
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}


/*
 * Jenkins Hash Function
 */
static __inline__ uint32_t
_jenkins_hash(uint8_t *key, size_t len)
{
    uint32_t hash;
    size_t i;

    hash = 0;
    for ( i = 0; i < len; i++ ) {
        hash += key[i];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);

    return hash;
}

/*
 * Initialize the hash table
 */
struct hopscotch_hash_table *
hopscotch_init(struct hopscotch_hash_table *ht, size_t keylen)
{
    int exponent;
    struct hopscotch_bucket *buckets;

    /* Allocate buckets first */
    exponent = TABLE_SIZE;
    buckets = malloc(sizeof(struct hopscotch_bucket) * (1 << exponent));
    if ( NULL == buckets ) {
        return NULL;
    }
    memset(buckets, 0, sizeof(struct hopscotch_bucket) * (1 << exponent));

    if ( NULL == ht ) {
        ht = malloc(sizeof(struct hopscotch_hash_table));
        if ( NULL == ht ) {
            return NULL;
        }
        ht->_allocated = 1;
    } else {
        ht->_allocated = 0;
    }
    ht->exponent = exponent;
    ht->buckets = buckets;
    ht->keylen = keylen;

    return ht;
}

/*
 * Release the hash table
 */
void
hopscotch_release(struct hopscotch_hash_table *ht)
{
    free(ht->buckets);
    if ( ht->_allocated ) {
        free(ht);
    }
}


/*
 * Lookup
 */
void *
hopscotch_lookup(struct hopscotch_hash_table *ht, char *key)
{
    uint32_t h;
    size_t idx;
    size_t i;
    size_t sz;

    sz = 1ULL << ht->exponent;
    //h = MurmurOAAT_32(key, SEED);
    h = _jenkins_hash(key, ht->keylen);
    idx = h & (sz - 1);

    
    //printf("%s----%lu----%lu---%lu\n", key, (unsigned long)h, (unsigned long)idx, ht->buckets[idx].hopinfo);

    if ( !ht->buckets[idx].hopinfo ) {
        return NULL;
    }

    
    for ( i = 0; i < HOP_NUMBER; i++ ) {
        if ( ht->buckets[idx].hopinfo & (1 << i) ) {
            if ( 0 == memcmp(key, ht->buckets[idx + i].key, ht->keylen) ) {
                /* Found */

              //  printf("%s----%s---%d--%lu---%lu---%lu\n", key,ht->buckets[idx + i].key, i, (unsigned long)h, (unsigned long)idx, ht->buckets[idx].hopinfo);

 
                return ht->buckets[idx + i].data;
            }
        }
    }

    return NULL;
}

/*
 * Insert an entry to the hash table
 */
int
hopscotch_insert(struct hopscotch_hash_table *ht, char *key, char *data)
{
    uint32_t h;
    size_t idx;
    size_t i;
    size_t sz;
    size_t off;
    size_t j;

    /* Ensure the key does not exist.  Duplicate keys are not allowed. */
    if ( NULL != hopscotch_lookup(ht, key) ) {
        /* The key already exists. */
        //printf("the key exisats");
        return -2;
    }

    sz = 1ULL << ht->exponent;
    //h = hash(key);
    //h = MurmurOAAT_32(key, SEED);
    h = _jenkins_hash(key, ht->keylen);
    idx = h & (sz - 1);

   // printf("%lu---------%lu\n", (unsigned long)h, (unsigned long)idx);

    /* Linear probing to find an empty bucket */
    for ( i = idx; i < sz; i++ ) {
        if ( NULL == ht->buckets[i].key ) {
            /* Found an available bucket */
            while ( i - idx >= HOP_NUMBER ) {
                for ( j = 1; j < HOP_NUMBER; j++ ) {
                    if ( ht->buckets[i - j].hopinfo ) {
                        off = __builtin_ctz(ht->buckets[i - j].hopinfo);
                        if ( off >= j ) {
                            continue;
                        }
                        ht->buckets[i].key = ht->buckets[i - j + off].key;
                        ht->buckets[i].data = ht->buckets[i - j + off].data;
                        ht->buckets[i - j + off].key = NULL;
                        ht->buckets[i - j + off].data = NULL;
                        ht->buckets[i - j].hopinfo &= ~(1ULL << off);
                        ht->buckets[i - j].hopinfo |= (1ULL << j);
                        i = i - j + off;
                        break;
                    }
                }
                if ( j >= HOP_NUMBER ) {
                    //TODO: resize table;
                    return -3;
                }
            }

            off = i - idx;
            ht->buckets[i].key = key;
            ht->buckets[i].data = data;
            ht->buckets[idx].hopinfo |= (1ULL << off);

            if(ht->buckets[idx].hopinfo>32){
                //printf("off %lu\n", off);
                //printf("hopionfo %lu\n", ht->buckets[idx].hopinfo);
                return -4;
            }

            return 0;
        }
    }

    return -1;
}

/*
 * Remove an item
 */
void *
hopscotch_remove(struct hopscotch_hash_table *ht, char *key)
{
    uint32_t h;
    size_t idx;
    size_t i;
    size_t sz;
    char *data;

    sz = 1ULL << ht->exponent;
    h = _jenkins_hash(key, ht->keylen);
    idx = h & (sz - 1);

    if ( !ht->buckets[idx].hopinfo ) {
        return NULL;
    }
    for ( i = 0; i < HOP_NUMBER; i++ ) {
        if ( ht->buckets[idx].hopinfo & (1 << i) ) {
            if ( 0 == memcmp(key, ht->buckets[idx + i].key, ht->keylen) ) {
                /* Found */
                data = ht->buckets[idx + i].data;
                ht->buckets[idx].hopinfo &= ~(1ULL << i);
                ht->buckets[idx + i].key = NULL;
                ht->buckets[idx + i].data = NULL;
                return data;
            }
        }
    }

    return NULL;
}

/*
 * Resize the bucket size of the hash table
 */
int
hopscotch_resize(struct hopscotch_hash_table *ht, int delta)
{
    size_t sz;
    size_t oexp;
    size_t nexp;
    ssize_t i;
    struct hopscotch_bucket *nbuckets;
    struct hopscotch_bucket *obuckets;
    int ret;

    oexp = ht->exponent;
    nexp = ht->exponent + delta;
    sz = 1ULL << nexp;

    nbuckets = malloc(sizeof(struct hopscotch_bucket) * sz);
    if ( NULL == nbuckets ) {
        return -1;
    }
    memset(nbuckets, 0, sizeof(struct hopscotch_bucket) * sz);
    obuckets = ht->buckets;

    ht->buckets = nbuckets;
    ht->exponent = nexp;

    for ( i = 0; i < (1LL << oexp); i++ ) {
        if ( obuckets[i].key ) {
            ret = hopscotch_insert(ht, obuckets[i].key, obuckets[i].data);
            if ( ret < 0 ) {
                ht->buckets = obuckets;
                ht->exponent = oexp;
                free(nbuckets);
                return -1;
            }
        }
    }
    free(obuckets);

    return 0;
}

#endif /* _HOPSCOTCH_H */