// demo.c
#include "graph.c"

int main(void){
    dgraph_t *g = dg_new(8);

    int A=111, B=222, C=333, D=444, E=555, F=666;

    dg_add_node(g, 0, &A, OWNER_LOCAL);
    dg_add_node(g, 1, &B, OWNER_REMOTE);
    dg_add_node(g, 2, &C, OWNER_LOCAL);
    dg_add_node(g, 3, &D, OWNER_REMOTE);
    dg_add_node(g, 4, &E, OWNER_LOCAL);
    dg_add_node(g, 5, &F, OWNER_REMOTE);

    // Directed edges
    dg_add_edge(g, 0,1);
    dg_add_edge(g, 0,2);
    dg_add_edge(g, 1,3);
    dg_add_edge(g, 2,3);
    dg_add_edge(g, 3,4);
    dg_add_edge(g, 4,5);

    bfs_result_t R = dg_bfs(g, 0);
    int len=0; gkey_t* path = dg_build_path(g, 0, 5, &R, &len);

    printf("dist(0->5) = %d\npath: ", R.dist[key_to_idx(g,5)]);
    for (int i=0;i<len;i++) printf("%llu ", (unsigned long long)path[i]);
    printf("\n");

    free(path); dg_bfs_free(&R); dg_free(g);
    return 0;
}
