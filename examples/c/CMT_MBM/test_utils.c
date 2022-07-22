#include <stdio.h>
#include <stdlib.h>
#include "catpc_utils.hpp"

int main(int argc, char** argv)
{
	if (argc < 2)
		return 1;

	pid_t pid = atoi(argv[1]);

	struct process_tree* tree = get_process_tree(pid);
	if (tree == NULL) {
		printf("Ooopss!!\n");
		return 1;
	}
	int pid_count = get_num_pids(tree);
	pid_t* pids = (pid_t*)malloc(pid_count * sizeof(pid_t));

	tree_to_list(tree, pids, 0);

	int i = 0;
	for (i = 0; i < pid_count; ++i) {
		printf("%d ", pids[i]);
	}
	printf("\n%d processes in the tree \n", pid_count);

	return 0;
}