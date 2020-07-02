/** \file msr_utils.c
 *
 *  \brief  This file provides the implementation of some utility function to manage routes
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
#include "msr_utils.h"

/**
 * \brief Get the absolute value of "a"
 *
 * \param[in]   a   The real number for which we want to know the absolute value
 *
 * \hideinitializer
 */
#define absolute(a) (((a) < 0) ? (-(a)) : (a))

/******************************************************************************
 *
 * \par Function Name:
 * 		populate_msr_route
 *
 * \brief Build a Route from the last contact to the first.
 *
 * \details This function should be used only to build routes that will be used by MSR.
 *
 * \par Date Written:
 * 		23/04/20
 *
 * \return int
 *
 * \retval  0    Success case: Route has been builded correctly
 * \retval -1    Arguments error
 * \retval -2    MWITHDRAW error
 *
 * \param[in]     finalContact   The last contact of the route
 * \param[out]    resultRoute    The Route just builded in success case. This field must be
 *                               allocated by the caller.
 *
 * \par Revision History:
 *
 *  DD/MM/YY | AUTHOR          |   DESCRIPTION
 *  -------- | --------------- |  -----------------------------------------------
 *  23/04/20 | L. Persampieri  |   Initial Implementation and documentation.
 *****************************************************************************/
int populate_msr_route(Contact *finalContact, Route *resultRoute)
{
	int result = -1;
	time_t earliestEndTime;
	Contact *contact, *firstContact = NULL;
	ContactNote *current_work;
	ListElt *elt;

	if(finalContact != NULL && resultRoute != NULL)
	{
		result = 0;

		resultRoute->arrivalConfidence = finalContact->routingObject->arrivalConfidence;
		resultRoute->computedAtTime = current_time;

		earliestEndTime = MAX_POSIX_TIME;
		contact = finalContact;

		while (contact != NULL)
		{
			current_work = contact->routingObject;
			if (contact->toTime < earliestEndTime)
			{
				earliestEndTime = contact->toTime;
			}

			elt = list_insert_first(resultRoute->hops, contact);

			if (elt == NULL)
			{
				result = -2;
				contact = NULL; //I leave the loop
			}
			else
			{
				firstContact = contact;
				contact = current_work->predecessor;
			}
		}

		if (result == 0)
		{
			resultRoute->neighbor = firstContact->toNode;
			resultRoute->fromTime = firstContact->fromTime;
			resultRoute->toTime = earliestEndTime;
		}
	}

	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 * 		get_msr_contact
 *
 * \brief Search a contact into contact graph. The contact has to match with
 *        the id {fromNode, toNode, fromTime}.
 *
 * \details contact->fromTime can be included in
 *          fromTime - MSR_TIME_TOLERANCE <= contact->fromTime <= fromTime + MSR_TIME_TOLERANCE
 *
 * \par Date Written:
 * 		23/04/20
 *
 * \return Contact*
 *
 * \retval  Contact*   The contact found
 * \retval  NULL       Contact not found
 *
 * \param[in]      fromNode     The sender node of the contact
 * \param[in]      toNode       The receiver node of the contact
 * \param[in]      fromTime     The start time of the contact
 *
 * \par Revision History:
 *
 *  DD/MM/YY | AUTHOR          |   DESCRIPTION
 *  -------- | --------------- |  -----------------------------------------------
 *  23/04/20 | L. Persampieri  |   Initial Implementation and documentation.
 *****************************************************************************/
Contact * get_msr_contact(unsigned long long fromNode, unsigned long long toNode, time_t fromTime)
{
	Contact *result = NULL;
	Contact *current;
	RbtNode *node;
	int stop = 0;
	time_t difference;

	for(current = get_first_contact_from_node_to_node(fromNode, toNode, &node);
			current != NULL && !stop; current = get_next_contact(&node))
	{
		if(current->fromNode == fromNode && current->toNode == toNode)
		{
			// difference in absolute value
			difference = absolute(current->fromTime - fromTime);

			if(difference <= MSR_TIME_TOLERANCE)
			{
				result = current;
				stop = 1;
			}
			else if(current->fromTime > fromTime + MSR_TIME_TOLERANCE)
			{
				// not found
				stop = 1;
			}
		}
		else
		{
			// not found
			stop = 1;
		}
	}

	return result;
}



#if (CGRR == 1 && MSR == 1)

/******************************************************************************
 *
 * \par Function Name:
 * 		build_msr_route
 *
 * \brief Convert a CGRRoute into Route and attach it to bundle.
 *
 * \par Date Written:
 * 		23/04/20
 *
 * \return int
 *
 * \retval  0   CGRRoute converted correctly in Route and attached it to bundle
 * \retval -1   The Route can't be builded
 * \retval -2   MWITHDRAW error
 * \retval -3   Arguments error
 *
 * \param[in]      current_time   The differencial time from CGR's start
 * \param[in]      cgrrRoute      The CGRRoute that we want to convert in Route
 * \param[in,out]  bundle         The bundle that at the end will contains the builded Route
 *
 * \par Revision History:
 *
 *  DD/MM/YY | AUTHOR          |   DESCRIPTION
 *  -------- | --------------- |  -----------------------------------------------
 *  23/04/20 | L. Persampieri  |   Initial Implementation and documentation.
 *****************************************************************************/
static int build_msr_route(time_t current_time, CGRRoute* cgrrRoute, CgrBundle *bundle)
{
	int result = -1;
	int stop = 0;
	unsigned int count;
	unsigned int localNodePosition, i;
	unsigned long long prevToNode;
	Route *newRoute;
	Contact *contact = NULL, *prevContact;

	if(cgrrRoute == NULL || bundle == NULL || current_time < 0)
	{
		return -3;
	}

	localNodePosition =  0;
	stop = 0;
	for(i = 0; i < cgrrRoute->hopCount && !stop; i++)
	{
		if(cgrrRoute->hopList[i].fromNode == localNode)
		{
			localNodePosition = i;
			stop = 1;
		}
	}
	if (stop)
	{
		newRoute = create_cgr_route();
		stop = 0;
		prevContact = NULL;
		count = 0;
		prevToNode = localNode;

		for (i = localNodePosition; i < cgrrRoute->hopCount && !stop; i++)
		{
			contact = get_msr_contact(cgrrRoute->hopList[i].fromNode,
					cgrrRoute->hopList[i].toNode,
					cgrrRoute->hopList[i].fromTime);

			if (contact != NULL && contact->toTime > current_time)
			{
				if(prevToNode == contact->fromNode &&
						((prevToNode != contact->toNode ) ||
								(count == 0 && bundle->terminus_node == localNode)))
				{
					prevToNode = contact->toNode;
					count++;
					contact->routingObject->predecessor = prevContact;
					if(prevContact != NULL)
					{
						contact->routingObject->arrivalConfidence =
							contact->confidence*prevContact->routingObject->arrivalConfidence;
					}
					else
					{
						contact->routingObject->arrivalConfidence = contact->confidence;
					}
					prevContact = contact;
				}
				else
				{
					stop = 1;
					verbose_debug_printf("MSR: malformed route...");
				}
			}
			else
			{
#if (WISE_NODE == 1)
				stop = 1;
#else
				if(count < MSR_HOPS_LOWER_BOUND)
				{
					// Lower bound not reached.
					stop = 1;
				}
				else
				{
					if(contact->toTime <= current_time)
					{
						contact = prevContact;
					}
					stop = 2;
				}
#endif
			}
		}

		if(!stop && (bundle->terminus_node != prevToNode))
		{
			stop = 1;
			verbose_debug_printf("MSR: malformed route...");
			verbose_debug_printf("prevToNode: %llu, destination: %llu", prevToNode, bundle->terminus_node);
		}

		if(stop == 1)
		{
			delete_msr_route(newRoute);
			result = -1;
		}
		else
		{
			if(populate_msr_route(contact, newRoute) < 0)
			{
				result = -2;
				delete_msr_route(newRoute);
			}
			else
			{
				// Success case
				result = 0;
				bundle->msrRoute = newRoute;
			}
		}
	}

	return result;

}

/******************************************************************************
 *
 * \par Function Name:
 * 		set_msr_route
 *
 * \brief Get the last route from CGRRoute, convert it in Route format and attach it to bundle.
 *
 * \par Date Written:
 * 		23/04/20
 *
 * \return int
 *
 * \retval   0   CGRRoute converted in Route and attached to bundle
 * \retval  -1   CGRRoute can't be converted in Route, or CGRRoute not found
 * \retval  -2   MWITHDRAW error
 * \retval  -3   Arguments error
 *
 * \param[in]      current_time   The differencial time from CGR's start
 * \param[in]      cgrrBlk        The CGRR Extension Block that contains all the routes previously computed
 *                                by some ipn node.
 * \param[in,out]  bundle         The bundle that at the end will contains the builded Route
 *
 * \par Revision History:
 *
 *  DD/MM/YY | AUTHOR          |   DESCRIPTION
 *  -------- | --------------- |  -----------------------------------------------
 *  23/04/20 | L. Persampieri  |   Initial Implementation and documentation.
 *****************************************************************************/
int set_msr_route(time_t current_time, CGRRouteBlock *cgrrBlk, CgrBundle *bundle)
{
	CGRRoute *cgrrRoute;
	int result = -3;

	if(cgrrBlk != NULL && bundle != NULL)
	{
		if(cgrrBlk->recRoutesLength > 0)
		{
			cgrrRoute = &(cgrrBlk->recomputedRoutes[cgrrBlk->recRoutesLength - 1]);
		}
		else
		{
			cgrrRoute = &(cgrrBlk->originalRoute);
		}

		if(cgrrRoute != NULL)
		{
			result = build_msr_route(current_time, cgrrRoute, bundle);
		}
		else
		{
			result = -1;
		}
	}

	return result;
}

#endif

/******************************************************************************
 *
 * \par Function Name:
 * 		delete_msr_route
 *
 * \brief Delete a route previously builded by populate_msr_route() function.
 *
 * \par Date Written:
 * 		23/04/20
 *
 * \return void
 *
 * \param[in]     route   The route to destroy
 *
 * \par Revision History:
 *
 *  DD/MM/YY | AUTHOR          |   DESCRIPTION
 *  -------- | --------------- |  -----------------------------------------------
 *  23/04/20 | L. Persampieri  |   Initial Implementation and documentation.
 *****************************************************************************/
void delete_msr_route(Route *route)
{
	if(route != NULL)
	{
		if(route->hops != NULL)
		{
			route->hops->delete_data_elt = NULL;
			route->hops->delete_userData = NULL;
			free_list(route->hops);
		}
		if(route->children != NULL)
		{
			route->children->delete_data_elt = NULL;
			route->children->delete_userData = NULL;
			free_list(route->children);
		}

		memset(route,0,sizeof(Route));
		MDEPOSIT(route);
	}
}

