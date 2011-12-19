#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "box.h"
#ifdef THREADS
#include "thread_db.h"
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
