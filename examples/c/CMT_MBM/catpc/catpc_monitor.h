#ifndef __CATPC_MONITOR_H__
#define __CATPC_MONITOR_H__

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>


/**
 * The structure to store monitoring data for all of the events
 */
struct monitoring_values {
	uint64_t llc;                /**< cache occupancy */
	double ipc;                  /**< instructions per cycle */
	uint64_t llc_misses;         /**< LLC misses - delta */
	uint64_t llc_references;     /**< LLC references */
};

/**
 * The structure to store array of monitoring data
 */
struct monitoring_values_tab {
	unsigned size;
	struct monitoring_values* values;
};

/**
 * @brief Starts monitoring on selected PIDs
 *
 * @param [in] cpu_info cpu information structure
 * @param [in] cap_mon monitoring capabilities structure
 *
 * @return Operation status
 * @retval 0 OK
 * @retval -1 error
 */

int start_monitoring(void);

/**
 * @brief Reads monitoring data
 * 
 * @param [in,out] mvalues monitoring value array
 * @param [in, out] size size of the monitoring values array
 * 
 * @retval 0 OK
 * @retval -1 error
 */
int poll_monitoring_data(struct monitoring_values_tab* mvalues_tab);

/**
 * @brief Stop monitoring on all PIDs
 * 
 */
void stop_monitoring(void);

#endif