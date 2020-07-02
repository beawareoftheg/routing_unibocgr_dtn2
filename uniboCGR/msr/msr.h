/** \file msr.h
 *
 *  \brief  This file provides the declaration of the function used
 *          to start, call and stop the Moderate Source Routing.
 *          You find also the macro to enable MSR in this CGR implementation,
 *          and other macros to change MSR behavior.
 *
 *
 ** \copyright Copyright (c) 2020, Alma Mater Studiorum, University of Bologna, All rights reserved.
 *
 *  \author Lorenzo Persampieri, lorenzo.persampieri@studio.unibo.it
 *
 *  \par Supervisor
 *          Carlo Caini, carlo.caini@unibo.it
 */

#ifndef CGR_UNIBO_MSR_MSR_H_
#define CGR_UNIBO_MSR_MSR_H_

#include <stdlib.h>
#include "../library/commonDefines.h"
#include "../library/list/list.h"
#include "../routes/routes.h"
#include "../bundles/bundles.h"

#ifndef MSR
/**
 * \brief Used to enable or disable Moderate Source Routing: set to 1 to enable MSR, otherwise set to 0.
 *
 * \hideinitializer
 */
#define MSR 0
#endif

#ifndef MSR_TIME_TOLERANCE
/**
 * \brief Tolerance for the contact's fromTime field.
 *        The fromTime of the CGRR contact can differs of MSR_TIME_TOLERANCE from the fromTime of the
 *        contact stored in the contact graph of this node.
 */
#define MSR_TIME_TOLERANCE 2
#endif

#ifndef WISE_NODE
/**
 * \brief Boolean: set to 1 (enable) if this node has the complete knowledge of the contact plan, otherwise set to 0.
 *        For a "wise node" the MSR must find all the contacts proposed in the MSR route.
 *
 * \hideinitializer
 */
#define WISE_NODE 1
#endif

#if (WISE_NODE == 0)
#ifndef MSR_HOPS_LOWER_BOUND
/**
 * \brief Used only with WISE_NODE disabled: set to N if you want that this node find (in the contact graph)
 *        at least the first N contacts (hops) of the MSR route.
 *
 * \hideinitializer
 */
#define MSR_HOPS_LOWER_BOUND 1
#endif
#endif

#if (MSR == 1)

extern int tryMSR(CgrBundle *bundle, List excludedNeighbors, FILE *file_call, List *bestRoutes);
extern int initialize_msr();
extern void destroy_msr();

#endif

/************ CHECK MACROS ERROR *************/

#if (MSR != 0 && MSR != 1)
fatal error
// Intentional compilation error
// MSR must be 0 or 1
#endif

#if (MSR == 1)

#if (WISE_NODE != 0 && WISE_NODE != 1)
fatal error
// Intentional compilation error
// WISE_NODE must be 0 or 1
#endif

#if (MSR_TIME_TOLERANCE < 0)
fatal error
// Intentional compilation error
// MSR_TIME_TOLERANCE must be greater or equal to 0.
#endif

#if (WISE_NODE == 0 && MSR_HOPS_LOWER_BOUND < 1)
fatal error
// Intentional compilation error
// MSR_HOPS_TOLERANCE must be greater or equal to 1.
#endif

#endif

/*********************************************/

#endif /* CGR_UNIBO_MSR_MSR_H_ */
