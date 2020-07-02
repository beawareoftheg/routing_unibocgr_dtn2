/** \file msr_utils.h
 *
 *  \brief  This file provides the declaration of some utility function to manage routes
 *          get from CGRR Extension Block and attach them to CgrBundle struct.
 *
 *
 ** \copyright Copyright (c) 2020, Alma Mater Studiorum, University of Bologna, All rights reserved.
 *
 *  \author Lorenzo Persampieri, lorenzo.persampieri@studio.unibo.it
 *
 *  \par Supervisor
 *          Carlo Caini, carlo.caini@unibo.it
 */

#ifndef CGR_UNIBO_MSR_MSR_UTILS_H_
#define CGR_UNIBO_MSR_MSR_UTILS_H_

#include <stdlib.h>
#include "../library/commonDefines.h"
#include "../contact_plan/contacts/contacts.h"
#include "../contact_plan/ranges/ranges.h"
#include "../bundles/bundles.h"
#include "../library/list/list.h"
#include "../routes/routes.h"
#include "../cgr/cgr_phases.h"
#include "msr.h"

#ifndef CGRR
#define CGRR 1
#endif

extern void delete_msr_route(Route *route);

#if (CGRR == 1 && MSR == 1)
#include "cgrr.h"
//You need to include a folder with cgrr.h in the path (options -I in gcc)
// From cgrr.h we get: CGRRouteBlock, CGRRoute, CGRRHop
/*
typedef struct
{
	uvast	fromNode;
	uvast	toNode;
	time_t	fromTime;
} CGRRHop;

//Note: uvast could be replaced with: unsigned int, unsigned long int, unsigned long long int

typedef struct
{
	unsigned int  hopCount; //Number of hops (contacts)
	CGRRHop		 *hopList; //Hop (contact): identified by [from, to, fromTime]
} CGRRoute;

//Note: hopList must be an array of hopCount size

typedef struct
{
	unsigned int recRoutesLength; //number of recomputedRoutes
	CGRRoute originalRoute; //computed by the source
	CGRRoute *recomputedRoutes; //computed by following nodes
} CGRRouteBlock;
*/
// Another implementation that defines these three struct type
// Can use the following functions:

extern int set_msr_route(time_t current_time, CGRRouteBlock *cgrrBlk, CgrBundle *bundle);

#endif




#endif /* CGR_UNIBO_MSR_MSR_UTILS_H_ */
