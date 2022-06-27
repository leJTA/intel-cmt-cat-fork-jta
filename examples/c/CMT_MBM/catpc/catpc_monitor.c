#include "catpc_monitor.h"
#include "catpc_utils.h"

#include "pqos.h"
#include "monitoring.h"

#include <stdint.h>
#include <assert.h>


#define PQOS_MAX_PIDS 256

/**
 * Maintains a table of process id, event, number of events that are selected
 * in config string for monitoring LLC occupancy, misses, and references
 */
static struct {
        pid_t pid;
        struct pqos_mon_data *pgrp;
} sel_monitor_pid_tab[PQOS_MAX_PIDS];
static struct pqos_mon_data *m_mon_grps[PQOS_MAX_PIDS];

static struct pqos_config config;
static const struct pqos_cpuinfo *p_cpu = NULL;
static const struct pqos_cap *p_cap = NULL;
static const struct pqos_capability *cap_mon = NULL;
static enum pqos_mon_event sel_events = (enum pqos_mon_event)0;			// Monitored PQOS events
static struct process_tree* ptree = NULL;											// Process tree 
static unsigned sel_process_num = 0;												// Maintains the number of process id's we want to track

int start_monitoring(void)
{
	int ret;
	pid_t* pids = NULL;
	unsigned i;

	memset(&config, 0, sizeof(config));
	config.fd_log = STDOUT_FILENO;
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

	ptree = get_process_tree((pid_t)1);												// <-- the systemd/init process !
	sel_process_num = get_num_pids(ptree);
	pids = (pid_t*)malloc(sel_process_num * sizeof(pid_t));
	tree_to_list(ptree, pids, 0);

	for (i = 0; i < (unsigned)sel_process_num; i++) {
		m_mon_grps[i] = malloc(sizeof(**m_mon_grps));
		sel_monitor_pid_tab[i].pgrp = m_mon_grps[i];
		sel_monitor_pid_tab[i].pid = pids[i];

		ret = pqos_mon_start_pids(1, &pids[i], sel_events, NULL,
											sel_monitor_pid_tab[i].pgrp);
		if (ret != PQOS_RETVAL_OK) {
			i--;
			sel_process_num--;
			continue;
			//return -1;
		}
	}

	return 0;
}

int poll_monitoring_data(struct monitoring_values_tab* mvalues_tab)
{
	unsigned i, ret = PQOS_RETVAL_OK;
	mvalues_tab->size = sel_process_num;
	mvalues_tab->values = (struct monitoring_values*)
						malloc(sel_process_num * sizeof(struct monitoring_values));

	ret = pqos_mon_poll(m_mon_grps, (unsigned)sel_process_num);

	if (ret != PQOS_RETVAL_OK) {
		return -1;
	}

	for (i = 0; i < sel_process_num; ++i) {
		mvalues_tab->values[i].llc = m_mon_grps[i]->values.llc;
		mvalues_tab->values[i].ipc = m_mon_grps[i]->values.ipc;
		mvalues_tab->values[i].llc_misses = m_mon_grps[i]->values.llc_misses_delta;
		mvalues_tab->values[i].llc_references = m_mon_grps[i]->intl->values.llc_references_delta; 
	}

	return 0;
}

void stop_monitoring(void)
{
	unsigned i;
	for (i = 0; i < (unsigned)sel_process_num; i++) {
		pqos_mon_stop(m_mon_grps[i]);
		free(m_mon_grps[i]);
	}
	pqos_fini();
}