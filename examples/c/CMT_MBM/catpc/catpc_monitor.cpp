#include "catpc_monitor.hpp"
#include "catpc_utils.hpp"

#include "pqos.h"
#include "monitoring.h"

#include <vector>
#include <memory>
#include <unordered_map>
#include <string>
#include <stdint.h>
#include <assert.h>


#define PQOS_MAX_APPS 256

static struct pqos_config config;
static const struct pqos_cpuinfo *p_cpu = NULL;
static const struct pqos_cap *p_cap = NULL;
static const struct pqos_capability *cap_mon = NULL;
static enum pqos_mon_event sel_events = (enum pqos_mon_event)0;			// Monitored PQOS events
static unsigned sel_process_num = 0;												// Maintains the number of process id's we want to track

static std::unordered_map<std::string, struct pqos_mon_data*> m_mon_grps; 

int start_monitoring(std::unordered_map<std::string, application*>& applications)
{
	int ret = 0;
	pid_t pids[256];

	memset(&config, 0, sizeof(config));
	config.fd_log = 0;
	config.verbose = 0;
	config.interface = PQOS_INTER_OS;

	/* PQoS Initialization - Check and initialize CAT and CMT capability */
	ret = pqos_init(&config);
	if (ret != PQOS_RETVAL_OK) {
				return -1;
	}
	/* Get CMT capability and CPU info pointer */
	ret = pqos_cap_get(&p_cap, &p_cpu);
	if (ret != PQOS_RETVAL_OK) {
				return -1;
	}
	ret = pqos_cap_get_type(p_cap, PQOS_CAP_TYPE_MON, &cap_mon);
	if (ret != PQOS_RETVAL_OK) {
				return -1;
	}
	
	sel_events = (enum pqos_mon_event)(PQOS_MON_EVENT_L3_OCCUP |
									 PQOS_PERF_EVENT_LLC_MISS |
									 PQOS_PERF_EVENT_IPC |
									 PQOS_PERF_EVENT_LLC_REF);
	
	for (std::pair<std::string, application*> element : applications) {
		sel_process_num = get_pids_by_cmdline(pids, element.first.c_str());
		m_mon_grps[element.first] = new pqos_mon_data();
		ret = pqos_mon_start_pids(sel_process_num, pids, sel_events, NULL,
											m_mon_grps[element.first]);
		if (ret != PQOS_RETVAL_OK) {
			return -1;
			// continue;
		}
	}

	return 0;
}

int poll_monitoring_data(std::unordered_map<std::string, application*>& applications)
{
	unsigned i, ret = PQOS_RETVAL_OK;
	for (std::pair<std::string, application*> element : applications) {
		ret = pqos_mon_poll(&m_mon_grps[element.first], 1);

		if (ret != PQOS_RETVAL_OK) {
			//applications.erase(element.first);
			return -1 * ret;
		}

		for (i = 0; i < sel_process_num; ++i) {
			element.second->values.llc = m_mon_grps[element.first]->values.llc;
			element.second->values.ipc = m_mon_grps[element.first]->values.ipc;
			element.second->values.llc_misses = m_mon_grps[element.first]->values.llc_misses_delta;
			element.second->values.llc_references = m_mon_grps[element.first]->intl->values.llc_references_delta; 
		}
	}

	return 0;
}

void stop_monitoring(std::unordered_map<std::string, application*>& applications)
{
	for (std::pair<std::string, application*> element : applications) {
		pqos_mon_stop(m_mon_grps[element.first]);
		delete m_mon_grps[element.first];
	}
	pqos_fini();
}