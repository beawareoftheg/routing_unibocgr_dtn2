/** \file contactPlan.c
 *
 *  \brief This file provides the implementation of the functions
 *         to manage the contact plan
 *
 ** \copyright Copyright (c) 2020, Alma Mater Studiorum, University of Bologna, All rights reserved.
 *
 *  \author Lorenzo Persampieri, lorenzo.persampieri@studio.unibo.it
 *
 *  \par Supervisor
 *       Carlo Caini, carlo.caini@unibo.it
 */

#include "contactPlan.h"
#include "nodes/nodes.h"
#include "contacts/contacts.h"
#include "ranges/ranges.h"

/**
 * \brief Boolean: 0 if the contacts graph hasn't been initialized, 1 otherwise.
 */
static int contactsGraph = 0;
/**
 * \brief Boolean: 0 if the ranges graph hasn't been initialized, 1 otherwise.
 */
static int rangesGraph = 0;
/**
 * \brief Boolean: 0 if the nodes tree hasn't been initialized, 1 otherwise.
 */
static int nodes = 0;
/**
 * \brief Boolean: 0 if all the main structures hasn't been initialized, 1 otherwise.
 */
static int initialized = 0;
struct timeval contactPlanEditTime = { -1, -1 };

/******************************************************************************
 *
 * \par Function Name:
 *      initialize_contact_plan
 *
 * \brief Initialize the structures to manage the contact plan
 *
 *
 * \par Date Written:
 *      23/01/20
 *
 * \return int
 *
 * \retval   1   Success case:	Contacts graph, ranges graph and nodes tree initialized correctly
 * \retval  -2   Error case:  MWITHDRAW error
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY | AUTHOR         |   DESCRIPTION
 *  -------- | ---------------|  -----------------------------------------------
 *  23/01/20 | L. Persampieri |   Initial Implementation and documentation.
 *****************************************************************************/
int initialize_contact_plan()
{
	int result;

	result = 1;
	if (contactsGraph == 0)
	{
		if (create_ContactsGraph() == 1)
		{
			contactsGraph = 1;
		}
		else
		{
			result = -2;
		}
	}
	if (rangesGraph == 0)
	{
		if (create_RangesGraph() == 1)
		{
			rangesGraph = 1;
		}
		else
		{
			result = -2;
		}
	}
	if (nodes == 0)
	{
		if (create_NodesTree() == 1)
		{
			nodes = 1;
		}
		else
		{
			result = -2;
		}
	}

	if (result == 1)
	{
		initialized = 1;
	}

	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 *      removeExpired
 *
 * \brief Remove the expired contacts ,ranges (toTime field less than time)
 *        and neighbors to whom whe haven't other contacts.
 *
 *
 * \par Date Written:
 *      23/01/20
 *
 * \return void
 *
 * \param[in]	time   The time used to know who are the expired contacts, ranges and "neighbors"
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY | AUTHOR         |   DESCRIPTION
 *  -------- | ---------------|  -----------------------------------------------
 *  23/01/20 | L. Persampieri |   Initial Implementation and documentation.
 *****************************************************************************/
void removeExpired(time_t time)
{
	if (initialized)
	{
		removeExpiredContacts(time);
		removeExpiredRanges(time);
		removeOldNeighbors(time);
	}
}

/******************************************************************************
 *
 * \par Function Name:
 *      addContact
 *
 * \brief Add a contact to the contacts graph
 *
 *
 * \par Date Written:
 *      23/01/20
 *
 * \return int
 *
 * \retval   2  Success case:   Revised contact's xmit rate (and MTVs)
 * \retval   1  Success case:	The contact now is inside the contacts graph
 * \retval   0  Arguments error case:	The contact cannot be inserted with this arguments
 * \retval  -1  The contact cannot be inserted because it overlaps some other contacts.
 * \retval  -2  MWITHDRAW error
 *
 * \param[in]	fromNode    The contact's sender node
 * \param[in]	toNode      The contact's receiver node
 * \param[in]	fromTime    The contact's start time
 * \param[in]	toTime      The contact's end time
 * \param[in]	xmitRate    In bytes per second
 * \param[in]	confidence  The confidence that the contact will effectively materialize
 * \param[in]   copyMTV      Set to 1 if you want to copy the MTV from mtv input parameter.
 *                           Set to 0 otherwise
 * \param[in]   mtv          The contact's MTV: this must be an array of 3 elements.
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY | AUTHOR         |   DESCRIPTION
 *  -------- | ---------------|  -----------------------------------------------
 *  23/01/20 | L. Persampieri |   Initial Implementation and documentation.
 *****************************************************************************/
int addContact(unsigned long long fromNode, unsigned long long toNode, time_t fromTime,
		time_t toTime, long unsigned int xmitRate, float confidence, int copyMTV, double mtv[])
{
	int result = 0;

	if (!initialized)
	{
		if (initialize_contact_plan() == -1)
		{
			result = -2;
		}
	}

	if (initialized)
	{
		result = add_contact_to_graph(fromNode, toNode, fromTime, toTime, xmitRate, confidence, copyMTV, mtv);
	}

	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 *      removeContact
 *
 * \brief Remove a contact from the contacts graph
 *
 *
 * \par Date Written:
 *      23/01/20
 *
 * \return int
 *
 * \retval   1  If the contact plan is initialized
 * \retval   0  If the contact plan isn't initialized
 *
 * \param[in]   fromNode    The contact's sender node
 * \param[in]   toNode      The contact's receiver node
 * \param[in]   *fromTime   The contact's start time, if NULL all contacts with fields
 *                          {fromNode, toNode} will be deleted
 *
 * \par Notes:
 *          1. The free_contact will be called for the contact(s) removed (routes discarded).
 *
 * \par Revision History:
 *
 *  DD/MM/YY | AUTHOR         |   DESCRIPTION
 *  -------- | ---------------|  -----------------------------------------------
 *  23/01/20 | L. Persampieri |   Initial Implementation and documentation.
 *****************************************************************************/
int removeContact(unsigned long long fromNode, unsigned long long toNode, time_t *fromTime)
{
	int result = 0;

	if (initialized)
	{
		remove_contact_from_graph(fromTime, fromNode, toNode);
		result = 1;
	}

	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 *      addRange
 *
 * \brief Add a range to the ranges graph
 *
 *
 * \par Date Written:
 *      23/01/20
 *
 * \return int
 *
 * \retval   1   Success case:	The range now is inside the contacts graph
 * \retval   0   Arguments error case:	The range cannot be inserted with this arguments
 * \retval  -1   The range cannot be inserted because it overlaps some other ranges.
 * \retval  -2   MWITHDRAW error
 *
 * \param[in]	fromNode   The contact's sender node
 * \param[in]	toNode     The contact's receiver node
 * \param[in]	fromTime   The contact's start time
 * \param[in]	toTime     The contact's end time
 * \param[in]	owlt       The distance from the sender node to the receiver node in light time
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY | AUTHOR         |   DESCRIPTION
 *  -------- | ---------------|  -----------------------------------------------
 *  23/01/20 | L. Persampieri |   Initial Implementation and documentation.
 *****************************************************************************/
int addRange(unsigned long long fromNode, unsigned long long toNode, time_t fromTime, time_t toTime,
		unsigned int owlt)
{
	int result = 0;

	if (!initialized)
	{
		if (initialize_contact_plan() == -1)
		{
			result = -2;
		}
	}

	if (initialized)
	{
		result = add_range_to_graph(fromNode, toNode, fromTime, toTime, owlt);

	}

	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 *      removeRange
 *
 * \brief Remove a range from the ranges graph
 *
 *
 * \par Date Written:
 *      23/01/20
 *
 * \return int
 *
 * \retval   1   If the contact plan is initialized
 * \retval   0   If the contact plan isn't initialized
 *
 * \param[in]	fromNode   The range's sender node
 * \param[in]	toNode     The range's receiver node
 * \param[in]	*fromTime  The range's start time, if NULL all ranges with fields
 *                         {fromNode, toNode} will be deleted
 *
 * \par Notes:
 *           1. The free_range will be called for the range(s) removed.
 *
 * \par Revision History:
 *
 *  DD/MM/YY | AUTHOR         |   DESCRIPTION
 *  -------- | ---------------|  -----------------------------------------------
 *  23/01/20 | L. Persampieri |   Initial Implementation and documentation.
 *****************************************************************************/
int removeRange(unsigned long long fromNode, unsigned long long toNode, time_t *fromTime)
{
	int result = 0;

	if (initialized)
	{
		remove_range_from_graph(fromTime, fromNode, toNode);
		result = 1;
	}

	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 *      reset_contact_plan
 *
 * \brief Delete all the contacts, ranges and nodes (but not the graphs)
 *
 *
 * \par Date Written:
 *      23/01/20
 *
 * \return void
 *
 *
 * \par Notes:
 *           1. All routes will be discarded
 *
 * \par Revision History:
 *
 *  DD/MM/YY | AUTHOR         |   DESCRIPTION
 *  -------- | ---------------|  -----------------------------------------------
 *  23/01/20 | L. Persampieri |   Initial Implementation and documentation.
 *****************************************************************************/
void reset_contact_plan()
{
	reset_NodesTree();
	reset_RangesGraph();
	reset_ContactsGraph();

	return;
}

/******************************************************************************
 *
 * \par Function Name:
 *      destroy_contact_plan
 *
 * \brief Delete all the contacts, ranges and nodes (and the graphs)
 *
 *
 * \par Date Written:
 *      23/01/20
 *
 * \return void
 *
 *
 * \par Notes:
 *           1. All routes will be discarded
 *
 * \par Revision History:
 *
 *  DD/MM/YY | AUTHOR         |   DESCRIPTION
 *  -------- | ---------------|  -----------------------------------------------
 *  23/01/20 | L. Persampieri |   Initial Implementation and documentation.
 *****************************************************************************/
void destroy_contact_plan()
{
	destroy_NodesTree();
	destroy_RangesGraph();
	destroy_ContactsGraph();

	initialized = 0;
	contactsGraph = 0;
	rangesGraph = 0;
	nodes = 0;

	contactPlanEditTime.tv_sec = -1;
	contactPlanEditTime.tv_usec = -1;

	return;
}

