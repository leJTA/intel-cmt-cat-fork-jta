#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>

struct process_tree {
	pid_t pid;
	struct process_tree** children;
	int child_count;
};

struct process_list {
	pid_t pid;
	struct process_list* next;
};

/**
 * The structure to store monitoring data for all of the events
 */
struct monitoring_data {
	uint64_t llc;                /**< cache occupancy */
	double ipc;                  /**< instructions per cycle */
	uint64_t llc_misses;   /**< LLC misses - delta */
	uint64_t llc_references;       /**< LLC references */
};

/**
 * @brief Returns the process tree from the parent pid
 * 
 * @param[in] ppid parent pid
 * 
 * @return process_tree
 */

struct process_tree* get_process_tree(pid_t ppid);

/**
 * @brief Process the number of processes composing the tree
 * 
 * @param[in] tree process tree
 * 
 * @return number of pids
 */
int get_num_pids(struct process_tree* tree);


/**
 * @brief Store all the pids composing the tree in the list
 * 
 * @param[in] tree the process tree
 * @param[in,out] pids the array of pids
 * @param[in] index from where to start the insertion (recursive function)
 * 
 * @return the current index
 */
int tree_to_list(struct process_tree* tree, pid_t* pids, int index);

/**
 * @brief CATPC log function
 * 
 * @param [in] fp file pointer to the log file
 * @param [in] fmt format string compatible with fprintf().
 * @param [in] args variadic arguments to follow depending on \a fmt. 
 */
void log_fprint(FILE* fp, const char* fmt, ...);

#endif