#ifndef _HOPSCOTCH_H
#define _HOPSCOTCH_H

#include <stdint.h>
#include <stdlib.h>

/* Initial size of buckets.  2 to the power of this value will be allocated. */
#define HOPSCOTCH_INIT_BSIZE_EXPONENT   21 // 2097152 buckets
/* Bitmap size used for linear probing in hopscotch hashing */
#define HOPSCOTCH_HOPINFO_SIZE          32

/*
 * Buckets
 */
struct hopscotch_bucket {
    void *key;
    void *data;
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
    exponent = HOPSCOTCH_INIT_BSIZE_EXPONENT;
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
hopscotch_lookup(struct hopscotch_hash_table *ht, void *key)
{
    uint32_t h;
    size_t idx;
    size_t i;
    size_t sz;

    sz = 1ULL << ht->exponent;
    h = _jenkins_hash(key, ht->keylen);
    idx = h & (sz - 1);

    if ( !ht->buckets[idx].hopinfo ) {
        return NULL;
    }
    for ( i = 0; i < HOPSCOTCH_HOPINFO_SIZE; i++ ) {
        if ( ht->buckets[idx].hopinfo & (1 << i) ) {
            if ( 0 == memcmp(key, ht->buckets[idx + i].key, ht->keylen) ) {
                /* Found */
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
hopscotch_insert(struct hopscotch_hash_table *ht, void *key, void *data)
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
        return -1;
    }

    sz = 1ULL << ht->exponent;
    h = _jenkins_hash(key, ht->keylen);
    idx = h & (sz - 1);

    /* Linear probing to find an empty bucket */
    for ( i = idx; i < sz; i++ ) {
        if ( NULL == ht->buckets[i].key ) {
            /* Found an available bucket */
            while ( i - idx >= HOPSCOTCH_HOPINFO_SIZE ) {
                for ( j = 1; j < HOPSCOTCH_HOPINFO_SIZE; j++ ) {
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
                if ( j >= HOPSCOTCH_HOPINFO_SIZE ) {
                    return -1;
                }
            }

            off = i - idx;
            ht->buckets[i].key = key;
            ht->buckets[i].data = data;
            ht->buckets[idx].hopinfo |= (1ULL << off);

            return 0;
        }
    }

    return -1;
}

/*
 * Remove an item
 */
void *
hopscotch_remove(struct hopscotch_hash_table *ht, void *key)
{
    uint32_t h;
    size_t idx;
    size_t i;
    size_t sz;
    void *data;

    sz = 1ULL << ht->exponent;
    h = _jenkins_hash(key, ht->keylen);
    idx = h & (sz - 1);

    if ( !ht->buckets[idx].hopinfo ) {
        return NULL;
    }
    for ( i = 0; i < HOPSCOTCH_HOPINFO_SIZE; i++ ) {
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

#ifdef __cplusplus
extern "C" {
#endif

    /* in hopscotch.c */
    struct hopscotch_hash_table *
    hopscotch_init(struct hopscotch_hash_table *, size_t);
    void hopscotch_release(struct hopscotch_hash_table *);
    void * hopscotch_lookup(struct hopscotch_hash_table *, void *);
    int hopscotch_insert(struct hopscotch_hash_table *, void *, void *);
    void * hopscotch_remove(struct hopscotch_hash_table *, void *);
    int hopscotch_resize(struct hopscotch_hash_table *, int);

#ifdef __cplusplus
}
#endif


#endif /* _HOPSCOTCH_H */