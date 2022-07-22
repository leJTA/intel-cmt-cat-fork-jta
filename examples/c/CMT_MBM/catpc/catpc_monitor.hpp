#ifndef __CATPC_MONITOR_HPP__
#define __CATPC_MONITOR_HPP__

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#include <string>
#include <unordered_map>
#include <map>


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
 * @brief Miss Rate Curve entry : occupancy (in kB) and miss_rate (in [0, 1])
 * 
 */
using MRC = std::map<unsigned, double>;

/**
 * @brief HPC Application data structure characterized by its command line 
 * that will be used to find all PIDs belonging to the app 
 */

struct application {
	std::string cmdline;
	monitoring_values values;
	ushort CLOS_id;

	application() : cmdline{""}, values{}, CLOS_id{0} {}

	application(const std::string& cl, const monitoring_values& v, const ushort& cid)
		: cmdline{cl}, values{v}, CLOS_id{cid} {}
};

/**
 * @brief Starts monitoring on selected PIDs
 *
 * @param [in] applications application map list
 * 
 * @return Operation status
 * @retval 0 OK
 * @retval -1 error
 */

int start_monitoring(std::unordered_map<std::string, application*>&);

/**
 * @brief Reads monitoring data
 * 
 * @param [in] applications application map list
 * 
 * @retval 0 OK
 * @retval -1 error
 */
int poll_monitoring_data(std::unordered_map<std::string, application*>&);

/**
 * @brief Stop monitoring on all PIDs
 * 
 * @param [in] applications application map list
 * 
 */
void stop_monitoring(std::unordered_map<std::string, application*>&);

#endif