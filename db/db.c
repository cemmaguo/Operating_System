#define _POSIX_C_SOURCE 200112L

#include "db.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Node_t head = {"", "", NULL, NULL};

static Node_t *search(const char *, Node_t *, Node_t **);

static Node_t *
Node_constructor(const char *arg_name, const char *arg_value, Node_t *arg_left,
    Node_t *arg_right) {
	Node_t *new_node = malloc(sizeof (Node_t));
	if (new_node == NULL)
		return (NULL);
	if ((new_node->name = malloc(strlen(arg_name) + 1)) == NULL) {
		free(new_node);
		return (NULL);
	}
	if ((new_node->value = malloc(strlen(arg_value) + 1)) == NULL) {
		free(new_node->name);
		free(new_node);
		return (NULL);
	}
	strcpy(new_node->name, arg_name);
	strcpy(new_node->value, arg_value);
	new_node->lchild = arg_left;
	new_node->rchild = arg_right;

	return (new_node);
}

static void
Node_destructor(Node_t *node)
{
	if (node->name != NULL)
		free(node->name);
	if (node->value != NULL)
		free(node->value);
	free(node);
}

static void
query(const char *name, char *result, size_t len)
{
	Node_t *target;

	target = search(name, &head, NULL);
	if (target == NULL) {
		strncpy(result, "not found", len-1);
		return;
	} else {
		strncpy(result, target->value, len-1);
		return;
	}
}

static int
add(const char *name, const char *value)
{
	Node_t *parent, *target, *newnode;

	if ((target = search(name, &head, &parent)) != NULL) {
		return (0);
	}

	newnode = Node_constructor(name, value, NULL, NULL);

	if (strcmp(name, parent->name) < 0)
		parent->lchild = newnode;
	else
		parent->rchild = newnode;

	return (1);
}

static int
xremove(const char *name)
{
	Node_t *parent, *dnode, *next;

	/* First, find the node to be removed. */
	if ((dnode = search(name, &head, &parent)) == NULL) {
		/* It's not there. */
		return (0);
	}

	/*
	 * We found it. Now check out the easy cases. If the node has no right
	 * child, then we can merely replace its parent's pointer to it with
	 * the node's left child.
	 */
	if (dnode->rchild == NULL) {
		if (strcmp(dnode->name, parent->name) < 0)
			parent->lchild = dnode->lchild;
		else
			parent->rchild = dnode->lchild;

		/* done with dnode */
		Node_destructor(dnode);
	} else if (dnode->lchild == NULL) {
		/* ditto if the node had no left child */
		if (strcmp(dnode->name, parent->name) < 0)
			parent->lchild = dnode->rchild;
		else
			parent->rchild = dnode->rchild;

		/* done with dnode */
		Node_destructor(dnode);
	} else {
		Node_t **pnext;

		/*
		 * So much for the easy cases ...
		 * We know that all nodes in a node's right subtree have
		 * lexicographically greater names than the node does, and all
		 * nodes in a node's left subtree have lexicographically
		 * smaller names than the node does. So, we find the
		 * lexicographically smallest node in the right subtree and
		 * replace the node to be deleted with that node. This new node
		 * thus is lexicographically smaller than all nodes in its
		 * right subtree, and greater than all nodes in its left
		 * subtree. Thus the modified tree is well formed.
		 */

		/*
		 * pnext is the address of the field within the parent of next
		 * that points to next.
		 */
		pnext = &dnode->rchild;

		next = dnode->rchild;
		while (next->lchild != NULL) {
			/*
			 * Work our way down the lchild chain, finding the
			 * smallest node in the subtree.
			 */
			Node_t *nextl = next->lchild;
			pnext = &next->lchild;
			next = nextl;
		}
		{
			char *new_name, *new_value;
			if ((new_name = malloc(strlen(next->name) + 1)) ==
			    NULL) {
				return (0);
			}
			if ((new_value = malloc(strlen(next->value) + 1)) ==
			    NULL) {
				free(new_name);
				return (0);
			}
			free(dnode->name);
			free(dnode->value);
			dnode->name = new_name;
			dnode->value = new_value;
			strcpy(dnode->name, next->name);
			strcpy(dnode->value, next->value);
		}
		*pnext = next->rchild;
		Node_destructor(next);
	}

	return (1);
}

/*
 * Search the tree, starting at parent, for a node containing name (the "target
 * node"). Return a pointer to the node, if found, otherwise return 0.  If
 * parentpp is not 0, then it points to a location at which the address of the
 * parent of the target node is stored. If the target node is not found, the
 * location pointed to by parentpp is set to what would be the the address of
 * the parent of the target node, if it were there.
 *
 * Assumptions:
 * parent is not NULL and it does not contain name
 */
static Node_t *
search(const char *name, Node_t *parent, Node_t **parentpp)
{
	Node_t *next, *result;

	if (strcmp(name, parent->name) < 0) {
		if ((next = parent->lchild) == NULL) {
			result = NULL;
		} else {
			if (strcmp(name, next->name) == 0) {
				result = next;
			} else {
				/* parent is no longer needed. */
				result = search(name, next, parentpp);
				return (result);
			}
		}
	} else {
		if ((next = parent->rchild) == NULL) {
			result = NULL;
		} else {
			if (strcmp(name, next->name) == 0) {
				result = next;
			} else {
				/* parent is no longer needed. */
				result = search(name, next, parentpp);
				return (result);
			}
		}
	}

	if (parentpp != NULL) {
		*parentpp = parent;
	}

	return (result);
}

void
interpret_command(const char *command, char *response, size_t len)
{
	char value[256];
	char ibuf[256];
	char name[256];

	if (strlen(command) <= 1) {
		strncpy(response, "ill-formed command", len-1);
		return;
	}

	/* Which command is it? */
	switch (command[0]) {
	case 'q':
		/* query */
		sscanf(&command[1], "%255s", name);
		if (strlen(name) == 0) {
			strncpy(response, "ill-formed command", len-1);
			return;
		}
		query(name, response, len);
		if (strlen(response) == 0) {
			strncpy(response, "not found", len-1);
			return;
		} else {
			return;
		}

		break;

	case 'a':
		/* add to the database */
		sscanf(&command[1], "%255s %255s", name, value);
		if (strlen(name) == 0 || strlen(value) == 0) {
			strncpy(response, "ill-formed command", len-1);
			return;
		}
		if (add(name, value) != 0) {
			strncpy(response, "added", len-1);
			return;
		} else {
			strncpy(response, "already in database", len-1);
			return;
		}

	case 'd':
		/* delete from the database */
		sscanf(&command[1], "%255s", name);
		if (strlen(name) == 0) {
			strncpy(response, "ill-formed command", len-1);
			return;
		}
		if (xremove(name) != 0) {
			strncpy(response, "removed", len-1);
			return;
		} else {
			strncpy(response, "not in database", len-1);
			return;
		}

	case 'f':
		/* process the commands in a file (silently) */
		sscanf(&command[1], "%255s", name);
		if (name[0] == '\0') {
			strncpy(response, "ill-formed command", len-1);
			return;
		}
		{
			FILE *finput = fopen(name, "r");
			if (finput == NULL) {
				strncpy(response, "bad file name", len-1);
				return;
			}
			while (fgets(ibuf, sizeof (ibuf), finput) != NULL) {
				interpret_command(ibuf, response, len);
			}
			fclose(finput);
		}
		strncpy(response, "file processed", len-1);
		return;

	default:
		strncpy(response, "ill-formed command", len-1);
		return;
	}
}

/*
 * Helper method for cleanup_db().
 */
void
recursive_cleanup_db(Node_t *node)
{
	if (!node)
		return;

	recursive_cleanup_db(node->lchild);
	recursive_cleanup_db(node->rchild);
	if (node != &head) {
		Node_destructor(node);
	}
}

/*
 * Cleans up the database tree by calling a recursive helper method
 * to destroy a node and each of its children.
 */
void
cleanup_db()
{
	recursive_cleanup_db(&head);
}
