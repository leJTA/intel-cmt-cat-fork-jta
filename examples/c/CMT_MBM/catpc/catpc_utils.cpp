#include "catpc_utils.hpp"

#include <dirent.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>
#include <stdio.h>

/**
 * @brief filter for scandir.
 *
 * Skips entries starting with "."
 * Skips non-numeric entries
 *
 * @param[in] dir scandir dirent entry
 *
 * @retval 0 for entries to be skipped
 * @retval 1 otherwise
 */
static int filter_pids(const struct dirent *dir)
{
	size_t char_idx;

	if (dir->d_name[0] == '.')
		return 0;

	for (char_idx = 0; char_idx < strlen(dir->d_name); ++char_idx) {
		if (!isdigit(dir->d_name[char_idx]))
			return 0;
	}

	return 1;
}


/**
 * @brief Returns the process tree from the parent pid
 * 
 * @param [in] ppid parent pid
 * @param [in] allow_task_lookup allow siblings lookup or not
 * 
 * @return process_tree
 */
static struct process_tree* _get_process_tree(pid_t ppid, int allow_task_lookup)
{
	if (kill(ppid, 0)) {
		fprintf(stderr, "PID %d does not exist.\n", ppid);
		return NULL;
	}

	struct process_tree* tree = (struct process_tree*)malloc(sizeof(struct process_tree));
	struct dirent** namelist;
	char buf[512];
	FILE* file;
	int sibling_count = 0, child_count = 0, n = 0, i = 0;

	tree->pid = ppid;
	tree->children = NULL;

	// Get all siblings
	if (allow_task_lookup) {
		sprintf(buf, "/proc/%d/task", (int)tree->pid);
		n = scandir(buf, &namelist, filter_pids, NULL);
		sibling_count = n - 1;
		if (n == -1) {
			fprintf(stderr, "ERROR: scandir (%s): %s\n", buf, strerror(errno));
			return NULL;
		}

		tree->children = (struct process_tree**)malloc((sibling_count) * sizeof(struct process_tree*));	// sibling_count - 1 because we do not count the current(parent) process
		while (--n) {
			ppid = atoi(namelist[n]->d_name);
			if (tree->pid == ppid) {	// skip the current process
				continue;
			}
			tree->children[i] = _get_process_tree(ppid, 0);
			i++;
		}
	}

	// Get all children
	sprintf(buf, "/proc/%d/task/%d/children", (int)tree->pid, (int)tree->pid);
	file = fopen(buf, "r+");
	if (file == NULL) {
		fprintf(stderr, "PID %d : unable to open children file.\n", tree->pid);
		return NULL;
	}

	memset(buf, 0, sizeof(buf));
	while ( fscanf(file, "%s", buf) != EOF) {
		child_count++;
	}
	
	tree->child_count = sibling_count + child_count;
	if (child_count > 0) {
		tree->children = (struct process_tree**)realloc(tree->children, tree->child_count * sizeof(struct process_tree*));
		
		fseek(file, 0, SEEK_SET);
		for (i = sibling_count; i < tree->child_count; ++i) {
			fscanf(file, "%s", buf);
			ppid = atoi(buf);
			tree->children[i] = _get_process_tree(ppid, 1);
		}
	}

	fclose(file);
	return tree;	
	
}

struct process_tree* get_process_tree(pid_t ppid)
{
	return _get_process_tree(ppid, 1);
}

int get_num_pids(struct process_tree* tree)
{
	if (tree->child_count == 0) {
		return 1;
	}
	int i = 0, sum = 0;
	for (i = 0; i < tree->child_count; ++i) {
		if (tree->children[i] == NULL) {
			printf("%d\n", tree->pid);
		}
		sum += get_num_pids(tree->children[i]);
	}
	return 1 + sum;
}

int tree_to_list(struct process_tree* tree, pid_t* pids, int index)
{
	int i = index, j = 0;
	pids[i] = tree->pid;

	i++;
	for (j = 0; j < tree->child_count; ++j) {
		i = tree_to_list(tree->children[j], pids, i);
	}

	return i;
}


int get_pids_by_cmdline(pid_t* pids, const char* cmdline)
{
	char filepath[32];
	char buf[1024];
	char c = '\0';
	int i = 0, sz = 0;
	pid_t pid;
	FILE* fp = popen("/bin/ps -A -o pid=", "r");	// list of all pids
	FILE* cmdfile = NULL;
	
	if (fp == NULL) {
		return 0;
	}

	while (fscanf(fp, "%d", &pid) != EOF) {
		sprintf(filepath, "/proc/%d/cmdline", pid);
		cmdfile = fopen(filepath, "r");
		
		if (cmdfile == NULL)
			continue;

		// read command of the process
		i = 0;
		while ( fscanf(cmdfile, "%c", &c) != EOF) {
			if (c == '\0') {
				continue;	
			}
			buf[i++] = c;
		}
		buf[i] = '\0';

		if (strcmp(buf, cmdline) == 0) {
			pids[sz++] = pid;
		}
	}

	return sz;
}

void log_fprint(FILE* fp, const char* fmt, ...)
{
	va_list args;
	time_t timestamp;
	struct tm* now;
	char buff[70];
	
	time(&timestamp);
	now  = localtime(&timestamp);
	strftime(buff, sizeof buff, "%F %H:%M:%S", now);
	
	fprintf(fp, "[%s] ", buff);
	va_start(args, fmt);
	vfprintf(fp, fmt, args);
	va_end(args);
	fflush(fp);
}