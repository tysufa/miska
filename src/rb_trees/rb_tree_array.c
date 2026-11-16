#include <stdbool.h>
#include <stdio.h>

#define MAX_NODES 100
#define EMPTY -1

typedef enum { RED, BLACK } Color;

typedef struct {
  int key;
  int left;
  int right;
  int parent;
  Color color;    // For later Red-Black implementation
  bool is_active; // To track which array slots are used
} Node;

Node tree[MAX_NODES];
int root = EMPTY;
int next_free = 0; // Simple counter for allocation

void inorder(int node_idx, int level) {
  if (node_idx == EMPTY)
    return;

  inorder(tree[node_idx].right, level + 1);
  for (int i = 0; i < level; i++)
    printf("    ");

  printf("%d\n", tree[node_idx].key);
  inorder(tree[node_idx].left, level + 1);
}

void left_rotate(int x) {
  int y = tree[x].right;

  // Turn y's left subtree into x's right subtree
  tree[x].right = tree[y].left;
  if (tree[y].left != EMPTY) {
    tree[tree[y].left].parent = x;
  }

  // Link x's parent to y
  tree[y].parent = tree[x].parent;
  if (tree[x].parent == EMPTY) {
    root = y;
  } else if (x == tree[tree[x].parent].left) {
    tree[tree[x].parent].left = y;
  } else {
    tree[tree[x].parent].right = y;
  }

  // Put x on y's left
  tree[y].left = x;
  tree[x].parent = y;
}

void right_rotate(int y) {
  int x = tree[y].left;

  // Turn x's right subtree into y's left subtree
  tree[y].left = tree[x].right;
  if (tree[x].right != EMPTY) {
    tree[tree[x].right].parent = y;
  }

  // Link y's parent to x
  tree[x].parent = tree[y].parent;
  if (tree[y].parent == EMPTY) {
    root = x;
  } else if (y == tree[tree[y].parent].left) {
    tree[tree[y].parent].left = x;
  } else {
    tree[tree[y].parent].right = x;
  }

  // Put y on x's right
  tree[x].right = y;
  tree[y].parent = x;
}

void init_tree() {
  for (int i = 0; i < MAX_NODES; i++) {
    tree[i].is_active = false;
    tree[i].left = tree[i].right = tree[i].parent = EMPTY;
  }
}

int create_node(int key) {
  if (next_free >= MAX_NODES) {
    printf("Error: Out of memory\n");
    return EMPTY;
  }
  int idx = next_free++;
  tree[idx].key = key;
  tree[idx].left = tree[idx].right = tree[idx].parent = EMPTY;
  tree[idx].color = RED; // Standard for RB trees
  tree[idx].is_active = true;
  return idx;
}

void insert_fixup(int z) {
  // While parent is RED, we have a violation
  while (z != root && tree[tree[z].parent].color == RED) {
    int p = tree[z].parent;
    int g = tree[p].parent;

    // Parent is a left child
    if (p == tree[g].left) {
      int u = tree[g].right; // The Uncle

      if (u != EMPTY && tree[u].color == RED) {
        // Case 1: Uncle is Red
        tree[p].color = BLACK;
        tree[u].color = BLACK;
        tree[g].color = RED;
        z = g; // Move up to grandparent and repeat
      } else {
        // Case 2: Z is a right child (Triangle)
        if (z == tree[p].right) {
          z = p;
          left_rotate(z);
          p = tree[z].parent; // Update p after rotation
          g = tree[p].parent;
        }
        // Case 3: Z is a left child (Line)
        tree[p].color = BLACK;
        tree[g].color = RED;
        right_rotate(g);
      }
    }
    // Parent is a right child (Mirror of above)
    else {
      int u = tree[g].left; // The Uncle

      if (u != EMPTY && tree[u].color == RED) {
        // Case 1: Uncle is Red
        tree[p].color = BLACK;
        tree[u].color = BLACK;
        tree[g].color = RED;
        z = g;
      } else {
        // Case 2: Z is a left child
        if (z == tree[p].left) {
          z = p;
          right_rotate(z);
          p = tree[z].parent;
          g = tree[p].parent;
        }
        // Case 3: Z is a right child
        tree[p].color = BLACK;
        tree[g].color = RED;
        left_rotate(g);
      }
    }
  }
  // Property 2: The root must always be BLACK
  tree[root].color = BLACK;
}

void insert(int key) {
  int new_node = create_node(key);
  if (new_node == EMPTY)
    return;

  if (root == EMPTY) {
    root = new_node;
    return;
  }

  int current = root;
  int parent = EMPTY;

  while (current != EMPTY) {
    parent = current;
    if (key < tree[current].key)
      current = tree[current].left;
    else
      current = tree[current].right;
  }

  tree[new_node].parent = parent;
  if (key < tree[parent].key)
    tree[parent].left = new_node;
  else
    tree[parent].right = new_node;

  insert_fixup(new_node);
}

int search(int key) {
  int current = root;

  while (current != EMPTY) {
    if (key == tree[current].key) {
      return current; // Found it!
    }

    if (key < tree[current].key) {
      current = tree[current].left;
    } else {
      current = tree[current].right;
    }
  }

  return EMPTY; // Key not in tree
}

void rb_transplant(int u, int v) {
  if (tree[u].parent == EMPTY) {
    root = v;
  } else if (u == tree[tree[u].parent].left) {
    tree[tree[u].parent].left = v;
  } else {
    tree[tree[u].parent].right = v;
  }

  if (v != EMPTY) {
    tree[v].parent = tree[u].parent;
  }
}

int minimum(int x) {
  while (tree[x].left != EMPTY) {
    x = tree[x].left;
  }
  return x;
}

void delete_fixup(int x) {
  while (x != root && (x == EMPTY || tree[x].color == BLACK)) {
    if (x == tree[tree[x].parent].left ||
        (x == EMPTY && tree[root].left == EMPTY)) {
      // Standardizing logic: we need to find the sibling 's'
      int p = (x == EMPTY) ? find_parent_of_empty_slot() : tree[x].parent;
      int s = tree[p].right;

      // Case 1: Sibling is Red
      if (s != EMPTY && tree[s].color == RED) {
        tree[s].color = BLACK;
        tree[p].color = RED;
        left_rotate(p);
        s = tree[p].right;
      }

      // Case 2: Sibling is Black, and both its children are Black
      if ((tree[s].left == EMPTY || tree[tree[s].left].color == BLACK) &&
          (tree[s].right == EMPTY || tree[tree[s].right].color == BLACK)) {
        tree[s].color = RED;
        x = p;
      } else {
        // Case 3: Sibling is Black, s.left is Red, s.right is Black
        if (tree[s].right == EMPTY || tree[tree[s].right].color == BLACK) {
          tree[tree[s].left].color = BLACK;
          tree[s].color = RED;
          right_rotate(s);
          s = tree[p].right;
        }
        // Case 4: Sibling is Black, s.right is Red
        tree[s].color = tree[p].color;
        tree[p].color = BLACK;
        tree[tree[s].right].color = BLACK;
        left_rotate(p);
        x = root;
      }
    } else {
      // (Mirror image for when x is the right child)
      // ... Similar logic with right/left swapped ...
    }
  }
  if (x != EMPTY)
    tree[x].color = BLACK;
}

void rb_delete(int z_idx) {
  int y = z_idx;
  int y_original_color = tree[y].color;
  int x; // The node that moves into y's old position

  if (tree[z_idx].left == EMPTY) {
    x = tree[z_idx].right;
    rb_transplant(z_idx, tree[z_idx].right);
  } else if (tree[z_idx].right == EMPTY) {
    x = tree[z_idx].left;
    rb_transplant(z_idx, tree[z_idx].left);
  } else {
    y = minimum(tree[z_idx].right); // Successor
    y_original_color = tree[y].color;
    x = tree[y].right;

    if (tree[y].parent == z_idx) {
      if (x != EMPTY)
        tree[x].parent = y;
    } else {
      rb_transplant(y, tree[y].right);
      tree[y].right = tree[z_idx].right;
      tree[tree[y].right].parent = y;
    }
    rb_transplant(z_idx, y);
    tree[y].left = tree[z_idx].left;
    tree[tree[y].left].parent = y;
    tree[y].color = tree[z_idx].color;
  }

  // Only fix if we removed a BLACK node
  if (y_original_color == BLACK) {
    delete_fixup(x);
  }

  deallocate_node(z_idx); // Return the index to our free list!
}

int main(int argc, char *argv[]) {
  insert(10);
  insert(11);
  insert(7);
  insert(5);
  insert(9);
  insert(3);
  insert(1);
  insert(6);
  inorder(root, 0);

  return 0;
}
