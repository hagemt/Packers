#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#define WORLD_ID '.'

#ifdef THREADS
#define MAX_THREADS 4
#define MAX_BRANCH_LEVEL 4
#define LIST_HEAD (struct thread_data_t *)(-1)
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
	}
}

void
add_box(struct box_db_t * db, struct box_t * box)
{
	struct box_list_t * node;
	assert(db && db->list.head && box);
	node = malloc(sizeof(struct box_list_t));
	node->head = copy_data(box);
	node->tail = db->list.tail;
	db->list.tail = node;
	++db->num_elements;
}

#ifdef ROTATIONS
inline void
rotate(struct box_t * box)
{
	/* TODO doesn't work with threads
	 * TODO also, it's a terrible hack in general...
	 */
	box_size_t s;
	assert(box && !box->data);
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
	struct box_t * space;
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
	struct thread_list_t * list, * next;
	pthread_mutex_t list_mutex;
	pthread_mutex_t result_mutex;
	size_t thread_count, thread_limit;
} thread_db;

struct thread_list_t *
init_thread_db(size_t thread_limit)
{
	thread_db.list = malloc(sizeof(struct thread_list_t));
	thread_db.list->head = LIST_HEAD;
	thread_db.list->tail = NULL;
	thread_db.next = thread_db.list;
	pthread_mutex_init(&thread_db.list_mutex, NULL);
	pthread_mutex_init(&thread_db.result_mutex, NULL);
	thread_db.thread_count = 0;
	thread_db.thread_limit = thread_limit;
	return thread_db.list;
}

typedef void (*thread_db_error_handler)(pthread_t);

void
destroy_thread_db(thread_db_error_handler callback)
{
	struct thread_list_t * current, * next;
	current = thread_db.list->tail;
	while (current) {
		next = current->tail;
		if (pthread_join(current->head->tid, NULL)) {
			#ifdef VERBOSE
			fprintf(stderr, "ERROR: unable to collect worker\n");
			#endif
			if (callback) {
				callback(current->head->tid);
			}
		}
		destroy(current->head->space);
		free(current->head);
		free(current);
		current = next;
	}
	free(thread_db.list);
	pthread_mutex_destroy(&thread_db.list_mutex);
	pthread_mutex_destroy(&thread_db.result_mutex);
	thread_db.thread_count = thread_db.thread_limit = 0;
}

int
add_thread(struct thread_db_t * db, struct thread_data_t * thread_data)
{
	assert(db && db->next);
	pthread_mutex_lock(&db->list_mutex);
	if (db->thread_count < db->thread_limit) {
		db->next->tail = malloc(sizeof(struct thread_list_t));
		db->next->tail->head = thread_data;
		db->next->tail->tail = NULL;
		if (pthread_create(&thread_data->tid, NULL, &packer, thread_data)) {
	  		destroy(thread_data->space);
			free(thread_data);
			free(db->next->tail);
			db->next->tail = NULL;
		} else {
			++db->thread_count;
			db->next = db->next->tail;
			pthread_mutex_unlock(&db->list_mutex);
			return EXIT_SUCCESS;
		}
	}
	pthread_mutex_unlock(&db->list_mutex);
	return EXIT_FAILURE;
}
#endif

/* Packing Functions *********************************************************/

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
pack(struct box_t * space, struct box_list_t * list, size_t depth)
{
	box_size_t i, j;
	struct box_t * piece;
	#ifdef THREADS
	struct thread_data_t * child_data;
	#endif
	assert(space && list);
	piece = list->head;

	/* Base case: we are out of pieces, so dump */
	if (!piece) {
		#ifdef THREADS
		pthread_mutex_lock(&thread_db.result_mutex);
		#endif
		add_box(&box_db, space);
		#ifdef VERBOSE
		fprintf(stderr, "INFO: found solution %lu\n", (unsigned long)box_db.num_elements);
		#endif
		#ifdef THREADS
		pthread_mutex_unlock(&thread_db.result_mutex);
		#endif
		return;
	}

	/* Otherwise, we need to try this piece at every position */
	for (i = 0; i < space->height; ++i) {
		for (j = 0; j < space->width; ++j) {
			#ifdef ROTATIONS
			check_fits:
			#endif
			if (fits(space, piece, i, j)) {
				/* If the piece fits here, either spawn or branch */
        			#ifdef THREADS
				if (depth < MAX_BRANCH_LEVEL) {
					/* Setup the thread node */
					child_data = malloc(sizeof(struct thread_data_t));
					child_data->space = copy_data(space);
					child_data->list  = list->tail;
					child_data->depth = depth;
					fill(child_data->space, piece->id, i, j, piece->height, piece->width);
					if (!add_thread(&thread_db, child_data)) { continue; }
					destroy(child_data->space);
					free(child_data);
				}
				#endif
				/* Try packing the remaining pieces and then reset the state */
				fill(space, piece->id, i, j, piece->height, piece->width);
				pack(space, list->tail, depth + 1);
				fill(space, WORLD_ID, i, j, piece->height, piece->width);
			#ifdef ROTATIONS
			/* TODO nix all of this, it's awful */
			} else if (space->id == WORLD_ID) {
				if (piece->height != piece->width) {
					space->id = '\0';
					rotate(piece);
					goto check_fits;
				}
			} else {
				space->id = WORLD_ID;
				rotate(piece);
			#endif
			}
		}
	}
}

/* Utility Functions *********************************************************/

#ifdef THREADS
void *
packer(void * package)
{
	struct thread_data_t * data = (struct thread_data_t *)package;
	#ifdef VERBOSE
	fprintf(stderr, "[thread %lu] worker started\n", (long unsigned)data->tid);
	#endif
	pack(data->space, data->list, data->depth + 1);
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
	size_t i, pc;
	box_data_t pid;
	box_size_t wh, ww, ph, pw;
	struct box_t * world, ** pieces;
	struct box_list_t * list, * result;

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

	/* Setup optional optimizations */
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
		fprintf(stderr, "\tPIECE '%c': %lux%lu\n",
				(char)pieces[i]->id,
				(long unsigned int)pieces[i]->height,
				(long unsigned int)pieces[i]->width);
	}
	#endif

	/* Actual packing */
	#ifdef THREADS
	thread_db.list = init_thread_db(MAX_THREADS);
	#endif
	box_db.list.head = world;
	box_db.list.tail = NULL;
	box_db.num_elements = 0;
	pack(world, list, 0);
	#ifdef THREADS
	destroy_thread_db(NULL);
	#endif

	/* Print results */
	if (box_db.num_elements) {
		printf("%lu solution(s) found:\n", box_db.num_elements);
	} else {
		printf("No solutions found.\n");
	}
	result = box_db.list.tail;
	while (result) {
		box_db.list.tail = result->tail;
		putchar('\n');
		print(result->head);
		destroy(result->head);
		free(result);
		result = box_db.list.tail;
	}

	/* Cleanup */
	destroy(world);
	for (i = 0; i < pc; ++i) {
		free(pieces[i]);
	}
	free(pieces);
	free(list);
	return EXIT_SUCCESS;
}
