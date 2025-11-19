// dgraph_simple.h — ultra-simple directed graph with key -> (value*, owner).
#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

typedef uint64_t gkey_t;

typedef enum { OWNER_LOCAL = 0, OWNER_REMOTE = 1 } owner_t;

typedef struct {
    gkey_t  *data; // pointer to contiguous array of keys
    int      len, cap; // how many elements are used and allocated
} keyvec_t;

static inline void keyvec_push(keyvec_t *v, gkey_t x){
    if (v->len == v->cap){
        v->cap = v->cap ? v->cap*2 : 4;
        v->data = (gkey_t*)realloc(v->data, v->cap * sizeof(gkey_t));
    }
    v->data[v->len++] = x;
}

typedef struct {
    gkey_t   key;     // node id 
    void    *value;   // pointer to your object
    owner_t  owner;   // LOCAL or REMOTE
    keyvec_t out;     // children keys (directed edges)
} dnode_t;

typedef struct {
    dnode_t *nodes;   // plain array
    int      n;       // number of nodes used
    int      cap;     // capacity
} dgraph_t;

/* ---------- basic helpers ---------- */

static inline dgraph_t* dg_new(int cap_hint){
    dgraph_t *g = (dgraph_t*)calloc(1, sizeof(*g));
    g->cap = cap_hint > 0 ? cap_hint : 8;
    g->nodes = (dnode_t*)calloc(g->cap, sizeof(dnode_t));
    return g;
}

static inline void dg_free(dgraph_t *g){
    if (!g) return;
    for (int i=0;i<g->n;i++) free(g->nodes[i].out.data);
    free(g->nodes);
    free(g);
}

static inline int dg_find_index(const dgraph_t *g, gkey_t key){
    for (int i=0;i<g->n;i++) if (g->nodes[i].key == key) return i;
    return -1;
}
//if array full, double its size
static inline void dg_grow(dgraph_t *g){
    g->cap = g->cap ? g->cap*2 : 8;
    g->nodes = (dnode_t*)realloc(g->nodes, g->cap * sizeof(dnode_t));
    // zero-init the new half
    memset(&g->nodes[g->n], 0, (g->cap - g->n)*sizeof(dnode_t));
}

/* Add node if not present. Returns 1 if inserted, 0 if existed. */
static inline int dg_add_node(dgraph_t *g, gkey_t key, void *value, owner_t owner){
    int ix = dg_find_index(g, key);
    if (ix >= 0){ // update value/owner if you want
        g->nodes[ix].value = value;
        g->nodes[ix].owner = owner;
        return 0;
    }
    if (g->n == g->cap) dg_grow(g);
    dnode_t *nn = &g->nodes[g->n];
    nn->key   = key;
    nn->value = value;
    nn->owner = owner;
    nn->out.data = NULL; nn->out.len = nn->out.cap = 0;
    g->n++;
    return 1;
}

/* Directed edge: src -> dst (both keys must exist). */
static inline void dg_add_edge(dgraph_t *g, gkey_t src, gkey_t dst){
    int s = dg_find_index(g, src);
    int d = dg_find_index(g, dst);
    assert(s >= 0 && d >= 0); // add nodes first
    keyvec_t *out = &g->nodes[s].out;
    keyvec_push(out, dst);
}

/* Accessors */
static inline owner_t dg_owner(const dgraph_t *g, gkey_t key){
    int i = dg_find_index(g, key); assert(i>=0);
    return g->nodes[i].owner;
}
static inline void* dg_value(const dgraph_t *g, gkey_t key){
    int i = dg_find_index(g, key); assert(i>=0);
    return g->nodes[i].value;
}
static inline int dg_children(const dgraph_t *g, gkey_t key, const gkey_t **out_keys, int *out_len){
    int i = dg_find_index(g, key); if (i<0) return 0;
    *out_keys = g->nodes[i].out.data;
    *out_len  = g->nodes[i].out.len;
    return 1;
}

/* ---------- BFS on keys (linear search version) ---------- */

typedef struct {
    int *dist;    // by node index
    int *parent;  // by node index (index of parent, or -1)
} bfs_result_t;

/* Build a simple index array so we can convert key -> idx faster inside BFS.
   (Still O(V) per lookup because it's linear search, but we reuse results.) */
static inline int* dg_build_index_array(const dgraph_t *g){
    int *idx = (int*)malloc(g->n * sizeof(int));
    for (int i=0;i<g->n;i++) idx[i] = i;
    return idx;
}

static inline int key_to_idx(const dgraph_t *g, gkey_t k){
    // linear scan (simple!)
    for (int i=0;i<g->n;i++) if (g->nodes[i].key == k) return i;
    return -1;
}

static inline bfs_result_t dg_bfs(const dgraph_t *g, gkey_t src_key){
    bfs_result_t R = {0};
    R.dist   = (int*)malloc(g->n * sizeof(int));
    R.parent = (int*)malloc(g->n * sizeof(int));
    for (int i=0;i<g->n;i++){ R.dist[i] = -1; R.parent[i] = -1; }

    int src = key_to_idx(g, src_key);
    assert(src >= 0);

    int *q = (int*)malloc(g->n * sizeof(int));
    int head=0, tail=0;

    R.dist[src]=0; R.parent[src]=src; q[tail++]=src;

    while (head != tail){
        int u = q[head++]; if (head==g->n) head=0;
        const keyvec_t *out = &g->nodes[u].out;
        for (int i=0;i<out->len;i++){
            int v = key_to_idx(g, out->data[i]);
            if (v >= 0 && R.dist[v] == -1){
                R.dist[v] = R.dist[u] + 1;
                R.parent[v] = u;
                q[tail++] = v; if (tail==g->n) tail=0;
            }
        }
    }

    free(q);
    return R;
}

static inline void dg_bfs_free(bfs_result_t *R){
    if (!R) return; free(R->dist); free(R->parent); R->dist=R->parent=NULL;
}

/* Path in KEYS; returns malloc'd array, set *out_len. */
static inline gkey_t* dg_build_path(const dgraph_t *g, gkey_t src_key, gkey_t dst_key,
                                    const bfs_result_t *R, int *out_len){
    int src = key_to_idx(g, src_key), dst = key_to_idx(g, dst_key);
    if (src<0 || dst<0 || !R){ *out_len=0; return NULL; }
    gkey_t *tmp = (gkey_t*)malloc(g->n * sizeof(gkey_t)); int len=0;
    for (int v=dst; v!=-1 && v!=R->parent[v]; v=R->parent[v]){
        tmp[len++] = g->nodes[v].key;
    }
    tmp[len++] = g->nodes[src].key;
    gkey_t *path = (gkey_t*)malloc(len * sizeof(gkey_t));
    for (int i=0;i<len;i++) path[i] = tmp[len-1-i];
    free(tmp);
    if (path[0] != src_key){ free(path); *out_len=0; return NULL; }
    *out_len = len; return path;
}
