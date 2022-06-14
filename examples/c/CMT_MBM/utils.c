#include "utils.h"

struct process_tree* get_process_tree(pid_t ppid)
{
	if (kill(ppid, 0)) {
		fprintf(stderr, "PID %d does not exist.\n", ppid);
		return NULL;
	}

	struct process_tree* tree = (struct process_tree*)malloc(sizeof(struct process_tree));
	char buf[128];
	FILE* file;
	int count = 0;

	tree->pid = ppid;
	tree->children = NULL;

	sprintf(buf, "/proc/%d/task/%d/children", (int)ppid, (int)ppid);
	file = fopen(buf, "r+");

	if (file == NULL) {
		fprintf(stderr, "PID %d : unable to open children file.\n", ppid);
		return NULL;
	}

	while ( fscanf(file, "%s", buf) != EOF) {
		count++;
	}

	tree->child_count = count;
	if (count > 0) {
		int i = 0;
		tree->children = (struct process_tree**)malloc(count * sizeof(struct process_tree*));
		
		fseek(file, 0, SEEK_SET);
		for (i = 0; i < tree->child_count; ++i) {
			fscanf(file, "%s", buf);
			ppid = atoi(buf);
			tree->children[i] = get_process_tree(ppid);
		}
	}

	fclose(file);
	return tree;	
	
}

int get_num_pids(struct process_tree* tree)
{
	if (tree->child_count == 0) {
		return 1;
	}
	int i = 0, sum = 0;
	for (i = 0; i < tree->child_count; ++i) {
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