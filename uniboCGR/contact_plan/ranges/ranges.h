/** \file ranges.h
 *
 * \brief  This file provides the definition of the Range type,
 *         with all the declarations of the functions to manage the range tree.
 *
 *
 ** \copyright Copyright (c) 2020, Alma Mater Studiorum, University of Bologna, All rights reserved.
 *
 *  \author Lorenzo Persampieri, lorenzo.persampieri@studio.unibo.it
 *
 *  \par Supervisor
 *       Carlo Caini, carlo.caini@unibo.it
 */

#ifndef SOURCES_CONTACTS_PLAN_RANGES_RANGES_H_
#define SOURCES_CONTACTS_PLAN_RANGES_RANGES_H_

#include "../../ported_from_ion/rbt/rbt_type.h"
#include "../../library/commonDefines.h"
#include <sys/time.h>

#ifndef REVISABLE_RANGE
/**
 * \brief Set to 1 if you want to permits that range's owlt can be changed.
 *        Set to 0 otherwise.
 *
 * \details You should consider the owlt as contact plan changes.
 *
 * \par Notes:
 *          1. I suggest you to set this macro to 1 if you copy ranges from
 *             another contact plan by an interface.
 */
#define REVISABLE_RANGE 1
#endif

typedef struct
{
	/**
	 * \brief Start time
	 */
	time_t fromTime;
	/**
	 * \brief End time
	 */
	time_t toTime;
	/**
	 * \brief Sender ipn node
	 */
	unsigned long long fromNode;
	/**
	 * \brief Receiver ipn node
	 */
	unsigned long long toNode;
	/**
	 * \brief One-Way-Light-Time
	 */
	unsigned int owlt;
} Range;

#ifdef __cplusplus
extern "C"
{
#endif

extern int compare_ranges(void *first, void *second);
extern void free_range(void*);

extern int create_RangesGraph();

extern void removeExpiredRanges();

extern int add_range_to_graph(unsigned long long fromNode, unsigned long long toNode,
		time_t fromTime, time_t toTime, unsigned int owlt);
extern void remove_range_from_graph(time_t *fromTime, unsigned long long fromNode,
		unsigned long long toNode);
extern void remove_range_elt_from_graph(Range *range);

extern Range* get_range(unsigned long long fromNode, unsigned long long toNode, time_t fromTime,
		RbtNode **node);
extern Range* get_first_range(RbtNode **node);
extern Range* get_first_range_from_node(unsigned long long fromNodeNbr, RbtNode **node);
extern Range* get_first_range_from_node_to_node(unsigned long long fromNodeNbr,
		unsigned long long toNodeNbr, RbtNode **node);
extern Range* get_next_range(RbtNode **node);
extern Range* get_prev_range(RbtNode **node);
extern int get_applicable_range(unsigned long long fromNode, unsigned long long toNode,
		time_t targetTime, unsigned int *owltResult);

extern void reset_RangesGraph();
extern void destroy_RangesGraph();

#if (REVISABLE_RANGE)
extern int revise_owlt(unsigned long long fromNode, unsigned long long toNode, time_t fromTime, unsigned int owlt);
#endif

#if (LOG == 1)
extern int printRangesGraph(FILE *file, time_t currentTime);
#else
#define printRangesGraph(file, current_time) do {  } while(0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* SOURCES_CONTACTS_PLAN_RANGES_RANGES_H_ */
