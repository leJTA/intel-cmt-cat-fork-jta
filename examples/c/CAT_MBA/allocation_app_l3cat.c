/*
 * BSD LICENSE
 *
 * Copyright(c) 2018-2022 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/**
 * @brief Platform QoS sample COS allocation application
 *
 */

#include "pqos.h"

#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/**
 * Maintains number of Class of Services supported by socket for
 * L3 cache allocation
 */
static int sel_l3ca_cos_num = 0;
/**
 * Maintains table for L3 cache allocation class of service data structure
 */
static struct pqos_l3ca sel_l3ca_cos_tab[PQOS_MAX_L3CA_COS];
/**
 * @brief Converts string into 64-bit unsigned number.
 *
 * Numbers can be in decimal or hexadecimal format.
 *
 * On error, this functions causes process to exit with FAILURE code.
 *
 * @param s string to be converted into 64-bit unsigned number
 *
 * @return Numeric value of the string representing the number
 */
static uint64_t
strtouint64(const char *s)
{
        const char *str = s;
        int base = 10;
        uint64_t n = 0;
        char *endptr = NULL;

        if (strncasecmp(s, "0x", 2) == 0) {
                base = 16;
                s += 2;
        }
        n = strtoull(s, &endptr, base);
        if (endptr != NULL && *endptr != '\0' && !isspace(*endptr)) {
                printf("Error converting '%s' to unsigned number!\n", str);
                exit(EXIT_FAILURE);
        }
        return n;
}
/**
 * @brief Verifies and translates definition of single
 *        allocation class of service
 *        from args into internal configuration.
 *
 * @param argc Number of arguments in input command
 * @param argv Input arguments for COS allocation
 */
static void
allocation_get_input(int argc, char *argv[])
{
        uint64_t mask = 0;

        if (argc < 2)
                sel_l3ca_cos_num = 0;
        else if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "-H")) {
                printf("Usage: %s [<COS#> <COS bitmask>]\n", argv[0]);
                printf("Example: %s 1 0xff\n\n", argv[0]);
                sel_l3ca_cos_num = 0;
        } else {
                sel_l3ca_cos_tab[0].class_id = (unsigned)atoi(argv[1]);
                mask = strtouint64(argv[2]);
                sel_l3ca_cos_tab[0].u.ways_mask = mask;
                sel_l3ca_cos_num = 1;
        }
}
/**
 * @brief Sets up allocation classes of service on selected L3 CAT ids
 *
 * @param l3cat_id_count number of CPU l3cat_ids
 * @param l3cat_ids arrays with CPU l3cat id's
 *
 * @return Number of classes of service set
 * @retval 0 no class of service set (nor selected)
 * @retval negative error
 * @retval positive success
 */
static int
set_allocation_class(unsigned l3cat_id_count, const unsigned *l3cat_ids)
{
        int ret;

        while (l3cat_id_count > 0 && sel_l3ca_cos_num > 0) {
                ret = pqos_l3ca_set(*l3cat_ids, sel_l3ca_cos_num,
                                    sel_l3ca_cos_tab);
                if (ret != PQOS_RETVAL_OK) {
                        printf("Setting up cache allocation class of "
                               "service failed!\n");
                        return -1;
                }
                l3cat_id_count--;
                l3cat_ids++;
        }
        return sel_l3ca_cos_num;
}
/**
 * @brief Prints allocation configuration
 * @param l3cat_id_count number of CPU l3cat_id
 * @param l3cat_ids arrays with CPU l3cat id's
 *
 * @return PQOS_RETVAL_OK on success
 * @return error value on failure
 */
static int
print_allocation_config(const unsigned l3cat_id_count,	// --> socket count
                        const unsigned *l3cat_ids)			// --> socket ids
{
        int ret = PQOS_RETVAL_OK;
        unsigned i;

        for (i = 0; i < l3cat_id_count; i++) {
                struct pqos_l3ca tab[PQOS_MAX_L3CA_COS];	// --> array of "CLOS" data
                unsigned num = 0;

                ret = pqos_l3ca_get(l3cat_ids[i], PQOS_MAX_L3CA_COS, &num, tab);
                if (ret == PQOS_RETVAL_OK) {
                        unsigned n = 0;

                        printf("L3CA COS definitions for Socket %u:\n",
                               l3cat_ids[i]);
                        for (n = 0; n < num; n++) {
                                printf("    L3CA COS%u => MASK 0x%llx\n",
                                       tab[n].class_id,
                                       (unsigned long long)tab[n].u.ways_mask);
                        }
                } else {
                        printf("Error:%d", ret);
                        return ret;
                }
        }
        return ret;
}

int
main(int argc, char *argv[])
{
        struct pqos_config cfg;
        const struct pqos_cpuinfo *p_cpu = NULL;
        const struct pqos_cap *p_cap = NULL;
        unsigned l3cat_id_count, *p_l3cat_ids = NULL;
        int ret, exit_val = EXIT_SUCCESS;

        memset(&cfg, 0, sizeof(cfg));
        cfg.fd_log = STDOUT_FILENO;
        cfg.verbose = 0;
		  cfg.interface = PQOS_INTER_OS;
        /* PQoS Initialization - Check and initialize CAT and CMT capability */
        ret = pqos_init(&cfg);
        if (ret != PQOS_RETVAL_OK) {
                printf("Error initializing PQoS library!\n");
                exit_val = EXIT_FAILURE;
                goto error_exit;
        }
        /* Get CMT capability and CPU info pointer */
        ret = pqos_cap_get(&p_cap, &p_cpu);
        if (ret != PQOS_RETVAL_OK) {
                printf("Error retrieving PQoS capabilities!\n");
                exit_val = EXIT_FAILURE;
                goto error_exit;
        }
        /* Get CPU l3cat id information to set COS */
        p_l3cat_ids = pqos_cpu_get_l3cat_ids(p_cpu, &l3cat_id_count);
        if (p_l3cat_ids == NULL) {
                printf("Error retrieving CPU socket information!\n");
                exit_val = EXIT_FAILURE;
                goto error_exit;
        }
        /* Get input from user  */
        allocation_get_input(argc, argv);
        if (sel_l3ca_cos_num != 0) {
                /* Set bit mask for COS allocation */
                ret = set_allocation_class(l3cat_id_count, p_l3cat_ids);
                if (ret < 0) {
                        printf("Allocation configuration error!\n");
                        goto error_exit;
                }
                printf("Allocation configuration altered.\n");
        }
        /* Print COS and associated bit mask */
        ret = print_allocation_config(l3cat_id_count, p_l3cat_ids);
        if (ret != PQOS_RETVAL_OK) {
                printf("Allocation capability not detected!\n");
                exit_val = EXIT_FAILURE;
                goto error_exit;
        }
error_exit:
        /* reset and deallocate all the resources */
        ret = pqos_fini();
        if (ret != PQOS_RETVAL_OK)
                printf("Error shutting down PQoS library!\n");
        if (p_l3cat_ids != NULL)
                free(p_l3cat_ids);
        return exit_val;
}
