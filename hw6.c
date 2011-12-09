#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#define WORLD_ID '.'
/*
#include "box.h"
#ifdef THREADS
#include "threads.h"
#endif
*/

#ifdef THREADS
/* These options control the thread pool */
#define MAX_THREADS 4
/* Use this to restrict workers to spawn above a given depth */
#define MAX_BRANCH_LEVEL 4
#include <pthread.h>
void *
packer(void *);
#endif /* THREADS */

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
struct box_t *
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
void
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
void
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
#endif

/* Thread Definitions ********************************************************/

#ifdef THREADS
struct
thread_data_t
{
	pthread_t tid;
	struct box_t * box;
	struct box_list_t * list;
	size_t depth;
};

struct
thread_list_t
{
	struct thread_data_t * head;
	struct thread_list_t * tail;
};

struct
thread_db_t
{
	struct thread_list_t * list;
	pthread_mutex_t list_mutex;
	pthread_mutex_t result_mutex;
	size_t thread_count, thread_limit;
} thread_db;
#endif

/* Thread Manipulations ******************************************************/

#ifdef THREADS
/*! \brief Initialize a database to keep track of threads
 *
 *  \param db            The database of threads to initialize
 *  \param thread_limit  The maximal number of threads to allow in the pool
 */
void
prepare_thread_db(struct thread_db_t * db, size_t thread_limit)
{
	/* Invariant: The database should be valid */
	assert(db);

	/* Produce a dummy head to represent this thread */
	db->list = malloc(sizeof(struct thread_list_t));
	db->list->head = (struct thread_data_t *)(-1);
	db->list->tail = NULL;

	/* Initialize semaphores */
	pthread_mutex_init(&db->list_mutex, NULL);
	pthread_mutex_init(&db->result_mutex, NULL);

	/* Initialize numeric values */
	db->thread_count = 0;
	db->thread_limit = thread_limit;
}

typedef void (*thread_db_error_handler)(pthread_t);

/*! \brief Attempt to join every thread in a database
 *
 *  \param db        The database of threads to finalize
 *  \param callback  A method of handling and resolving failures
 */
void
finalize_thread_db(struct thread_db_t * db, thread_db_error_handler callback)
{
	struct thread_list_t * current, * next;
	/* Invariant: The database and its list should be valid */
	assert(db && db->list);

	/* Make sure to catch every thread, TODO are we? */
	current = db->list->tail;
	while (current) {
		next = current->tail;
		/* Try collecting each thread in turn */
		if (pthread_join(current->head->tid, NULL)) {
			#ifdef VERBOSE
			fprintf(stderr, "ERROR: unable to collect worker\n");
			#endif
			if (callback) {
				callback(current->head->tid);
			}
		}
		/* Clean up thread memory, TODO check for leak? */
		destroy(current->head->box);
		free(current->head);
		free(current);
		current = next;
	}

	/* Clean up the database */
	free(db->list);
	pthread_mutex_destroy(&db->list_mutex);
	pthread_mutex_destroy(&db->result_mutex);
	db->thread_count = db->thread_limit = 0;
}

/*! \brief Attempt to add a given thread to a given database
 *
 *  \param db           The database of threads
 *  \param thread_data  The work unit for the new thread
 *  \return             1 on success, 0 on failure
 */
int
add_thread(struct thread_db_t * db, struct thread_data_t * thread_data)
{
	int status;
	struct thread_list_t * next;
	/* Invariant: The database, its list, and the work unit should be valid */
	assert(db && thread_data);

	status = 0;
	/* Prevent other threads from being added simultaneously */
	pthread_mutex_lock(&db->list_mutex);
	if (db->thread_count < db->thread_limit) {
		/* Prepare a node for insertion */
		next = malloc(sizeof(struct thread_list_t));
		next->head = thread_data;
		next->tail = db->list->tail;
		/* Try to kicking off the worker and respond accordingly */
		if (pthread_create(&thread_data->tid, NULL, &packer, thread_data)) {
	  		destroy(thread_data->box);
			free(thread_data);
			free(next);
		} else {
			db->list->tail = next;
			++db->thread_count;
			status = 1;
		}
	}
	pthread_mutex_unlock(&db->list_mutex);

	return status;
}
#endif

/* Packing Functions *********************************************************/

/*! \brief Checks if a piece fits into the box at a given position
 *
 *  \param box    The box to check
 *  \param piece  The piece to check
 *  \param x      The horizontal position at which to begin
 *  \param y      The vertical position at which to begin
 *  \return       1 on success, 0 on failure
 */
inline int
fits(struct box_t * box, struct box_t * piece, box_size_t x, box_size_t y)
{
	box_size_t i, j, h, w;
	/* Invariant: the box, its data, and the piece should be valid */
	assert(box && box->data && piece);

	/* Find and check the limits of our traversal, fail fast if necessary */
	h = x + piece->height;
	w = y + piece->width;
	if (box->height < h || box->width < w) {
		return 0;
	}
	/* Otherwise, check that every required position is free */
	for (i = x; i < h; ++i) {
		for (j = y; j < w; ++j) {
			if (box->data[i][j] != WORLD_ID) {
				return 0;
			}
		}
	}
	return 1;
}

/*! \brief Perform a destructive assignment of the specified positions
 *
 *  \param box    The box to manipulate
 *  \param value  The literal data value to assign
 *  \param x      The horizontal position at which to begin
 *  \param y      The vertical position at which to begin
 *  \param h      The number of positions to fill horizontally
 *  \param w      The number of positions to fill vertically
 */
inline void
fill(struct box_t * box, box_data_t value,
	box_size_t x, box_size_t y,
	box_size_t h, box_size_t w)
{
	box_size_t i, j;
	/* Invariant: The box should be valid, properly sized, and have data */
	assert(box && x + h <= box->height && y + w <= box->width && box->data);

	/* Set the limit of our traversal to (h,w) */
	h += x;
	w += y;
	/* Starting at (x,y), fill box space with piece value of size (h,w) */
	for (i = x; i < h; ++i) {
		for (j = y; j < w; ++j) {
			box->data[i][j] = value;
		}
	}
}

/*! \brief Try packing a series of pieces into a given box
 *
 *  Packing follows the following scheme:
 *  0) If we have exhausted the piece list, return
 *  1) Check if this piece fits at any position in the box
 *  2) If a potential fit is found, explore this branch
 *     (this may spawn a worker thread, if appropriate)
 *  3) Reset the box to it's prior state
 *
 *  \param box    The box to manipulate
 *  \param list   A list node indicating the first piece
 *                (may be NULL, or possibly empty)
 *  \param depth  The reported depth of the search tree
 *                (this is used by the thread model)
 */
void
pack(struct box_t * box, struct box_list_t * list, size_t depth)
{
	box_size_t i, j;
	struct box_t * piece;
	#ifdef THREADS
	struct thread_data_t * worker_data;
	#endif
	/* Invariant: The box should be valid */
	assert(box);

	/* Base case: we are out of pieces (i.e. at a result) */
	if (!list || !list->head) {
		#ifdef THREADS
		/* Guarrantee mutual exclusion */
		pthread_mutex_lock(&thread_db.result_mutex);
		#endif
		/* Add the current state to the record of results */
		add_box(&box_db, box);
		#ifdef VERBOSE
		fprintf(stderr, "INFO: found solution %lu at depth %lu\n",
				(unsigned long)box_db.num_elements,
				(unsigned long)depth);
		#endif
		#ifdef THREADS
		pthread_mutex_unlock(&thread_db.result_mutex);
		#endif
		return;
	}

	/* Otherwise, snag the first piece */
	piece = list->head;

	/* We need to try this piece at every position */
	for (i = 0; i < box->height; ++i) {
		for (j = 0; j < box->width; ++j) {
			#ifdef ROTATIONS
			/* TODO Oh jesus, forgive me... */
			check_fits:
			#endif
			if (fits(box, piece, i, j)) {
				/* If the piece fits here, we must recur */
        			#ifdef THREADS
				if (depth < MAX_BRANCH_LEVEL) {
					/* Setup the thread node (TLM) */
					worker_data = malloc(sizeof(struct thread_data_t));
					worker_data->box   = copy_data(box);
					worker_data->list  = list->tail;
					worker_data->depth = depth;
					fill(worker_data->box, piece->id, i, j, piece->height, piece->width);
					/* Spawn the worker and proceed, or run cleanup */
					if (add_thread(&thread_db, worker_data)) { continue; }
					destroy(worker_data->box);
					free(worker_data);
				}
				#endif
				/* Proceed with this packing, dive, and then reset the state */
				fill(box, piece->id, i, j, piece->height, piece->width);
				pack(box, list->tail, depth + 1);
				fill(box, WORLD_ID, i, j, piece->height, piece->width);
			#ifdef ROTATIONS
			/* TODO nix all of this, it's awful */
			} else if (box->id == WORLD_ID) {
				/* Ignore square pieces, whose rotations are identical */
				if (piece->height != piece->width) {
					box->id = 'R';
					rotate(piece);
					goto check_fits;
				}
			} else {
				box->id = WORLD_ID;
				rotate(piece);
			#endif
			}
		}
	}
}

/* Utility Functions *********************************************************/

#ifdef THREADS
/*! \brief Provide a simple handle to direct a worker thread's execution
 *
 *  \param package  The data provided that describes this work unit
 *  \return         NULL
 */
void *
packer(void * package)
{
	struct thread_data_t * data = (struct thread_data_t *)package;
	#ifdef VERBOSE
	fprintf(stderr, "[thread %lu] worker started\n", (long unsigned)data->tid);
	#endif
	/* Just perform a packing based on the work unit */
	pack(data->box, data->list, data->depth + 1);
	#ifdef VERBOSE
	fprintf(stderr, "[thread %lu] worker finished\n", (long unsigned)data->tid);
	#endif
	return NULL;
}
#endif

#ifdef SORT
/*! \brief A comparator for determining the "decreasing" order of boxes
 *
 *  \param a  The first box
 *  \param b  The second box
 *  \return   1 if a's max dimension is less than b's, or 0 otherwise
 */
static int
large_to_small(const void * a, const void * b)
{
	box_size_t la, lb;
	struct box_t * pa, * pb;
	pa = *((struct box_t **)a);
	pb = *((struct box_t **)b);
	la = (pa->height < pa->width) ? pa->width : pa->height;
	lb = (pb->height < pb->width) ? pb->width : pb->height;
	return la < lb;
}
#endif

/*! \brief Read in a packing puzzle and produce all solutions
 *
 *  \return  EXIT_SUCCESS
 */
int
main(void)
{
	/* Declarations of problem variables */
	size_t i, pc;
	box_data_t pid;
	box_size_t wh, ww, ph, pw;
	struct box_t * world, ** pieces;
	struct box_list_t * list, * results;

	/* Read primary description (TODO remove scanf) */
	int sr = scanf("%lu %lu\n%lu\n", &ww, &wh, &pc);
        assert(sr == 3);
	/* Setup the world */
	world = create_with_data(wh, ww);
	fill(world, WORLD_ID, 0, 0, wh, ww);
	/* Setup the pieces (TODO remove scanf) */
	pieces = malloc(sizeof(struct box_t *) * (pc + 1));
	for (i = 0; i < pc; ++i) {
		sr = scanf("%c %lu %lu\n", &pid, &pw, &ph);
		assert(pid != WORLD_ID && sr == 3);
		pieces[i] = malloc(sizeof(struct box_t));
		pieces[i]->id     = pid;
		pieces[i]->height = ph;
		pieces[i]->width  = pw;
		pieces[i]->data   = NULL;
	}
	pieces[i] = NULL;
	/* Construct a structed list of nodes for the pieces */
	list = malloc(sizeof(struct box_list_t) * (pc + 1));
	for (i = 0; i < pc; ++i) {
		list[i].head = pieces[i];
		list[i].tail = &list[i + 1];
	}
	list[i].head = NULL;
	list[i].tail = NULL;

	#ifdef SORT
	/* Optional optimization: order the pieces prior to packing */
	qsort(pieces, pc, sizeof(struct box_t *), &large_to_small);
	#endif

	#ifdef VERBOSE
	/* Dump a full description of the puzzle */
	fprintf(stderr, "INFO:\t%lux%lu BOARD w/ %lu PIECES\n",
			(long unsigned)world->height,
			(long unsigned)world->width,
			(long unsigned)pc);
	for (i = 0; i < pc; ++i) {
		fprintf(stderr, "\tPIECE '%c': %lux%lu\n",
				(char)pieces[i]->id,
				(long unsigned int)pieces[i]->height,
				(long unsigned int)pieces[i]->width);
	}
	#endif

	#ifdef THREADS
	/* Setup a database to monitor the workers */
	prepare_thread_db(&thread_db, MAX_THREADS);
	#endif
	/* Setup an area to track results, then begin packing */
	box_db.list.head = world;
	box_db.list.tail = NULL;
	box_db.num_elements = 0;
	pack(world, list, 0);
	#ifdef THREADS
	/* Wait for workers to finish, if necessary */
	finalize_thread_db(&thread_db, NULL);
	#endif

	/* Report the solution(s), if any */
	if (box_db.num_elements) {
		printf("%lu solution(s) found:\n", box_db.num_elements);
	} else {
		printf("No solutions found.\n");
	}
	results = box_db.list.tail;
	while (results) {
		box_db.list.tail = results->tail;
		/* Print each result */
		putchar('\n');
		print(results->head);
		/* Destroy the record */
		destroy(results->head);
		free(results);
		results = box_db.list.tail;
	}

	/* Final cleanup */
	destroy(world);
	for (i = 0; i < pc; ++i) {
		free(pieces[i]);
	}
	free(pieces);
	free(list);

	return EXIT_SUCCESS;
}
