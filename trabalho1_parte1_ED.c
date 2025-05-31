#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <string.h>
#include <assert.h>
#include <math.h>

/*Estrutura de dados para embeddings*/
typedef struct {
    float embedding[128];
    char id[100];
} treg;

void *aloca_reg(float embedding[], const char nome[]) {
    treg *reg = malloc(sizeof(treg));
    memcpy(reg->embedding, embedding, sizeof(float) * 128);
    strncpy(reg->id, nome, 100);
    return reg;
}

int comparador(void *a, void *b, int pos) {
    float diff = ((treg *)a)->embedding[pos] - ((treg *)b)->embedding[pos];
    return (diff > 0) - (diff < 0);  // retorna 1, -1 ou 0
}

double distancia(void *a, void *b) {
    double dist = 0.0;
    for (int i = 0; i < 128; i++) {
        float da = ((treg *)a)->embedding[i];
        float db = ((treg *)b)->embedding[i];
        float d = da - db;
        dist += d * d;
    }
    return dist;
}

/*Estrutura da KDTree*/
typedef struct _node {
    void *key;
    struct _node *esq;
    struct _node *dir;
} tnode;

typedef struct {
    tnode *raiz;
    int (*cmp)(void *, void *, int);
    double (*dist)(void *, void *);
    int k;
} tarv;

void kdtree_constroi(tarv *arv, int (*cmp)(void *, void *, int),
                     double (*dist)(void *, void *), int k) {
    arv->raiz = NULL;
    arv->cmp = cmp;
    arv->dist = dist;
    arv->k = k;
}

void _kdtree_insere(tnode **raiz, void *key, int (*cmp)(void *, void *, int), int profund, int k) {
    if (*raiz == NULL) {
        *raiz = malloc(sizeof(tnode));
        (*raiz)->key = key;
        (*raiz)->esq = NULL;
        (*raiz)->dir = NULL;
    } else {
        int pos = profund % k;
        if (cmp(key, (*raiz)->key, pos) < 0)
            _kdtree_insere(&((*raiz)->esq), key, cmp, profund + 1, k);
        else
            _kdtree_insere(&((*raiz)->dir), key, cmp, profund + 1, k);
    }
}

void kdtree_insere(tarv *arv, void *key) {
    _kdtree_insere(&(arv->raiz), key, arv->cmp, 0, arv->k);
}

void _kdtree_destroi(tnode *node) {
    if (node != NULL) {
        _kdtree_destroi(node->esq);
        _kdtree_destroi(node->dir);
        free(node->key);
        free(node);
    }
}

void kdtree_destroi(tarv *arv) {
    _kdtree_destroi(arv->raiz);
}

/*Heap máxima para N vizinhos*/
typedef struct {
    double distancia;
    tnode *no;
} heap_item;

typedef struct {
    heap_item *itens;
    int tamanho;
    int capacidade;
} max_heap;

max_heap *heap_cria(int capacidade) {
    max_heap *h = malloc(sizeof(max_heap));
    h->itens = malloc(sizeof(heap_item) * capacidade);
    h->tamanho = 0;
    h->capacidade = capacidade;
    return h;
}

void heap_swap(heap_item *a, heap_item *b) {
    heap_item tmp = *a;
    *a = *b;
    *b = tmp;
}

void heap_sobe(max_heap *h, int i) {
    while (i > 0) {
        int pai = (i - 1) / 2;
        if (h->itens[i].distancia <= h->itens[pai].distancia) break;
        heap_swap(&h->itens[i], &h->itens[pai]);
        i = pai;
    }
}

void heap_desce(max_heap *h, int i) {
    int maior = i;
    int esq = 2 * i + 1, dir = 2 * i + 2;
    if (esq < h->tamanho && h->itens[esq].distancia > h->itens[maior].distancia)
        maior = esq;
    if (dir < h->tamanho && h->itens[dir].distancia > h->itens[maior].distancia)
        maior = dir;
    if (maior != i) {
        heap_swap(&h->itens[i], &h->itens[maior]);
        heap_desce(h, maior);
    }
}

void heap_insere(max_heap *h, tnode *no, double dist) {
    if (h->tamanho < h->capacidade) {
        h->itens[h->tamanho].no = no;
        h->itens[h->tamanho].distancia = dist;
        heap_sobe(h, h->tamanho);
        h->tamanho++;
    } else if (dist < h->itens[0].distancia) {
        h->itens[0].no = no;
        h->itens[0].distancia = dist;
        heap_desce(h, 0);
    }
}

void heap_destroi(max_heap *h) {
    free(h->itens);
    free(h);
}

/*Busca com N vizinhos*/
void _kdtree_busca_n(tarv *arv, tnode **atual, void *key, int profund, max_heap *heap) {
    if (*atual != NULL) {
        double dist_atual = arv->dist((*atual)->key, key);
        heap_insere(heap, *atual, dist_atual);

        int pos = profund % arv->k;
        int comp = arv->cmp(key, (*atual)->key, pos);

        tnode **lado_principal, **lado_oposto;
        if (comp < 0) {
            lado_principal = &((*atual)->esq);
            lado_oposto = &((*atual)->dir);
        } else {
            lado_principal = &((*atual)->dir);
            lado_oposto = &((*atual)->esq);
        }

        _kdtree_busca_n(arv, lado_principal, key, profund + 1, heap);

        float diff = ((treg *)key)->embedding[pos] - ((treg *)(*atual)->key)->embedding[pos];
        if ((diff * diff) < heap->itens[0].distancia || heap->tamanho < heap->capacidade) {
            _kdtree_busca_n(arv, lado_oposto, key, profund + 1, heap);
        }
    }
}

void buscar_n_vizinhos(tarv *arv, treg *query, int n) {
    max_heap *heap = heap_cria(n);
    _kdtree_busca_n(arv, &(arv->raiz), query, 0, heap);

    printf("N vizinhos mais próximos de '%s':\n", query->id);
    for (int i = 0; i < heap->tamanho; i++) {
        treg *viz = (treg *)heap->itens[i].no->key;
        printf("%s (distância: %.4f)\n", viz->id, heap->itens[i].distancia);
    }

    heap_destroi(heap);
}

/*Teste*/
int main() {
    tarv arv;
    kdtree_constroi(&arv, comparador, distancia, 128);

    float a[128] = {0}, b[128] = {0}, c[128] = {0}, d[128] = {0};
    a[0] = 1.0f; b[0] = 2.0f; c[0] = 3.0f; d[0] = 4.0f;

    kdtree_insere(&arv, aloca_reg(a, "Ana"));
    kdtree_insere(&arv, aloca_reg(b, "Bruno"));
    kdtree_insere(&arv, aloca_reg(c, "Carlos"));
    kdtree_insere(&arv, aloca_reg(d, "Daniela"));

    float query_emb[128] = {0};
    query_emb[0] = 2.5f;
    treg *query = aloca_reg(query_emb, "Consulta");

    buscar_n_vizinhos(&arv, query, 2);

    free(query);
    kdtree_destroi(&arv);
    return 0;
}
