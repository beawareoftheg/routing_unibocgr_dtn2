/** \file cgr_phases.h
 *	
 *  \brief  This file contains the declarations of the functions
 *          to call and manage each of the three logical CGR phases.
 *          It also contains macros to enable or disable some CGR features or
 *          change related default settings.
 *
 *
 *  \details You find the implementation of the functions declared here
 *           in the files phase_one.c, phase_two.c and phase_three.c .
 *
 *
 ** \copyright Copyright (c) 2020, Alma Mater Studiorum, University of Bologna, All rights reserved.
 *
 *  \author Lorenzo Persampieri, lorenzo.persampieri@studio.unibo.it
 *
 *  \par Supervisor
 *       Carlo Caini, carlo.caini@unibo.it
 */

#ifndef SOURCES_CGR_PHASES_H_
#define SOURCES_CGR_PHASES_H_

#include "../bundles/bundles.h"
#include "../contact_plan/nodes/nodes.h"
#include "../library/list/list_type.h"
#include "../library/commonDefines.h"
#include "../ported_from_ion/scalar/scalar.h"
#include <sys/time.h>

#ifdef __cplusplus
extern "C"
{
#endif

/******************************************************/

/************** BUILT-IN IMPLEMENTATIONS **************/
/*
 * In this section you find some macro to enable or disable
 * a series of built-in implementations.
 * Keep in mind that they must be mutually exclusive.
 *
 * Built-in implementations:
 * - CGR_UNIBO_SUGGESTED_SETTINGS
 * - CCSDS_SABR_DEFAULTS
 * - CGR_ION_3_7_0
 *
 */

#ifndef CGR_UNIBO_SUGGESTED_SETTINGS
/**
 * \brief Enable to get a CGR with our suggested settings.
 *
 * \details Set to 1 to enforce our suggested settings for this CGR implementation.
 *          Otherwise set to 0.
 *
 * \hideinitializer
 */
#define CGR_UNIBO_SUGGESTED_SETTINGS 0
#endif

#if(CGR_UNIBO_SUGGESTED_SETTINGS == 1)
#undef CGR_AVOID_LOOP
#undef MAX_LOOPS_NUMBER
#undef QUEUE_DELAY
#undef MAX_DIJKSTRA_ROUTES
#undef ADD_COMPUTED_ROUTE_TO_INTERMEDIATE_NODES
#undef NEGLECT_CONFIDENCE
#undef PERC_CONVERGENCE_LAYER_OVERHEAD
#undef MIN_CONVERGENCE_LAYER_OVERHEAD

#define CGR_AVOID_LOOP 3
#define MAX_LOOPS_NUMBER -1
#define QUEUE_DELAY 1
#define MAX_DIJKSTRA_ROUTES 0
#define ADD_COMPUTED_ROUTE_TO_INTERMEDIATE_NODES 0
#define NEGLECT_CONFIDENCE 0
#ifndef MIN_CONFIDENCE_IMPROVEMENT
#define	MIN_CONFIDENCE_IMPROVEMENT	(0.05)
#endif
#define PERC_CONVERGENCE_LAYER_OVERHEAD (6.25)
#define MIN_CONVERGENCE_LAYER_OVERHEAD 100 //TODO (ION 3.7.0: 36, CCSDS SABR: 100).
#endif


#ifndef CCSDS_SABR_DEFAULTS
/**
 * \brief   Enable to get a CGR that follows all and only the criteria listed by CCSDS SABR.
 *
 * \details Set to 1 to enforce a CCSDS SABR like behavior
 *          (it will disable all ION and Unibo_CGR enhancements when in contrast to CCSDS SABR).
 *          Otherwise set to 0.
 *
 * \hideinitializer
 */
#define CCSDS_SABR_DEFAULTS 0
#endif

#if (CCSDS_SABR_DEFAULTS == 1)
#undef CGR_AVOID_LOOP
#undef QUEUE_DELAY
#undef MAX_DIJKSTRA_ROUTES
#undef ADD_COMPUTED_ROUTE_TO_INTERMEDIATE_NODES
#undef NEGLECT_CONFIDENCE
#undef PERC_CONVERGENCE_LAYER_OVERHEAD
#undef MIN_CONVERGENCE_LAYER_OVERHEAD
//TODO #undef MIN_CONFIDENCE_IMPROVEMENT

#define CGR_AVOID_LOOP 0
#define QUEUE_DELAY 0
#define MAX_DIJKSTRA_ROUTES 1
#define ADD_COMPUTE_ROUTE_TO_INTERMEDIATE_NODES 0
#define NEGLECT_CONFIDENCE 1
//TODO #define MIN_CONFIDENCE_IMPROVEMENT (1.0) ???
#define PERC_CONVERGENCE_LAYER_OVERHEAD (3.0)
#define MIN_CONVERGENCE_LAYER_OVERHEAD 100
#endif

#ifndef CGR_ION_3_7_0
/**
 * \brief   Enable to get an ION-3.7.0's CGR implementation like.
 *
 * \details Set to 1 to enforce a ION-3.7.0 like behavior (it will disable all Unibo_CGR enhancements),
 *          otherwise set to 0.
 *
 * \par Notes:
 *           - You could find some difference with the ION's CGR implementation like
 *             proactive fragmentation (here absent).
 *
 * \hideinitializer
 */
#define CGR_ION_3_7_0 0
#endif

#if (CGR_ION_3_7_0 == 1)

#undef CGR_AVOID_LOOP
#undef QUEUE_DELAY
#undef MAX_DIJKSTRA_ROUTES
#undef ADD_COMPUTED_ROUTE_TO_INTERMEDIATE_NODES
#undef NEGLECT_CONFIDENCE
#undef PERC_CONVERGENCE_LAYER_OVERHEAD
#undef MIN_CONVERGENCE_LAYER_OVERHEAD

#define CGR_AVOID_LOOP 0
#define QUEUE_DELAY 0
#define MAX_DIJKSTRA_ROUTES 1
#define ADD_COMPUTE_ROUTE_TO_INTERMEDIATE_NODES 0
#define NEGLECT_CONFIDENCE 0
#ifndef MIN_CONFIDENCE_IMPROVEMENT
// ION 3-7-0 uses the same macro name.
#define	MIN_CONFIDENCE_IMPROVEMENT	(0.05)
#endif
#define PERC_CONVERGENCE_LAYER_OVERHEAD (6.25)
#define MIN_CONVERGENCE_LAYER_OVERHEAD 36
#endif


/******************************************************/

/**********************SHARED MACROS*******************/
// Macros used in multiple CGR logical phases.

/**
 * \brief   The maximum rate of change in distance between any two nodes in the
 *          network is 450,000 miles (720,000 km) per hour.
 *
 * \details From SABR CCSDS Blue book, 2.4.2
 */
#define	MAX_SPEED_MPH	(450000)

// TODO modificare documentazione (e forse anche  il nome),
//      adesso l'utilizzo di questa macro è condiviso dalla fase due
#ifndef MAX_DIJKSTRA_ROUTES
/**
 * \brief   This macro limits the number of Dijkstra's routes calculated by the Unibo_CGR "one-route-per-neighbor" enhancement.
 *
 * \details CCSDS SABR leaves to the implementation the number of routes computed by Dijkstra at each call of Phase 1; in ION only one route is computed.
 *          Better perfomance can be achived by computing additional routes, in particular by forcing one route for each neighbor, if possible ("one-route-
 *          per-neighbor").
 *          To cope with environments with stringent computational constraints, it is possible to limit the number of routes to the best N neighbors.
 *          - Set to 0 to disable the limit (i.e. leaving one-route-per-neigbhor unmodified).
 *          - Set to N (N > 0) to limit the number of routes; in particular, set to 1 to enforce ION behavior.
 *
 * \hideinitializer
 */
#define MAX_DIJKSTRA_ROUTES 0
#endif

#ifndef CGR_AVOID_LOOP
/**
 * \brief    Unibo_CGR provides the user with both a reactive and a proactive mechanism to counteract loops.
 *
 * \details This macro is used in all logical phases.
 *          - Set to 0 to disable all anti-loop mechanisms (as in CCSDS and ION SABR).
 *          - Set to 1 to enable the reactive version only.
 *          - Set to 2 to enable the proactive version only.
 *          - Set to 3 to enable both.
 *
 * \hideinitializer
 */
#define CGR_AVOID_LOOP 3

#endif

#if (CGR_AVOID_LOOP > 0)
#undef NO_LOOP
#undef POSSIBLE_LOOP
#undef CLOSING_LOOP
#undef FAILED_NEIGHBOR
/**
 * \brief   Used to flag candidate routes that the anti-loop mechanism consider as "No loop routes".
 *
 * \details For the reactive version it means that the route's neighbor isn't a failed neighbor.
 *          For the proactive version it means that there aren't matches between
 *          the candidate route and the geo route of the bundle.
 *          For the proactive and reactive version it means that the CGR of the local node
 *          doesn't see a risk of a routing loop using this candidate route.
 *          Don't change this value.
 */
#define NO_LOOP 1
/**
 * \brief   Used to flag routes considered as "Possible loop routes" by the anti-loop proactive version.
 *
 * \details It means that at least one node of the route (except the neighbor)
 *          matches with one node of the geographic route of the bundle.
 *          Don't change this value.
 */
#define POSSIBLE_LOOP 2
/**
 * \brief   Used to flag routes considered as "Closing loop routes" by the anti-loop proactive version.
 *
 * \details It means that the route's neighbor matches with one node of the bundle's geo route.
 *          Don't change this value.
 */
#define CLOSING_LOOP 3
/**
 * \brief   Used by the anti-loop reactive versione to flag routes with a failed neighbor.
 *
 * \details It means that the route's neighbor matches with one bundle's failed neighbor.
 *          Don't change this value.
 *
 * \par Notes:
 *           - The bundle's failed neighbors list is the list of neighbor to which the
 *             bundle was previously forwarded by the local node but due a loop
 *             it's come back to the local node.
 */
#define FAILED_NEIGHBOR 4
#endif

#ifndef NEGLECT_CONFIDENCE
/**
 * \brief   Used to enable or disable the use of the confidence for the computation and the choose of the routes.
 *
 * \details ION implementation differs slightly from CCSDS SABR in the candidate and best route selection criteria
 *          as ION use the route confidence, not mentioned in CCSDS SABR.
 *          Set to 1 if you want CGR to strictly implements the CCSDS SABR criteria, thus to neglect confidence.
 *          Otherwise set to 0.
 *
 * \hideinitializer
 */
#define NEGLECT_CONFIDENCE 0
#endif
/******************************************************/

/*******************PHASE ONE MACROS*******************/
// Macros used in phase one only

#ifndef ADD_COMPUTED_ROUTE_TO_INTERMEDIATE_NODES
/**
 * \brief Enable to compute routes in advance to some nodes.
 *
 * \details This is a possible enhancement provided by Unibo_CGR. Dijkstra is called in phase 1 to compute a CGR route
 *          (sequence of contacts) to destination D. At computation time ("t0") this route, however, because of Dikstra's optimality,
 *          it is also the best route to get not only to D, but to all nodes along the geo route subjected at the CGR route (e.g. B-C-F-D).
 *          Set to 1 if you want to add the computed route to the computed routes list of each node of the GEO route (e.g. B,C,F and D).
 *          Set to 0 if you want the computed route to be added only to D, thus disabling this optimization.
 *          Note that this optimization is intended to reduce computational load, should a following bundle be destined (at "t1") to an intermediate node
 *          (e.g. B,C or F). On the other hand, a route computed at "t0" may be no more optimal at "t1", with a possible impact on routing accuracy.
 *          - Set to 1 to possibly reduce computational load.
 *          - Set to 0 to to preserve standard behavior (to disable the enhancement).
 *
 * \hideinitializer
 */
#define ADD_COMPUTED_ROUTE_TO_INTERMEDIATE_NODES 0
#endif

/******************************************************/

/*******************PHASE TWO MACROS*******************/
// Macros used in phase two only

#ifndef QUEUE_DELAY
/**
 * \brief   ETO on all hops
 *
 * \details This is a possible enhancement provided by Unibo_CGR. In both CCSDS SABR and ION only the local queue delay is computed ("ETO on first hop")
 *          However, by exploiting the Expected Volume Consumptions of the route contacts, it is possible to have a rough but conservative estimation of
 *          queuing delay on next hops.
 *          - Set to 1 to consider ETO (expected queue delays) on all hops of the route.
 *          - Set to 0 to disable this enhancement.
 *
 * \hideinitializer
 */
#define QUEUE_DELAY 1
#endif

#ifndef MIN_CONFIDENCE_IMPROVEMENT
/**
 * \brief   Lower confidence for a route.
 *
 * \details It must be between 0.0 and 1.0 .
 *          This macro should assume the same value used by the bundle protocol.
 *          Keep in mind that ION 3.7.0 use 0.05 as default value.
 *
 * \warning Change this value only if you know what you are doing.
 */
#define	MIN_CONFIDENCE_IMPROVEMENT	(0.05)
#endif

#ifndef PERC_CONVERGENCE_LAYER_OVERHEAD
/**
 * \brief   Value used to compute the estimated convergence-layer overhead.
 *
 * \details It must be greater or equal to 0.0 .
 *          Keep in mind that ION 3.7.0 use 6.25% as default value.
 */
#define PERC_CONVERGENCE_LAYER_OVERHEAD (6.25)
#endif

#ifndef MIN_CONVERGENCE_LAYER_OVERHEAD
/**
 * \brief   Minimum value for the estimated convergence-layer overhead.
 *
 * \details It assumes the same meaning of the macro TIPYCAL_STACK_OVERHEAD in ION's bp/library/bpP.h.
 *
 * \par Notes:
 *           - CCSDS SABR: 100
 *           - ION 3.7.0:   36 (default value in ION but it can be modified)
 *
 */
#define MIN_CONVERGENCE_LAYER_OVERHEAD 100
#endif
/******************************************************/

/**
 * \brief  The current "internal" time of the CGR.
 */
extern time_t current_time;
/**
 * \brief The own ipn node.
 */
extern unsigned long long localNode;

/***************************** PHASE ONE *****************************/
extern int initialize_phase_one(unsigned long long localNode);
extern void reset_phase_one();
extern void destroy_phase_one();
extern int computeRoutes(Node *terminusNode, List subsetComputedRoutes, long unsigned int missingNeighbors);
/*********************************************************************/

/***************************** PHASE TWO *****************************/
extern int initialize_phase_two();
extern void destroy_phase_two();
extern void reset_phase_two();
extern int checkRoute(CgrBundle *bundle, List excludedNeighbors, Route *route);
extern int getCandidateRoutes(Node *terminusNode, CgrBundle *bundle, List excludedNeighbors, List computedRoutes,
		List *subsetComputedRoutes, long unsigned int *missingNeighbors, List *candidateRoutes);
extern int computeApplicableBacklog(unsigned long long neighbor, int priority, unsigned int ordinal, CgrScalar *applicableBacklog,
		CgrScalar *totalBacklog);
/*********************************************************************/

/**************************** PHASE THREE ****************************/
extern int chooseBestRoutes(CgrBundle *bundle, List candidateRoutes);
/*********************************************************************/

#if (LOG == 1)
extern void print_phase_one_routes(FILE *file, List computedRoutes);
extern void print_phase_two_routes(FILE *file, List candidateRoutes);
extern void print_phase_three_routes(FILE *file, List bestRoutes);
#else
#define print_phase_one_routes(file, computedRoutes) do { } while(0)
#define print_phase_two_routes(file, candidateRoutes) do { } while(0)
#define print_phase_three_routes(file, bestRoutes) do { } while(0)
#endif

/******************CHECK MACROS ERROR******************/
/**
 * \cond
 */
#if (MAX_DIJKSTRA_ROUTES < 0)
fatal error
// Intentional compilation error
// MAX_DIJKSTRA_ROUTES must be >= 0.
#endif

#if (NEGLECT_CONFIDENCE != 0 && NEGLECT_CONFIDENCE != 1)
fatal error
// Intentional compilation error
// NEGLECT_CONFIDENCE must be 0 or 1.
#endif

#if (ADD_COMPUTED_ROUTE_TO_INTERMEDIATE_NODES != 0 && ADD_COMPUTED_ROUTE_TO_INTERMEDIATE_NODES != 1)
fatal error
// Intention compilation error
// ADD_COMPUTED_ROUTE_TO_INTERMEDIATE_NODES must be 0 or 1
#endif

#if (QUEUE_DELAY != 0 && QUEUE_DELAY != 1)
fatal error
// Intentional compilation error
// QUEUE_DELAY must be 0 or 1.
#endif

#if (CGR_AVOID_LOOP != 0 && CGR_AVOID_LOOP != 1 && CGR_AVOID_LOOP != 2 && CGR_AVOID_LOOP != 3)
fatal error
// Intentional compilation error
// CGR_AVOID_LOOP must be 0 or 1 or 2 or 3.
#endif

#if (CGR_UNIBO_SUGGESTED_SETTINGS != 0 && CGR_UNIBO_SUGGESTED_SETTINGS != 1)
// Intentional compilation error
// CGR_UNIBO_SUGGESTED_SETTINGS must be 0 or 1.
fatal error
#endif

#if (CCSDS_SABR_DEFAULTS != 0 && CCSDS_SABR_DEFAULTS != 1)
// Intentional compilation error
// CCSDS_SABR_DEFAULTS must be 0 or 1.
fatal error
#endif

#if (CGR_ION_3_7_0 != 0 && CGR_ION_3_7_0 != 1)
// Intentional compilation error
// CGR_ION_3_7_0 must be 0 or 1.
fatal error
#endif

#if( (CCSDS_SABR_DEFAULTS == 1 && (CGR_UNIBO_SUGGESTED_SETTINGS != 0 || CGR_ION_3_7_0 != 0)) \
	|| ( CGR_UNIBO_SUGGESTED_SETTINGS == 1 && (CCSDS_SABR_DEFAULTS != 0 || CGR_ION_3_7_0 != 0)) \
	|| (CGR_ION_3_7_0 == 1 && (CCSDS_SABR_DEFAULTS != 0 || CGR_UNIBO_SUGGESTED_SETTINGS != 0)) )
fatal error
// Intentional compilation error
// CGR_UNIBO_SUGGESTED_SETTINGS, CCSDS_SABR_DEFAULTS and CGR_ION_3_7_0 must be mutually exclusive.
#endif
/**
 * \endcond
 */
/******************************************************/



#ifdef __cplusplus
}
#endif

#endif /* SOURCES_CGR_PHASES_H_ */
