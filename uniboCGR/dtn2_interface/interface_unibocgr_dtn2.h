/** \file interface_unibocgr_dtn2.h
 *
 *  \brief This file provides the definitions of the functions
 *         that make UniboCGR's implementation compatible with DTN2,
 *         included in interface_unibocgr_dtn2.c
 *
 ** \copyright Copyright (c) 2020, Alma Mater Studiorum, University of Bologna, All rights reserved.
 *
 *  \author Giacomo Gori, giacomo.gori3@studio.unibo.it
 *
 *  \par Supervisor
 *       Carlo Caini, carlo.caini@unibo.it
 */


#ifndef SOURCES_INTERFACE_UNIBO_CGR_DTN2_H_
#define SOURCES_INTERFACE_UNIBO_CGR_DTN2_H_

//include from dtn2
#include "../bundling/bundle.h"

#include "../../../../ici/include/ion.h"
#include "../../../../ici/include/platform.h"
#include "../../../../ici/include/sdrstring.h"
#include "../../../library/cgr.h"
#include "../../../library/bpP.h"
#include "bp.h"
#include "cgrr.h"
#include "rgr.h" 

#include <string>

#ifdef QUEUE_DELAY
#undef QUEUE_DELAY
// Just to avoid macro redefinition error
// (same name used in ION and in this CGR implementation)
#endif

//DTN2 is written in c++
//#ifndef __cplusplus
//#define __cplusplus
//#endif

#include "../msr/msr.h"
#include <sys/time.h>

#ifdef __cplusplus
extern "C"
{
#endif

extern int callUniboCGR(time_t time, Bundle *bundle,
		 std::string *res);
extern void destroy_contact_graph_routing(time_t time);
extern int initialize_contact_graph_routing(unsigned long long ownNode, time_t time);

#ifdef __cplusplus
}
#endif

#endif /* SOURCES_INTERFACE_UNIBO_CGR_DTN2_H_ */