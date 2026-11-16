#include <stdio.h>
#include <stdlib.h>

struct Node {
  struct Node *parent;
  int value;
  int color;
  struct Node *child_left;
  struct Node *child_right;
};

void insertion(int val, struct Node *node) {}

void affichage_arbre(struct Node *a, int level) {
  if (a == NULL)
    return;

  if (a->child_right) {
    affichage_arbre(a->child_right, level + 1);
  }
  for (int i = 0; i < level; i++)
    printf("    ");
  printf("%d:%d\n", a->color, a->value);
  if (a->child_left) {
    affichage_arbre(a->child_left, level + 1);
  }
}

struct Node *insert_value(struct Node *root, struct Node *node) {
  if (root == NULL) {
    return node;
  } else if (root->value < node->value) {
    root->child_right = insert_value(root->child_right, node);
    root->child_right->parent = root;
  } else if (root->value >= node->value) {
    root->child_left = insert_value(root->child_left, node);
    root->child_left->parent = root;
  }

  return root;
}

struct Node *rotate_right(struct Node *node) {
  if (node) {
    struct Node *left = node->child_left;
    node->child_left = left->child_right;

    if (node->child_left)
      node->child_left->parent = node;

    left->parent = node->parent;

    left->child_right = node;
    node->parent = left;
    return left;
  }
  return NULL;
}

struct Node *rotate_left(struct Node *node) {
  if (node) {
    struct Node *right = node->child_right;
    node->child_right = right->child_left;

    if (node->child_right)
      node->child_right->parent = node;

    right->parent = node->parent;

    right->child_left = node;
    node->parent = right;
    return right;
  }
  return NULL;
}

void fix_after_ins(struct Node *node) {
  if (node->parent == NULL) {
    node->color = 0;
    return;
  }

  struct Node *parent;
  struct Node *gp;
  struct Node *oncle;

  do {
    parent = node->parent;

    if (parent->color == 0)
      return;

    gp = node->parent->parent;
    if (!gp) {
      parent->color = 0;
    }

    if (gp->child_left == parent) {
      oncle = gp->child_right;

      // uncle and parent of the same color, only recoloring needed
      if (oncle && oncle->color == 1) {
        gp->color = 1;
        oncle->color = 0;
        parent->color = 0;
        node = gp;
      } else {
        if (node == node->parent->child_right) {
          node->parent = rotate_left(node->parent);
          node = parent;
          parent = node->parent;
        } else if (node == node->parent->child_left) {
          node->parent = rotate_right(node->parent);
          node = parent;
          parent = node->parent;
        }
      }

    } else {
      oncle = gp->child_left;

      // uncle and parent of the same color, only recoloring needed
      if (oncle && oncle->color == 1) {
        gp->color = 1;
        oncle->color = 0;
        parent->color = 0;
        node = gp;
      } else {
        if (node == node->parent->child_right) {
          node->parent = rotate_left(node->parent);
          node = parent;
          parent = node->parent;
        } else if (node == node->parent->child_left) {
          node->parent = rotate_right(node->parent);
          node = parent;
          parent = node->parent;
        }
      }
    }

  } while ((node = node->parent));
}

struct Node *create_node(int value) {
  struct Node *temp = NULL;
  temp = malloc(sizeof(struct Node));
  temp->value = value;
  temp->color = 1;
  temp->child_left = NULL;
  temp->child_right = NULL;

  return temp;
}

struct Node *insert(struct Node *node, int value) {
  struct Node *temp = create_node(value);
  node = insert_value(node, temp);
  fix_after_ins(temp);
  return node;
}

int main(int argc, char **argv) {

  struct Node *racine = NULL;
  struct Node *temp = NULL;
  racine = insert(racine, 10);
  racine = insert(racine, 11);
  racine = insert(racine, 7);
  racine = insert(racine, 5);
  racine = insert(racine, 6);
  // temp = create_node(10);
  // racine = insert_value(racine, temp);
  // fix_after_ins(temp);
  // temp = create_node(11);
  // racine = insert_value(racine, temp);
  // fix_after_ins(temp);
  // temp = create_node(7);
  // racine = insert_value(racine, temp);
  // fix_after_ins(temp);
  // temp = create_node(5);
  // racine = insert_value(racine, temp);
  // fix_after_ins(temp);

  affichage_arbre(racine, 0);
  // printf("-------------\n");
  // fix_after_ins(temp);
  // affichage_arbre(racine, 0);
  //
  // printf("-------------\n");
  // temp = create_node(11);
  // racine = insert_value(racine, temp);
  // fix_after_ins(temp);
  // racine = insert_value(5, racine);
  // fix_after_ins(racine);
  // racine = insert_value(6, racine);
  // fix_after_ins(racine);
  // racine = insert_value(4, racine);

  printf("---------------------\n\n\n\n\n");
  return 0;
}
