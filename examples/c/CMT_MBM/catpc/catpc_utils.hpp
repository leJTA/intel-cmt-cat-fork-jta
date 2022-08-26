#ifndef __CATPC_UTILS_HPP__
#define __CATPC_UTILS_HPP__

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>

#include "catpc_monitor.hpp"

/**
 * @brief CATPC message values
 * 
 */
enum catpc_message {
	CATPC_GET_MONITORING_VALUES = 0, /**< retrieve monitoring values */ 
	CATPC_GET_ALLOCATION_CONF = 1,   /**< retrieve allocation configuration */ 
	CATPC_PERFORM_ALLOCATION = 2,    /**< perform allocation */
	CATPC_ADD_APP_TO_MONITOR = 3,		/**< add application to monitor */
	CATPC_REMOVE_APP_TO_MONITOR = 4		/**< remove application to monitor */
};

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
 * @brief Returns the process tree from the parent pid
 * 
 * @param [in] ppid parent pid
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
 * @brief Retrieve process ids created from a command line
 * 
 * @param [in] cmdline full command line with spaces removed
 * @param [out] pids array of pids
 * @return number of pids
 */
int get_pids_by_cmdline(pid_t* pids, const char* cmdline);

/**
 * @brief logging function
 * 
 * @param [in] fp file pointer to the log file
 * @param [in] fmt format string compatible with fprintf().
 * @param [in] args variadic arguments to follow depending on \a fmt. 
 */
void log_fprint(FILE* fp, const char* fmt, ...);

#endif