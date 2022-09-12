#ifndef __CATPC_ALLOCATOR_HPP__
#define __CATPC_ALLOCATOR_HPP__

#include <vector>
#include <unordered_map>

#include "catpc_monitor.hpp"

struct CLOS {
	unsigned int id;
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

/**
 * @brief Perform the cache allocation on the applications
 * 
 * @param [in] applications application map list
 * 
 * @retval 0 OK
 * @retval -1 error
 */
int perform_allocation(std::unordered_map<std::string, catpc_application*>&);

void set_logfile(FILE* file);

#endif