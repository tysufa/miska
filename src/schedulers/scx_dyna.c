/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 Meta Platforms, Inc. and affiliates.
 * Copyright (c) 2022 Tejun Heo <tj@kernel.org>
 * Copyright (c) 2022 David Vernet <dvernet@meta.com>
 */
//#include "bpf/libbpf.h"
#include "linux/bpf.h"
#include <signal.h>
#include <assert.h>
#include <libgen.h>
#include <bpf/bpf.h>
#include <scx/common.h>
#include "scx_mine.bpf.skel.h"
#include <unistd.h>

#define MODESCHED 2 // 0 FIFO, 1 TAS MIN, 2 ARN
#define DYNA 1 // 0 sans dynamicité, 1 avec

#include <stdio.h>
#include <stdlib.h>



// ma struct qui représente le message

struct task_event{
	pid_t pid;
	char comm[16]; // nom du processus
	char prio; // s'il est prioritaire ou pas
};



// Définition des couleurs
typedef enum { RED, BLACK } Color;

// Structure d'un nœud
typedef struct Node {
    int data;
    Color color;
    struct Node *left, *right, *parent;
} Node;

// Structure de l'arbre (inclut un nœud sentinelle TNULL pour simplifier la gestion des feuilles)
typedef struct {
    Node *root;
    Node *TNULL;
} RBTree;

RBTree * rbtree;

RBTree * waitingTree;


// Signatures des fonctions
int Empty();


void switch_tree(){
    RBTree * tmp = rbtree;
    rbtree = waitingTree;
    waitingTree = tmp;
}

// Initialiser un nouveau nœud
Node* create_node(int data) {
    Node* node = (Node*)malloc(sizeof(Node));
    node->data = data;
    node->color = RED; // Les nouveaux nœuds sont toujours rouges par défaut
    node->left = NULL;
    node->right = NULL;
    node->parent = NULL;
    return node;
}

// Initialiser l'arbre avec sa sentinelle
RBTree* create_tree() {
    RBTree* tree = (RBTree*)malloc(sizeof(RBTree));
    Node* tnull = (Node*)malloc(sizeof(Node));
    tnull->color = BLACK;
    tnull->left = tnull;
    tnull->right = tnull;
    
    tree->TNULL = tnull;
    tree->root = tree->TNULL;
    return tree;
}

// Rotation gauche
void left_rotate(RBTree* tree, Node* x) {
    Node* y = x->right;
    x->right = y->left;
    if (y->left != tree->TNULL) {
        y->left->parent = x;
    }
    y->parent = x->parent;
    if (x->parent == NULL) {
        tree->root = y;
    } else if (x == x->parent->left) {
        x->parent->left = y;
    } else {
        x->parent->right = y;
    }
    y->left = x;
    x->parent = y;
}

// Rotation droite
void right_rotate(RBTree* tree, Node* x) {
    Node* y = x->left;
    x->left = y->right;
    if (y->right != tree->TNULL) {
        y->right->parent = x;
    }
    y->parent = x->parent;
    if (x->parent == NULL) {
        tree->root = y;
    } else if (x == x->parent->right) {
        x->parent->right = y;
    } else {
        x->parent->left = y;
    }
    y->right = x;
    x->parent = y;
}

// Réparer l'arbre après l'insertion (corriger les violations des règles)
void insert_fixup(RBTree* tree, Node* k) {
    Node* u;
    while (k->parent != NULL && k->parent->color == RED) {
        if (k->parent == k->parent->parent->right) {
            u = k->parent->parent->left; // L'oncle
            if (u->color == RED) {
                // Cas 1 : L'oncle est rouge
                u->color = BLACK;
                k->parent->color = BLACK;
                k->parent->parent->color = RED;
                k = k->parent->parent;
            } else {
                if (k == k->parent->left) {
                    // Cas 2 : L'oncle est noir et k est un enfant gauche (Zig-Zag)
                    k = k->parent;
                    right_rotate(tree, k);
                }
                // Cas 3 : L'oncle est noir et k est un enfant droit (Zig-Zig)
                k->parent->color = BLACK;
                k->parent->parent->color = RED;
                left_rotate(tree, k->parent->parent);
            }
        } else {
            // Miroir : k->parent est l'enfant gauche
            u = k->parent->parent->right; // L'oncle
            if (u->color == RED) {
                u->color = BLACK;
                k->parent->color = BLACK;
                k->parent->parent->color = RED;
                k = k->parent->parent;
            } else {
                if (k == k->parent->right) {
                    k = k->parent;
                    left_rotate(tree, k);
                }
                k->parent->color = BLACK;
                k->parent->parent->color = RED;
                right_rotate(tree, k->parent->parent);
            }
        }
        if (k == tree->root) break;
    }
    tree->root->color = BLACK;
}

// Insertion standard de type BST, suivie du rééquilibrage
void insertRB(RBTree* tree, int key) {
    Node* node = create_node(key);
    node->left = tree->TNULL;
    node->right = tree->TNULL;

    Node* y = NULL;
    Node* x = tree->root;

    // Trouver la position d'insertion
    while (x != tree->TNULL) {
        y = x;
        if (node->data < x->data) {
            x = x->left;
        } else {
            x = x->right;
        }
    }

    node->parent = y;
    if (y == NULL) {
        tree->root = node; // L'arbre était vide
    } else if (node->data < y->data) {
        y->left = node;
    } else {
        y->right = node;
    }

    // Si le nœud est la racine, on le colorie juste en noir et on a fini
    if (node->parent == NULL) {
        node->color = BLACK;
        return;
    }

    // Si le grand-parent n'existe pas, pas de violation possible
    if (node->parent->parent == NULL) {
        return;
    }

    // Appeler la fonction de correction
    insert_fixup(tree, node);
}

// Fonction utilitaire pour un affichage basique (In-order)
void inorder_helper(RBTree* tree, Node* node) {
    if (node != tree->TNULL) {
        inorder_helper(tree, node->left);
        printf("%d(%s) ", node->data, node->color == RED ? "R" : "B");
        inorder_helper(tree, node->right);
    }
}

void print_tree(RBTree* tree) {
    inorder_helper(tree, tree->root);
    printf("\n");
}

// Fonction utilitaire pour trouver le nœud avec la valeur minimale d'un sous-arbre
Node* minimum(RBTree* tree, Node* node) {
    while (node->left != tree->TNULL) {
        node = node->left;
    }
    return node;
}

// Remplace le sous-arbre enraciné en 'u' par le sous-arbre enraciné en 'v'
void transplant(RBTree* tree, Node* u, Node* v) {
    if (u->parent == NULL) {
        tree->root = v;
    } else if (u == u->parent->left) {
        u->parent->left = v;
    } else {
        u->parent->right = v;
    }
    v->parent = u->parent;
}

// Réparer l'arbre après la suppression d'un nœud noir
void delete_fixup(RBTree* tree, Node* x) {
    Node* w;
    while (x != tree->root && x->color == BLACK) {
        if (x == x->parent->left) {
            w = x->parent->right; // w est le frère de x
            
            // Cas 1 : Le frère w est rouge
            if (w->color == RED) {
                w->color = BLACK;
                x->parent->color = RED;
                left_rotate(tree, x->parent);
                w = x->parent->right;
            }
            
            // Cas 2 : Le frère w est noir, et ses deux enfants sont noirs
            if (w->left->color == BLACK && w->right->color == BLACK) {
                w->color = RED;
                x = x->parent;
            } else {
                // Cas 3 : Le frère w est noir, son enfant gauche est rouge, et le droit est noir
                if (w->right->color == BLACK) {
                    w->left->color = BLACK;
                    w->color = RED;
                    right_rotate(tree, w);
                    w = x->parent->right;
                }
                
                // Cas 4 : Le frère w est noir, et son enfant droit est rouge
                w->color = x->parent->color;
                x->parent->color = BLACK;
                w->right->color = BLACK;
                left_rotate(tree, x->parent);
                x = tree->root;
            }
        } else {
            // Miroir : x est l'enfant droit
            w = x->parent->left; // w est le frère de x
            
            if (w->color == RED) {
                w->color = BLACK;
                x->parent->color = RED;
                right_rotate(tree, x->parent);
                w = x->parent->left;
            }
            
            if (w->right->color == BLACK && w->left->color == BLACK) {
                w->color = RED;
                x = x->parent;
            } else {
                if (w->left->color == BLACK) {
                    w->right->color = BLACK;
                    w->color = RED;
                    left_rotate(tree, w);
                    w = x->parent->left;
                }
                
                w->color = x->parent->color;
                x->parent->color = BLACK;
                w->left->color = BLACK;
                right_rotate(tree, x->parent);
                x = tree->root;
            }
        }
    }
    x->color = BLACK;
}

// Supprimer un nœud avec une clé donnée
void delete_node(RBTree* tree, int key) {
    Node* z = tree->root;
    Node* x;
    Node* y;
    Color y_original_color;

    // Chercher le nœud à supprimer
    while (z != tree->TNULL) {
        if (z->data == key) break;
        if (key < z->data) z = z->left;
        else z = z->right;
    }

    // Le nœud n'existe pas
    if (z == tree->TNULL) return;

    y = z;
    y_original_color = y->color;

    // Si le nœud a moins de deux enfants, on fait une transplantation simple
    if (z->left == tree->TNULL) {
        x = z->right;
        transplant(tree, z, z->right);
    } else if (z->right == tree->TNULL) {
        x = z->left;
        transplant(tree, z, z->left);
    } else {
        // Le nœud a deux enfants : trouver le successeur (minimum du sous-arbre droit)
        y = minimum(tree, z->right);
        y_original_color = y->color;
        x = y->right;

        if (y->parent == z) {
            x->parent = y; // Gère le cas où le successeur est l'enfant direct
        } else {
            transplant(tree, y, y->right);
            y->right = z->right;
            y->right->parent = y;
        }

        transplant(tree, z, y);
        y->left = z->left;
        y->left->parent = y;
        y->color = z->color; // Le successeur prend la couleur du nœud supprimé
    }

    free(z); // Libérer la mémoire du nœud supprimé

    // Si la couleur originelle du nœud déplacé était noire, la hauteur noire a changé
    // Il faut donc rééquilibrer l'arbre
    if (y_original_color == BLACK) {
        delete_fixup(tree, x);
    }
}

// Fonction utilitaire récursive effectuant un parcours post-fixe
void free_node(RBTree* tree, Node* node) {
    if (node != tree->TNULL) {
        free_node(tree, node->left);   // Libérer le sous-arbre gauche
        free_node(tree, node->right);  // Libérer le sous-arbre droit
        free(node);                    // Libérer le nœud courant
    }
}

// Fonction principale pour détruire l'arbre rouge-noir
void free_tree(RBTree* tree) {
    if (tree != NULL) {
        // 1. Libérer tous les nœuds de l'arbre en partant de la racine
        free_node(tree, tree->root);
        
        // 2. Libérer le nœud sentinelle partagé (TNULL)
        if (tree->TNULL != NULL) {
            free(tree->TNULL);
        }
        
        // 3. Libérer la structure de contrôle de l'arbre
        free(tree);
    }
}


// Structure représentant le tas min
typedef struct {
    int *array;
    int size;
    int capacity;
} MinHeap;

// Fonctions utilitaires pour obtenir les indices
int parent(int i) { return (i - 1) / 2; }
int left_child(int i) { return (2 * i) + 1; }
int right_child(int i) { return (2 * i) + 2; }

// Fonction pour échanger deux entiers
void swap(int *x, int *y) {
    int temp = *x;
    *x = *y;
    *y = temp;
}

// Initialiser le tas
MinHeap* create_min_heap(int capacity) {
    MinHeap* heap = (MinHeap*)malloc(sizeof(MinHeap));
    if (!heap) return NULL;
    
    heap->size = 0;
    heap->capacity = capacity;
    heap->array = (int*)malloc(capacity * sizeof(int));
    
    if (!heap->array) {
        free(heap);
        return NULL;
    }
    return heap;
}

// Insérer une nouvelle valeur dans le tas
void insertHeap(MinHeap* heap, int value) {
    if (heap->size == heap->capacity) {
        printf("Erreur : Le tas est plein.\n");
        return;
    }

    // Insérer à la fin
    int i = heap->size;
    heap->size++;
    heap->array[i] = value;

    // Remonter la valeur pour restaurer la propriété du tas min
    while (i != 0 && heap->array[parent(i)] > heap->array[i]) {
        swap(&heap->array[i], &heap->array[parent(i)]);
        i = parent(i);
    }
}

// Maintenir la propriété du tas min de haut en bas (Heapify)
void min_heapify(MinHeap* heap, int i) {
    int left = left_child(i);
    int right = right_child(i);
    int smallest = i;

    if (left < heap->size && heap->array[left] < heap->array[smallest])
        smallest = left;

    if (right < heap->size && heap->array[right] < heap->array[smallest])
        smallest = right;

    if (smallest != i) {
        swap(&heap->array[i], &heap->array[smallest]);
        min_heapify(heap, smallest);
    }
}

// Extraire la valeur minimale (la racine)
int extract_min(MinHeap* heap) {
    if (heap->size <= 0)
        return -1; // Ou INT_MAX selon le besoin
    if (heap->size == 1) {
        heap->size--;
        return heap->array[0];
    }

    // Stocker le minimum et le remplacer par le dernier élément
    int root = heap->array[0];
    heap->array[0] = heap->array[heap->size - 1];
    heap->size--;

    // Restaurer le tas
    min_heapify(heap, 0);

    return root;
}

// Libérer la mémoire
void free_heap(MinHeap* heap) {
    if (heap) {
        free(heap->array);
        free(heap);
    }
}

MinHeap * heap;


struct fNode{

	int pid;
	struct fNode * next;

};

struct MyFile {

	struct fNode * head;
	struct fNode * tail;

};

struct MyFile myfifo;

int fEmpty(){
	return myfifo.head == NULL;
}

void fEnqueue(int pid){
	struct fNode * n = malloc(sizeof(struct fNode));
	n->next = NULL;
	n->pid = pid;
	if (fEmpty()){
		myfifo.head = n;
	}else{
		myfifo.tail->next = n;
	}
	myfifo.tail = n;
}

int fDequeue(){
	if (!fEmpty()){
		struct fNode * n = myfifo.head;
		myfifo.head = n->next;
		int r = n->pid;
		free(n);
		return r;
	}else{
		return -1;
	}
}

// 0 = absent, 1 = déjà dans la file
int in_queue[100000] = {0};

void Enqueue(const struct task_event * e){
    int pid = e->pid;
    int target = (e->prio == 1) ? 1 : 2;

    if (in_queue[pid] == target) return; // déjà dans le bon arbre

    if (in_queue[pid] != 0 && in_queue[pid] != target){
        // il est dans le mauvais arbre → on le retire et on le réinsère
        RBTree * wrong_tree = (in_queue[pid] == 1) ? rbtree : waitingTree;
        delete_node(wrong_tree, pid);
    }

    in_queue[pid] = target;
    insertRB((target == 1) ? rbtree : waitingTree, pid);
}

// void Enqueue(const struct task_event * e){
//     int pid = e->pid;
//     int target = (e->prio == 1) ? 1 : 2;
//     if (in_queue[pid] == target) return;

// 	switch (MODESCHED){
// 		case 0:
//             in_queue[pid] = target;
// 			fEnqueue(pid);
// 			break;
// 		case 1:
//             in_queue[pid] = target;
// 			insertHeap(heap, pid);
// 			break;
// 		case 2:
//             if (in_queue[pid] != 0 && in_queue[pid] != target){
//                 // il est dans le mauvais arbre → on le retire et on le réinsère
//                 RBTree * wrong_tree = (in_queue[pid] == 1) ? rbtree : waitingTree;
//                 delete_node(wrong_tree, pid);
//             }
//             in_queue[pid] = target;
// 			insertRB(target == 1 ? rbtree : waitingTree, pid);
// 			break;
// 		default:
// 			break;
// 	}	
// }

int Dequeue(){
    int r;
	switch (MODESCHED){
		case 0:
			r = fDequeue();
            break;
		case 1:
			r = extract_min(heap);
			break;
		case 2:
			if (Empty()) {
                // l'arbre principale est vide
                // donc plus de pid prioritaire on echange les arbres
                if (waitingTree->root != waitingTree->TNULL){
                    switch_tree();
                }else{
                    return -1;
                }
			}
			Node * n = NULL;
			n = minimum(rbtree, rbtree->root);
			r = n->data;
			delete_node(rbtree, r);
			break;
		default:
			break;
	}	
    if (r > 0 && r <= 100000){
        in_queue[r] = 0;
    }
    return r;
}

int Empty(){

    switch (MODESCHED){
		case 0:
			return fEmpty();
		case 1:
			return heap->size == 0;
		case 2:
			return rbtree->root == rbtree->TNULL;
		default:
			break;
	}	
}


int pid_max = 0;
int pid_min = 0;

static int handle_event(void *ctx, void *data, size_t data_sz){
	const struct task_event *e = data;
    fprintf(stderr, "[EVENT] PID %d comm=%s prio=%d\n", e->pid, e->comm, e->prio);
	// printf("[NOYAU -> USER] Nouvelle tâche interceptée ! PID: %d, Nom: %s\n", 
    //        e->pid, e->comm);
	
	Enqueue(e);

    if (pid_min == 0) pid_min = e->pid;

    if (e->pid > pid_max){
        pid_max = e->pid;
    }else if (e->pid < pid_min){
        pid_min = e->pid;
    }
	
	return 0;
}


const char help_fmt[] =
"My simple sched_ext scheduler.\n"
"\n"
"See the top-level comment in .bpf.c for more details.\n"
"\n"
"Usage: %s [-f] [-v]\n"
"\n"
"  -f            Use FIFO scheduling instead of weighted vtime scheduling\n"
"  -v            Print libbpf debug messages\n"
"  -h            Display this help and exit\n";

static bool verbose;
static volatile int exit_req;

static int libbpf_print_fn(enum libbpf_print_level level, const char *format, va_list args)
{
	if (level == LIBBPF_DEBUG && !verbose)
		return 0;
	return vfprintf(stderr, format, args);
}

static void sigint_handler(int mine)
{
	exit_req = 1;
}

// static void read_stats(struct scx_mine *skel, __u64 *stats)
// {
// 	int nr_cpus = libbpf_num_possible_cpus();
// 	assert(nr_cpus > 0);
// 	__u64 cnts[2][nr_cpus];
// 	__u32 idx;

// 	memset(stats, 0, sizeof(stats[0]) * 2);

// 	for (idx = 0; idx < 2; idx++) {
// 		int ret, cpu;

// 		ret = bpf_map_lookup_elem(bpf_map__fd(skel->maps.stats),
// 					  &idx, cnts[idx]);
// 		if (ret < 0)
// 			continue;
// 		for (cpu = 0; cpu < nr_cpus; cpu++)
// 			stats[idx] += cnts[idx][cpu];
// 	}
// }



int main(int argc, char **argv)
{
	struct scx_mine *skel;
	struct bpf_link *link;
	__u32 opt;
	__u64 ecode;

	myfifo.head = NULL;
	myfifo.tail = NULL;

	switch (MODESCHED){
		case 1:
			heap = create_min_heap(10000);
			break;
		case 2:
			rbtree = create_tree();
            waitingTree = create_tree();
			break;
		default:
			break;
	}


	
	
	libbpf_set_print(libbpf_print_fn);
	signal(SIGINT, sigint_handler);
	signal(SIGTERM, sigint_handler);
	restart:
	skel = SCX_OPS_OPEN(mine_ops, scx_mine);




	while ((opt = getopt(argc, argv, "fvh")) != -1) {
		switch (opt) {
		case 'f':
			skel->rodata->fifo_sched = true;
			break;
		case 'v':
			verbose = true;
			break;
		default:
			fprintf(stderr, help_fmt, basename(argv[0]));
			return opt != 'h';
		}
	}

	

	SCX_OPS_LOAD(skel, mine_ops, scx_mine, uei); 

	struct ring_buffer *rb = NULL;
	rb = ring_buffer__new(bpf_map__fd(skel->maps.rb), handle_event, NULL, NULL);

	int decision_map_fd = bpf_map__fd(skel->maps.decision_map);

	int idle_map_fd = bpf_map__fd(skel->maps.idle_map);

    int sum_latence_map_fd;
    sum_latence_map_fd =  bpf_map__fd(skel->maps.sum_latence_map);



	link = SCX_OPS_ATTACH(skel, mine_ops, scx_mine);

	while (!exit_req && !UEI_EXITED(skel, uei)) {
    
    int timeout = Empty() ? 100 : 0;
    ring_buffer__poll(rb, timeout);

    u32 key = 0;
    s32 value = 0;
    static s32 last_stuck_pid = 0;
    static int stuck_count = 0;
    
    bpf_map_lookup_elem(decision_map_fd, &key, &value);

    // Détection de blocage : même PID depuis trop longtemps
    if (value != 0) {
        if (value == last_stuck_pid) {
            stuck_count++;
            if (stuck_count > 1000) {
                fprintf(stderr, "[USER] STUCK: PID %d bloqué %d fois, force reset\n", value, stuck_count);
                // Re-enqueuer ce PID pour ne pas le perdre
                struct task_event fake = { .pid = value, .prio = 1 };
                Enqueue(&fake);
                // Forcer le reset
                s32 zero = 0;
                bpf_map_update_elem(decision_map_fd, &key, &zero, BPF_ANY);
                stuck_count = 0;
                last_stuck_pid = 0;
            }
        } else {
            last_stuck_pid = value;
            stuck_count = 1;
        }
    }
    
    if (value == 0) {
        int best_pid = Dequeue();
        
        if (best_pid > 0) {
            bpf_map_update_elem(decision_map_fd, &key, &best_pid, BPF_ANY);
            fprintf(stderr, "[USER] decision_map <- PID %d\n", best_pid);

            int kicked = 0;
            for (int i = 0; i < 64; ++i) {
                unsigned char idle;
                bpf_map_lookup_elem(idle_map_fd, &i, &idle);
                if (idle == 1) {
                    fprintf(stderr, "[USER] kick CPU %d pour PID %d\n", i, best_pid);
                    __u64 arg[1] = {i};
                    int prog_fd = bpf_program__fd(skel->progs.ring_doorbell);
                    LIBBPF_OPTS(bpf_test_run_opts, opts,
                        .ctx_in = arg, .ctx_size_in = sizeof(arg));
                    bpf_prog_test_run_opts(prog_fd, &opts);
                    kicked = 1;
                    break;
                }
            }
            if (!kicked) {
                fprintf(stderr, "[USER] WARN: PID %d dans decision_map mais aucun CPU idle !\n", best_pid);
            }
        } else if (best_pid == -1) {
            fprintf(stderr, "[USER] files vides (rbtree et waitingTree)\n");
        }
    } else {
        fprintf(stderr, "[USER] decision_map occupé avec PID %d, on attend\n", value);
    }
}

    // printf("Calcul des dernieres latences\n");
    // for (__s32 i = pid_min; i<= pid_max; ++i){
    //     __u64 arg[1] = {i};
    //     int prog_fd = bpf_program__fd(skel->progs.calcul_latence_pid);
    //     LIBBPF_OPTS(bpf_test_run_opts, opts, .ctx_in = arg, .ctx_size_in = sizeof(arg));
    //     bpf_prog_test_run_opts(prog_fd, &opts); // Déclenche le kick_cpu côté noyau
    // }


    printf("Résultats latence par pid:\n");
    unsigned long long sum = 0;
    for (__s32 i = pid_min; i<= pid_max; ++i){
        unsigned long long sum_lat = 0;
        bpf_map_lookup_elem(sum_latence_map_fd, &i, &sum_lat);
        if (sum_lat != 0){
            printf("[%d] -> %llu\n", i, sum_lat);
        }else{
            printf("[%d] -> INF\n", i);
        }
        sum += sum_lat;
    }

    printf("Latence totale: %llu\n", sum);
    printf("min:%d,  max:%d\n", pid_min, pid_max);



	switch (MODESCHED){
		case 1:
			free_heap(heap);
			break;
		case 2:
			free_tree(rbtree);
            free_tree(waitingTree);
			break;
		default:
			break;
	}


	ring_buffer__free(rb);

	bpf_link__destroy(link);
	ecode = UEI_REPORT(skel, uei);
	scx_mine__destroy(skel);


	if (UEI_ECODE_RESTART(ecode))
		goto restart;
	return 0;
}
