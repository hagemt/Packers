#ifndef BOX_H
#define BOX_H

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define WORLD_ID '.'

/* Box Definitions ***********************************************************/

typedef size_t box_size_t;
typedef char box_data_t;

struct
box_t
{
	box_size_t height, width;
	box_data_t id, ** data;
};

struct
box_list_t
{
	struct box_t * head;
	struct box_list_t * tail;
};

struct
box_db_t {
	struct box_list_t list;
	box_size_t num_elements;
} box_db;

/* Box Manipulations *********************************************************/

/*! \brief Allocate a box with grid data of the given dimension
 *
 *  Note that the box's grid is left uninitialized. (see "fill")
 *  \param height  The desired number of rows
 *  \param width   The desired number of columns
 *  \return        A valid box that matches the request
 */
inline struct box_t *
create_with_data(box_size_t height, box_size_t width)
{
	box_size_t i;
	struct box_t * new_box;

	/* Produce a box of the desired height and width */
	new_box = malloc(sizeof(struct box_t));
	new_box->height = height;
	new_box->width = width;
	new_box->id = WORLD_ID;

	/* Include a grid of the requested size */
	new_box->data = malloc(sizeof(box_data_t *) * height);
	for (i = 0; i < height; ++i) {
		new_box->data[i] = malloc(sizeof(box_data_t) * width);
	}

	return new_box;
}

/*! \brief Allocate a box that is an exact copy of another
 *
 *  \param box  The box to copy
 *  \return     An exact copy of box
 */
struct box_t *
copy_data(struct box_t * box)
{
	box_size_t i, j, h, w;
	struct box_t * new_box;
	/* Invariant: The box to copy should be valid */
	assert(box);

	/* Replicate all data contained in box */
	h = box->height;
	w = box->width;
	new_box = create_with_data(h, w);
	for (i = 0; i < h; ++i) {
		for (j = 0; j < w; ++j) {
			new_box->data[i][j] = box->data[i][j];
		}
	}

	return new_box;
}

/*! \brief Free all memory associated with a given box
 *
 *  \param box  The box to destroy
 */
inline void
destroy(struct box_t * box)
{
	box_size_t i, h;
	/* Invariant: The box to destroy should be valid */
	assert(box);

	/* Clean up any grid data associated with the box */
	if (box->data) {
		h = box->height;
		for (i = 0; i < h; ++i) {
			free(box->data[i]);
		}
		free(box->data);
		box->data = NULL;
	}

	/* Free the memory of the box itself*/
	free(box);
}

/*! \brief Produce a textual representation of this box
 *
 *  This is used to print a box's grid data to stdout.
 *  \param box  The box to print
 */
inline void
print(struct box_t * box)
{
	box_size_t i, j, h, w;
	/* Invariant: The box to print should be valid */
	assert(box);

	/* Printing boxes without grid data is a no-op */
	if (box->data) {
		h = box->height;
		w = box->width;
		/* Otherwise, write each datum */
		for (i = 0; i < h; ++i) {
			for (j = 0; j < w; ++j) {
				putchar((char)box->data[i][j]);
			}
			putchar('\n');
		}
	}
}

/*! \brief Insert a given box into a given database
 *
 *  \param db   The box database to use
 *  \param box  The box to add
 */
inline void
add_box(struct box_db_t * db, struct box_t * box)
{
	struct box_list_t * node;
	/* Invariant: The box and database should be valid and prepared */
	assert(box && db && db->list.head);

	/* Attach the new node second in the list */
	node = malloc(sizeof(struct box_list_t));
	node->head = copy_data(box);
	node->tail = db->list.tail;
	db->list.tail = node;
	++db->num_elements;
}

#ifdef ROTATIONS
/*! \brief Switches a given box's height and width
 *
 *  Any even-length sequence of successive rotations produces no results.
 *  \param box  The box to manipulate
 */
inline void
rotate(struct box_t * box)
{
	/* TODO make this thread-safe */
	box_size_t s;
	/* Invariant: The box should be valid and devoid of data */
	assert(box && !box->data);

	/* Toggle the box's height and width */
	s = box->height;
	box->height = box->width;
	box->width = s;
}
#endif /* ROTATIONS */

#endif /* BOX_H */
