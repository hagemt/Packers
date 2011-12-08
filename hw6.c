#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define WORLD_ID '.'

#ifdef THREADS
#define BRANCH_LEVEL 0
#include <pthread.h>
void *
packer(void *);
#endif /* THREADS */

/* Box Definitions */

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

#ifdef THREADS
struct
thread_data_t
{
	pthread_t tid;
	struct box_t * space;
	struct box_list_t * list;
	size_t * count;
};

struct
thread_list_t
{
	struct thread_data_t * head;
	struct thread_list_t * tail;
} thread_list;
#endif

/* Box Manipulations */

struct box_t *
create_with_data(box_size_t height, box_size_t width)
{
	box_size_t i;
	struct box_t * new_box;
	new_box = malloc(sizeof(struct box_t));
	new_box->height = height;
	new_box->width = width;
	new_box->id = WORLD_ID;
	new_box->data = malloc(sizeof(box_data_t *) * height);
	for (i = 0; i < height; ++i) {
		new_box->data[i] = malloc(sizeof(box_data_t) * width);
	}
	return new_box;
}

struct box_t *
copy_data(struct box_t * box)
{
	box_size_t i, j, h, w;
	struct box_t * new_box;
	assert(box);
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

void
destroy(struct box_t * box)
{
	box_size_t i, h;
	assert(box);
	if (box->data) {
		h = box->height;
		for (i = 0; i < h; ++i) {
			free(box->data[i]);
		}
		free(box->data);
		box->data = NULL;
	}
	free(box);
}

void
print(struct box_t * box)
{
	box_size_t i, j, h, w;
	assert(box);
	h = box->height;
	w = box->width;
	if (box->data) {
		for (i = 0; i < h; ++i) {
			for (j = 0; j < w; ++j) {
				putchar((char)box->data[i][j]);
			}
			putchar('\n');
		}
	} else {
		fprintf(stderr, "\tPIECE: %c %lu %lu\n",
				(char)box->id,
				(long unsigned int)h,
				(long unsigned int)w);
	}
}

#ifdef ROTATIONS
inline void
rotate(struct box_t * box)
{
	/* TODO doesn't work with threads or in general?
	 * TODO in general, it's a hack...
	 */
	box_size_t s;
	assert(box && !box->data);
	s = box->height;
	box->height = box->width;
	box->width = s;
}
#endif

/* Packing Functions */

int
fits(struct box_t * space, struct box_t * piece, box_size_t x, box_size_t y)
{
	box_size_t i, j, h, w;
	assert(space && piece);
	h = x + piece->height;
	w = y + piece->width;
	if (space->height < h || space->width < w) {
		return 0;
	}
	for (i = x; i < h; ++i) {
		for (j = y; j < w; ++j) {
			if (space->data[i][j] != WORLD_ID) {
				return 0;
			}
		}
	}
	return 1;
}

void
fill(struct box_t * box, box_data_t value,
	box_size_t x, box_size_t y,
	box_size_t h, box_size_t w)
{
	/* Starting at (x,y), fill box space with piece value of size (h,w) */
	box_size_t i, j;
	assert(box && box->data && x + h <= box->height && y + w <= box->width);
	h += x; w += y;
	for (i = x; i < h; ++i) {
		for (j = y; j < w; ++j) {
			box->data[i][j] = value;
		}
	}
}

void
pack(struct box_t * space, struct box_list_t * list, size_t * count, size_t depth)
{
	box_size_t i, j;
	struct box_t * new_space, * piece;
	#ifdef THREADS
	struct thread_list_t * current, * next;
	current = next = &thread_list;
	#endif
	assert(space && list && count);
	piece = list->head;
	if (!piece) {
		/* TODO problem with mutex on count? */
		++*count;
		#ifdef VERBOSE
		fprintf(stderr, "INFO: found solution %lu at depth %lu\n", *count, depth);
		#endif
		print(space);
		putchar('\n');
		return;
	}
	for (i = 0; i < space->height; ++i) {
		for (j = 0; j < space->width; ++j) {
			#ifdef ROTATIONS
			check_fits:
			#endif
			if (fits(space, piece, i, j)) {
				new_space = copy_data(space);
				fill(new_space, piece->id, i, j, piece->height, piece->width);
        			#ifdef THREADS
				if (depth == BRANCH_LEVEL) {
					/* Setup the thread node */
					next = malloc(sizeof(struct thread_list_t));
					next->head = malloc(sizeof(struct thread_data_t));
					next->head->space = new_space;
					next->head->list  = list->tail;
					next->head->count = count;
					next->tail = NULL;
					if (pthread_create(&next->head->tid, NULL, &packer, next->head)) {
						destroy(next->head->space);
						free(next->head);
						free(next);
					} else {
						current->tail = next;
						current = next;
          				}
				}
				#else
				pack(new_space, list->tail, count, depth + 1);
				destroy(new_space);
				#endif
			}
			#ifdef ROTATIONS
			if (space->id == WORLD_ID) {
				space->id = '\0';
				rotate(piece);
				goto check_fits;
			} else {
				space->id = WORLD_ID;
				rotate(piece);
			}
			#endif
		}
	}
	#ifdef THREADS
	if (depth == BRANCH_LEVEL) {
		current = thread_list.tail;
		thread_list.tail = NULL;
		while (current) {
			next = current->tail;
			assert(current->head);
			if (pthread_join(current->head->tid, NULL)) {
				fprintf(stderr, "ERROR: cannot collect worker\n");
			}
			destroy(current->head->space);
			free(current->head);
			free(current);
			current = next;
		}
	}
	#endif
}

/* Utility Functions */

#ifdef THREADS
void *
packer(void * package)
{
	struct thread_data_t * data = (struct thread_data_t *)package;
	#ifdef VERBOSE
	fprintf(stderr, "[thread %lu] worker started\n", (long unsigned)data->tid);
	#endif
	pack(data->space, data->list, data->count, BRANCH_LEVEL + 1);
	#ifdef VERBOSE
	fprintf(stderr, "[thread %lu] worker finished\n", (long unsigned)data->tid);
	#endif
	return NULL;
}
#endif

#ifdef SORT
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

int
main(void)
{
	/* Declarations */
	box_size_t wh, ww, ph, pw;
	size_t i, pc, sc; box_data_t pid;
	struct box_t * world, ** pieces;
	struct box_list_t * list;

	/* Read input (TODO remove scanf's) */
	int sr = scanf("%lu %lu\n%lu\n", &ww, &wh, &pc);
        assert(sr == 3);
	world = create_with_data(wh, ww);
	fill(world, WORLD_ID, 0, 0, wh, ww);
	pieces = malloc(sizeof(struct box_t *) * (pc + 1));
	list = malloc(sizeof(struct box_list_t) * (pc + 1));
	for (i = 0; i < pc; ++i) {
		sr = scanf("%c %lu %lu\n", &pid, &pw, &ph);
		assert(pid != WORLD_ID && sr == 3);
		pieces[i] = malloc(sizeof(struct box_t));
		pieces[i]->id = pid;
		pieces[i]->height = ph;
		pieces[i]->width = pw;
		pieces[i]->data = NULL;
	}
	pieces[i] = NULL;
	sc = 0;

	/* Setup optional bits */
	#ifdef SORT
	qsort(pieces, pc, sizeof(struct box_t *), &large_to_small);
	#endif
	for (i = 0; i < pc; ++i) {
		list[i].head = pieces[i];
		list[i].tail = list + i + 1;
	}
	list[i].head = NULL;
	list[i].tail = NULL;
	#ifdef VERBOSE
	fprintf(stderr, "INFO:\t%lux%lu BOARD w/ %lu PIECES\n",
			(long unsigned)world->height,
			(long unsigned)world->width,
			(long unsigned)pc);
	for (i = 0; i < pc; ++i) {
		print(pieces[i]);
	}
	#endif

	/* Actual packing */
	#ifdef THREADS
	thread_list.head = NULL;
	thread_list.tail = NULL;
	#endif
	pack(world, list, &sc, 0);
	printf("%lu solution(s) found.\n", (long unsigned)sc);

	/* Cleanup */
	destroy(world);
	for (i = 0; i < pc; ++i) {
		free(pieces[i]);
	}
	free(pieces);
	free(list);
	return EXIT_SUCCESS;
}
