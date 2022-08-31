#ifndef __CATPC_ALLOCATOR_HPP__
#define __CATPC_ALLOCATOR_HPP__

#include <vector>
#include <unordered_map>

struct CLOS {
	int id;
	uint64_t mask;
};

struct llc_ca {
	int id;
	unsigned clos_count;
	std::vector<CLOS> clos_list;
};

/**
 * @brief Get the cache allocation config of the system
 * 
 * @return std::vector<llc> vector of llcs
 */
std::vector<llc_ca> get_allocation_config();

#endif