#include <catpc_allocator.hpp>
#include <catpc_monitor.hpp>
#include "catpc_utils.hpp"

#include "pqos.h"

static FILE* test_log;
void set_logfile(FILE* file) {test_log = file;}

std::vector<llc_ca> get_allocation_config()
{
	unsigned l3cat_id_count, *l3cat_ids = NULL;
	std::vector<llc_ca> llc_ca_list;
	catpc_system_info si = get_system_info();
	
	/* Get CPU l3cat id information to set COS */
	l3cat_ids = pqos_cpu_get_l3cat_ids(si.p_cpu, &l3cat_id_count);
	if (l3cat_ids == NULL) {
		return {};
	}

	for (unsigned i = 0; i < l3cat_id_count; i++) {
		struct pqos_l3ca tab[PQOS_MAX_L3CA_COS];
		unsigned num = 0;
		int ret = pqos_l3ca_get(l3cat_ids[i], PQOS_MAX_L3CA_COS, &num, tab);
		
		llc_ca_list.push_back(llc_ca());
		llc_ca_list.back().id = l3cat_ids[i];
		llc_ca_list.back().clos_count = num;

		if (ret == PQOS_RETVAL_OK) {
			for (unsigned n = 0; n < num; n++) {
				llc_ca_list.back().clos_list.push_back(CLOS());
				llc_ca_list.back().clos_list.back().id = tab[n].class_id;
				llc_ca_list.back().clos_list.back().mask = tab[n].u.ways_mask;
			}
		}
		else {
			return {};
		}
	}

	return llc_ca_list;
}

int perform_allocation(std::unordered_map<std::string, catpc_application*>& applications)
{
	int ret = 0, sz = 0;
	pid_t pids[256];
	for (std::pair<std::string, catpc_application*> element : applications) {
		sz = get_pids_by_cmdline(pids, element.first.c_str());
		for (int i = 0; i < sz; ++i) {
			ret = pqos_alloc_assoc_set_pid(pids[i], element.second->CLOS_id);
			if (ret != PQOS_RETVAL_OK) {
				return -1 * ret;
			}
		}
	}
	return 0;
}