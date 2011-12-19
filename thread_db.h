#ifndef THREAD_DB_H
#define THREAD_DB_H

#include <assert.h>
#include <stdlib.h>
#include <pthread.h>

#include "box.h"

/* These options control the thread pool */
#define MAX_THREADS 4
/* Use this to restrict workers to spawn above a given depth */
#define MAX_BRANCH_LEVEL 4

void *
packer(void *);

/* Thread Definitions ********************************************************/

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

/* Thread Manipulations ******************************************************/

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

	/* Make sure to catch every thread, TODO are we always? */
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
		/* Clean up thread memory */
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

#endif /* THREAD_DB_H */
