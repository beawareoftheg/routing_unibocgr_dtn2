/** \file contactPlan.h
 *
 *  \brief   This file provides the declarations of the functions
 *           to manage the contact plan. The functions are implemented
 *           in contactPlan.c
 *
 ** \copyright Copyright (c) 2020, Alma Mater Studiorum, University of Bologna, All rights reserved.
 *
 *  \author Lorenzo Persampieri, lorenzo.persampieri@studio.unibo.it
 *
 *  \par Supervisor
 *       Carlo Caini, carlo.caini@unibo.it
 */

#ifndef LIBRARY_CONTACTSPLAN_H_
#define LIBRARY_CONTACTSPLAN_H_

#include <sys/time.h>
#include "../library/commonDefines.h"
#include <sys/time.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * \brief The last time when we add/delete contacts/ranges.
 */
extern struct timeval contactPlanEditTime;

extern int initialize_contact_plan();

extern void removeExpired(time_t time);

extern int addContact(unsigned long long fromNode, unsigned long long toNode, time_t fromTime,
		time_t toTime, long unsigned int xmitRate, float confidence, int copyMTV, double mtv[]);
extern int removeContact(unsigned long long fromNode, unsigned long long toNode,
		time_t *fromTime);

extern int addRange(unsigned long long fromNode, unsigned long long toNode, time_t fromTime,
		time_t toTime, unsigned int owlt);
extern int removeRange(unsigned long long fromNode, unsigned long long toNode, time_t *fromTime);

extern void reset_contact_plan();
extern void destroy_contact_plan();

#ifdef __cplusplus
}
#endif

#endif /* LIBRARY_CONTACTSPLAN_H_ */
