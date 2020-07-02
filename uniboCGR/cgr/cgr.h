/** \file cgr.h
 *  
 *  \brief  This files provides the declarations of the functions
 *          to start, call and stop the CGR; included in cgr.c .
 *
 *
 ** \copyright Copyright (c) 2020, Alma Mater Studiorum, University of Bologna, All rights reserved.
 *
 *  \author Lorenzo Persampieri, lorenzo.persampieri@studio.unibo.it
 *
 *  \par Supervisor
 *          Carlo Caini, carlo.caini@unibo.it
 */

#ifndef SOURCES_CGR_CGR_H_
#define SOURCES_CGR_CGR_H_

#include "../bundles/bundles.h"
#include "../library/list/list_type.h"
#include "../routes/routes.h"
#include "../library/commonDefines.h"
#include <sys/time.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * \brief The call counter.
 */
extern unsigned int count_bundles;

#if (LOG == 1)
/**
 * \brief Set the time for the log of the current call and print the call number in the main log file.
 */
#define start_call_log(time) do { \
		setLogTime(time); \
		writeLog("###### CGR: call n. %u ######", count_bundles); \
} while(0)

/**
 * \brief Print the sequence of characters that identify the end of the call.
 */
#define end_call_log() writeLog("###############################")

#else
#define start_call_log(time) do {  } while(0)
#define end_call_log() do {  } while(0)
#endif

extern int getBestRoutes(time_t time, CgrBundle *bundle, List excludedNeighbors, List *routes);
extern int initialize_cgr(time_t time, unsigned long long ownNode);
extern void destroy_cgr(time_t time);

#ifdef __cplusplus
}
#endif

#endif /* SOURCES_CGR_CGR_H_ */
