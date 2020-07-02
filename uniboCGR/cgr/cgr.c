/** \file cgr.c
 *
 *  \brief  This file provides the implementation of the functions
 *          used to start, call and stop the CGR.
 *
 *  \details The main function are executeCGR and getBestRoutes, which call
 *	     the functions included in phase_one.c, phase_two.c and phase_three.c
 *
 *
 ** \copyright Copyright (c) 2020, Alma Mater Studiorum, University of Bologna, All rights reserved.
 *
 *  \author Lorenzo Persampieri, lorenzo.persampieri@studio.unibo.it
 *
 *  \par Supervisor
 *          Carlo Caini, carlo.caini@unibo.it
 */

#include <stdlib.h>
#include "cgr.h"
#include "../contact_plan/contacts/contacts.h"
#include "../contact_plan/ranges/ranges.h"
#include "../contact_plan/nodes/nodes.h"
#include "../library/list/list.h"
#include "../routes/routes.h"
#include "../contact_plan/contactPlan.h"
#include "cgr_phases.h"
#include <stdio.h>
#include <sys/time.h>
#include "../msr/msr.h"

/**
 * \brief The file for the logs of the current call
 */
static FILE *file_call = NULL;
/**
 * \brief The number of CGR's calls.
 */
unsigned int count_bundles = 1;
time_t current_time = MAX_POSIX_TIME;
unsigned long long localNode = 0;
/**
 * \brief The last time when the CGR discarded all routes.
 */
static struct timeval cgrEditTime = { -1, -1 };

#if (LOG == 1)

/******************************************************************************
 *
 * \par Function Name:
 *  	print_result_cgr
 *
 * \brief  Print some logs for the CGR's output
 *
 *
 * \par Date Written:
 *  	15/02/20
 *
 * \return void
 *
 * \param[in]   result         The getBestRoutes function result
 * \param[in]   bestRoute      The list of best routes
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  15/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static void print_result_cgr(int result, List bestRoutes)
{
	ListElt *elt;
	Route *route;

	if (result >= 0)
	{
		writeLog("Best routes found: %d.", result);

		if (bestRoutes != NULL)
		{
			for (elt = bestRoutes->first; elt != NULL; elt = elt->next)
			{
				route = (Route*) elt->data;
				writeLog("Used route to neighbor %llu.", route->neighbor);
			}
		}
	}
	else if (result == -1)
	{
		writeLog("0 routes found to the destination.");
	}

}

/******************************************************************************
 *
 * \par Function Name:
 *  	print_cgr_settings
 *
 * \brief  Print the most important settings of the CGR
 *
 *
 * \par Date Written:
 *  	15/02/20
 *
 * \return void
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  02/04/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static void print_cgr_settings()
{
#if (CGR_AVOID_LOOP == 0)
	writeLog("Anti-loop mechanism disabled.");
#elif (CGR_AVOID_LOOP == 1)
	writeLog("Anti-loop mechanism enabled (only reactive version).");
#elif(CGR_AVOID_LOOP == 2)
	writeLog("Anti-loop mechanism enabled (only proactive version).");
#elif(CGR_AVOID_LOOP == 3)
	writeLog("Anti-loop mechanism enabled (proactive and reactive version).");
#else
	writeLog("CGR_AVOID_LOOP: Unknown macro value.");
#endif

#if (MAX_DIJKSTRA_ROUTES == 0)
	writeLog("One route per neighbor enabled (without limits).");
#elif (MAX_DIJKSTRA_ROUTES == 1)
	writeLog("One route per neighbor disabled.");
#elif (MAX_DIJKSTRA_ROUTES > 1)
	writeLog("One route per neighbor enabled (at most %d neighbors).", MAX_DIJKSTRA_ROUTES);
#else
	writeLog("MAX_DIJKSTRA_ROUTES: Unknown macro value.");
#endif

#if (QUEUE_DELAY == 0)
	writeLog("ETO only on the first hop.");
#elif (QUEUE_DELAY == 1)
	writeLog("ETO on all hops.");
#else
	writeLog("QUEUE_DELAY: Unknown macro value.");
#endif

#if (NEGLECT_CONFIDENCE == 1)
	writeLog("Neglect confidence.");
#elif (NEGLECT_CONFIDENCE != 0)
	writeLog("NEGLECT_CONFIDENCE: Unknown macro value.");
#endif

#if (ADD_COMPUTED_ROUTE_TO_INTERMEDIATE_NODES == 1)
	writeLog("Add computed route to intermediate nodes enabled.");
#endif

#if (CCSDS_SABR_DEFAULTS == 1)
	writeLog("CCSDS SABR standard algorithm enabled.");
#endif

#if (CGR_UNIBO_SUGGESTED_SETTINGS == 1)
	writeLog("CGR Unibo suggested settings enabled.");
#endif

#if (CGR_ION_3_7_0 == 1)
	writeLog("ION-3.7.0 CGR implementation settings enabled.");
#endif

#if (MSR == 1)
	writeLog("Moderate Source Routing enabled.");
	writeLog("MSR time tolerance: %d s.", MSR_TIME_TOLERANCE);
#if (WISE_NODE == 0)
	writeLog("MSR hops lower bound: %d.", MSR_HOPS_LOWER_BOUND);
#endif

#endif

	log_fflush();

	return;
}

#else
#define print_result_cgr(result, bestRoutes) do { } while(0)
#define print_cgr_settings() do { } while(0)
#endif

/******************************************************************************
 *
 * \par Function Name:
 * 		initialize_cgr
 *
 * \brief Initialize all the structures used by the CGR.
 *
 *
 * \par Date Written:
 *  	15/02/20
 *
 * \return int
 *
 * \retval          1	Success case: CGR initialized
 * \retval         -1	Error case: ownNode can't be 0
 * \retval         -2	MWITHDRAW error
 * \retval         -3	Error case: log directory can't be opened
 * \retval         -4	Error case: log file can't be opened
 *
 * \param[in] time     The time "zero"
 * \param[in] ownNode  The local node, used as contacts graph's root
 *
 * \par Notes:
 *             1. If LOG is setted to 1 this function open the log directory,
 *                clean the log directory and open the log file
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  15/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
int initialize_cgr(time_t time, unsigned long long ownNode)
{
	int result = -1;

	if (ownNode > 0)
	{
		current_time = time;
#if (LOG == 1)
		result = createLogDir();
		if (result >= 0)
		{
			result = openLogFile();
			if (result == 1)
			{
				setLogTime(current_time);
#endif
				if (initialize_contact_plan() == 1)
				{
					if (initialize_phase_one(ownNode) == 1)
					{
						localNode = ownNode;
						result = initialize_phase_two();

						print_cgr_settings();
						writeLog("Own node: %llu.", ownNode);

#if (MSR == 1)
						if(initialize_msr() != 1)
						{
							result = -2;
						}
#endif
					}
					else
					{
						result = -2;
					}
				}
				else
				{
					result = -2;
				}
#if (LOG == 1)
				if (cleanLogDir() < 0)
				{
					file_call = NULL;
				}
			}
			else
			{
				result = -4;
			}
		}
		else
		{
			result = -3;
		}
#endif
	}
	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 * 		discardAllRoutes
 *
 * \brief Discard all the previous computed routes for all ipn nodes.
 *
 * \details You should call this function every time you recognize
 *          a changes to the contact plan.
 *
 *
 * \par Date Written:
 * 		15/02/20
 *
 * \return void
 *
 * \par Notes:
 *          1. This function remove all the Node(s) and Neighbor(s)
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  15/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static void discardAllRoutes()
{
//	discardAllRoutesFromContactsGraph();
//	discardAllRoutesFromNodesTree();
	reset_NodesTree();
}

/******************************************************************************
 *
 * \par Function Name:
 * 		destroy_cgr
 *
 * \brief Deallocate all memory previously allocated by the CGR.
 *
 *
 * \par Date Written:
 *  	15/02/20
 *
 * \return void
 *
 * \param[in]  time   Used for logs
 *
 * \par Notes:
 *             1.  If LOG is setted to 1 this function close the log file.
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  15/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
void destroy_cgr(time_t time)
{
	current_time = time;
	setLogTime(current_time);
	destroy_contact_plan();
	destroy_phase_one();
	destroy_phase_two();

#if (MSR == 1)
	destroy_msr();
#endif

	writeLog("Shutdown.");
	closeLogFile();

	// reset to initial values all data

	file_call = NULL;
	localNode = 0;
	count_bundles = 1;

	cgrEditTime.tv_sec = -1;
	cgrEditTime.tv_usec = -1;
	current_time = MAX_POSIX_TIME;

}

/******************************************************************************
 *
 * \par Function Name:
 * 		parse_excluded_nodes
 *
 * \brief Remove from the list all duplicate nodes
 *
 *
 * \par Date Written:
 *  	30/04/20
 *
 * \return void
 *
 * \param[in]  excludedNodes    The list with the excluded neighbors
 *
 * \par Notes:
 *             1.  In CGR Unibo a neighbor is a node for which the local node
 *                 has at least one opportunity of transmission (contact).
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  30/04/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static void parse_excluded_nodes(List excludedNodes)
{
	ListElt *main_elt, *next_main;
	ListElt *current_elt, *next_current;
	unsigned long long *main_node;
	unsigned long long *current_node;


	main_elt = excludedNodes->first;
	while(main_elt != NULL)
	{
		next_main = main_elt->next;
		if(main_elt->data != NULL)
		{
			main_node = (unsigned long long *) main_elt->data;

			current_elt = main_elt->next;

			while (current_elt != NULL)
			{
				next_current = current_elt->next;

				if (current_elt->data != NULL)
				{
					current_node = (unsigned long long*) current_elt->data;

					if (*current_node == *main_node)
					{
						list_remove_elt(current_elt);
					}

				}

				current_elt = next_current;
			}
		}
		else
		{
			list_remove_elt(main_elt);
		}

		main_elt = next_main;
	}

	return;
}

/******************************************************************************
 *
 * \par Function Name:
 * 		reset_cgr
 *
 * \brief  Reset the CGR: all the structures used by the 3 phases
 *         will be cleaned but not the contact plan.
 *
 * \details   This function should be used every time you receive
 *            a new call for the CGR.
 *
 *
 * \par Date Written:
 *      15/02/20
 *
 * \return void
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  15/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static void reset_cgr()
{
	reset_phase_one();
	reset_phase_two();
	reset_neighbors_temporary_fields();
}

/******************************************************************************
 *
 * \par Function Name:
 * 		clear_rtg_object
 *
 * \brief  This function cleans the RtgObject from the temporary
 *         values used in this call.
 *
 *
 * \par Date Written:
 * 		15/02/20
 *
 * \return void
 *
 * \param[in]	*rtgObj  The RtgObject to clean
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  15/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static void clear_rtg_object(RtgObject *rtgObj)
{
	ListElt *elt;
	Route *route;

	if (rtgObj != NULL)
	{
		for (elt = rtgObj->selectedRoutes->first; elt != NULL; elt = elt->next)
		{
			route = (Route*) elt->data;
			route->checkValue = 0;
#if (LOG == 1)
			route->num = 0;
#endif
		}

	}

	return;
}

/******************************************************************************
 *
 * \par Function Name:
 * 		is_initialized_terminus_node
 *
 * \brief Boolean, to know if the Node has some field that points to NULL
 *
 *
 * \par Date Written:
 * 		30/01/20
 *
 * \return int
 *
 * \retval  1	The Node is initialized and can be used
 * \retval  0	The Node isn't initialized correctly
 *
 * \param[in]   *terminusNode   The Node for which we want to know if is initialized
 *
 * \par Revision History:
 *
 *  DD/MM/YY | AUTHOR          |   DESCRIPTION
 *  -------- | --------------- |  -----------------------------------------------
 *  30/01/20 | L. Persampieri  |   Initial Implementation and documentation.
 *****************************************************************************/
static int is_initialized_terminus_node(Node *terminusNode)
{
	int result = 0;
	RtgObject *rtgObj;
	if (terminusNode != NULL)
	{
		rtgObj = terminusNode->routingObject;

		if (rtgObj != NULL)
		{
			if (rtgObj->knownRoutes != NULL && rtgObj->selectedRoutes != NULL && rtgObj->citations != NULL)
			{
				result = 1;
			}
		}
	}

	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 * 		excludeNeighbor
 *
 * \brief Insert a neighbor in the excluded neighbors list
 *
 *
 * \par Date Written:
 * 		15/02/20
 *
 * \return int
 *
 * \retval    0   Success case: neighbor inserted in the excluded neighbors list
 * \retval   -2   MWITHDRAW error
 *
 * \param[in]	excludedNeighbors   The excluded neighbors list
 * \param[in]	neighbor            The ipn node of the neighbor that we want to exclude
 *
 * \warning excludedNeighbors doesn't have to be NULL
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  15/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static int excludeNeighbor(List excludedNeighbors, unsigned long long neighbor)
{
	unsigned long long *temp = (unsigned long long*) MWITHDRAW(sizeof(unsigned long long));
	int result = -2;
	if (temp != NULL)
	{
		*temp = neighbor;

		if (neighbor == 0 || list_insert_last(excludedNeighbors, temp) != NULL)
		{
			result = 0;
		}
		else
		{
			MDEPOSIT(temp);
		}
	}

	return result;
}

/******************************************************************************
 *
 * \par Function Name: executeCGR
 *
 * \brief Implementation of the CGR, call the 3 phases to choose
 *        the best routes for the forwarding of the bundle.
 *
 *
 * \par Date Written:
 * 		15/02/20
 *
 * \return int
 *
 * \retval      ">= 0"  Success case: number of best routes found
 * \retval         -1   There aren't routes to reach the destination.
 * \retval         -2   MWITHDRAW error
 * \retval         -3   Arguments error (phase one error)
 *
 * \param[in]   *bundle            The bundle that has to be forwarded
 * \param[in]   *terminusNode      The destination Node of the bundel
 * \param[in]   excludedNeighbors  The excluded neighbors list, the nodes to which
 *                                 the bundle hasn't to be forwarded as "first hop"
 * \param[out]  *bestRoutes        If result > 0: the list of best routes, NULL otherwise
 *
 * \warning bundle doesn't have to be NULL
 * \warning terminusNode doesn't have to be NULL
 * \warning excludedNeighbors doesn't have to be NULL
 * \warning excludedNeighbor doesn't have to be NULL
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  15/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static int executeCGR(CgrBundle *bundle, Node *terminusNode, List excludedNeighbors,
		List *bestRoutes)
{
	int result = 0, stop = 0;
	long unsigned int missingNeighbors = 0;
	List candidateRoutes = NULL;
	List subsetComputedRoutes = NULL;
	RtgObject *rtgObj = terminusNode->routingObject;

	reset_cgr();

	if (!(ALREADY_COMPUTED(rtgObj))) //If it's the first time that I compute routes for this destination
	{
		if(NEIGHBORS_DISCOVERED(rtgObj))
		{
			missingNeighbors = rtgObj->citations->length;
		}
		else
		{
			missingNeighbors = get_local_node_neighbors_count();
		}
#if (MAX_DIJKSTRA_ROUTES > 0)
		if(!IS_CRITICAL(bundle) && missingNeighbors > MAX_DIJKSTRA_ROUTES)
		{
			missingNeighbors = MAX_DIJKSTRA_ROUTES;
		}
#endif
		if(missingNeighbors > 0)
		{
			result = computeRoutes(terminusNode, NULL, missingNeighbors); //phase one
			stop = (result <= 0) ? 1 : 0;
		}
		else
		{
			stop = 1;
		}
	}

	if((NEIGHBORS_DISCOVERED(rtgObj) && rtgObj->citations->length == 0)
			|| get_local_node_neighbors_count() == 0)
	{
		// 0 neighbors to reach destination...
		stop = 1;
	}

	while (!stop)
	{
		result = getCandidateRoutes(terminusNode, bundle, excludedNeighbors, rtgObj->selectedRoutes, &subsetComputedRoutes, &missingNeighbors, &candidateRoutes); //phase two

		if (result != 0 || missingNeighbors == 0)
		{
			stop = 1;
		}
		if (!stop)
		{
			result = computeRoutes(terminusNode, subsetComputedRoutes, missingNeighbors); //phase one

			stop = (result <= 0) ? 1 : 0;
		}
	}

	print_phase_one_routes(file_call, rtgObj->selectedRoutes);
	print_phase_two_routes(file_call, candidateRoutes);
	*bestRoutes = NULL;

	if (result >= 0 && candidateRoutes != NULL && candidateRoutes->length > 0)
	{
		result = chooseBestRoutes(bundle, candidateRoutes); //phase three
		*bestRoutes = candidateRoutes;
	}

	print_phase_three_routes(file_call, *bestRoutes);

	clear_rtg_object(rtgObj); //clear the temporary values

	debug_printf("result -> %d", result);

	return result;

}

/******************************************************************************
 *
 * \par Function Name:
 *  	getBestRoutes
 *
 * \brief	 Implementation of the CGR, get the best routes list.
 *
 *
 * \par Date Written:
 *  	15/02/20
 *
 * \return int
 *
 * \retval      ">= 0"  Success case: number of best routes found
 * \retval         -1   There aren't routes to reach the destination.
 * \retval         -2   MWITHDRAW error
 * \retval         -3   Phase one error (phase one's arguments error)
 * \retval         -4   Arguments error
 * \retval         -5   Time is in the past
 *
 * \param[in]   time                 The current time
 * \param[in]   *bundle              The bundle that has to be forwarded
 * \param[in]   excludedNeighbors    The excluded neighbors list, the nodes to which
 *                                   the bundle hasn't to be forwarded as "first hop"
 * \param[out]  *bestRoutes          If result > 0: the list of best routes, NULL otherwise
 *
 * \par Notes:
 *          1.  If the contact plan changed this function discards
 *              all the routes computed by the previous calls.
 *          2.  You must initialize the CGR before to call this function.
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  15/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
int getBestRoutes(time_t time, CgrBundle *bundle, List excludedNeighbors, List *bestRoutes)
{
	int result = -4;
	Node *terminusNode;

	setLogTime(time);

	if (bundle != NULL && excludedNeighbors != NULL && bestRoutes != NULL)
	{
		*bestRoutes = NULL;
		debug_printf("Call n.: %u", count_bundles);
		writeLog("Destination node: %llu.", bundle->terminus_node);
		if (check_bundle(bundle) != 0)
		{
			writeLog("Bundle bad formed.");
			result = -4;
		}
		else if (bundle->expiration_time < time) //bundle expired
		{
			result = 0;
			writeLog("Bundle expired.");
		}
		else if(time < current_time)
		{
			result = -5;
			writeLog("Error, time (%ld s) is in the past (last time: %ld s)", time, current_time);
		}
		else
		{

			result = 0;

			if (contactPlanEditTime.tv_sec > cgrEditTime.tv_sec
					|| (cgrEditTime.tv_sec == contactPlanEditTime.tv_sec
							&& contactPlanEditTime.tv_usec > cgrEditTime.tv_usec))
			{
				if (cgrEditTime.tv_sec != -1)
				{
					writeLog("Contact plan modified, all routes will be discarded.");
					discardAllRoutes();
				}
				cgrEditTime.tv_sec = contactPlanEditTime.tv_sec;
				cgrEditTime.tv_usec = contactPlanEditTime.tv_usec;

				if(build_local_node_neighbors_list(localNode) < 0)
				{
					result = -2;
					verbose_debug_printf("Error...");
				}
			}

			if(result == 0)
			{

				current_time = time;

				removeExpired(current_time);

				terminusNode = add_node(bundle->terminus_node);

				if(!is_initialized_terminus_node(terminusNode))
				{
					// Some error in the "Node tree" management
					terminusNode = NULL;
				}

				result = 0;

#if (CGR_AVOID_LOOP == 1 || CGR_AVOID_LOOP == 3)
				result = set_failed_neighbors_list(bundle, localNode);
#endif
				if (result >= 0 && !(RETURN_TO_SENDER(bundle)) && bundle->sender_node != 0)
				{
					result = excludeNeighbor(excludedNeighbors, bundle->sender_node);
				}

				parse_excluded_nodes(excludedNeighbors);

#if (LOG == 1)
				file_call = openBundleFile(count_bundles);
				print_bundle(file_call, bundle, excludedNeighbors, current_time);
#endif

				if (terminusNode != NULL && result >= 0)
				{
#if (MSR == 1)
					result = tryMSR(bundle, excludedNeighbors, file_call, bestRoutes);
					if(result <= 0 && result != -2)
					{
#endif
						result = executeCGR(bundle, terminusNode, excludedNeighbors, bestRoutes);
#if (MSR == 1)
					}
#endif
				}
				else
				{
					result = -2;
				}

				closeBundleFile(&file_call);
			}
		}

		if(result == -1)
		{
			writeLog("0 routes found to destination.");
		}
		else if(result == 0)
		{
			writeLog("Best routes found: 0.");
		}
		else if(result > 0)
		{
			print_result_cgr(result, *bestRoutes);
		}
	}

	debug_printf("result -> %d", result);

	count_bundles++;

	return result;
}
