/** \file contacts.c
 *
 *  \brief   This file provides the implementation of the functions
 *           to manage the contact graph.
 *
 *
 ** \copyright Copyright (c) 2020, Alma Mater Studiorum, University of Bologna, All rights reserved.
 *
 *  \author Lorenzo Persampieri, lorenzo.persampieri@studio.unibo.it
 *
 *  \par Supervisor
 *       Carlo Caini, carlo.caini@unibo.it
 */

#include <stdlib.h>
#include "contacts.h"
#include "../../library/list/list.h"
#include "../../library/commonDefines.h"
#include "../../ported_from_ion/rbt/rbt.h"
#include "../../routes/routes.h"

#ifndef ADD_AND_REVISE_CONTACT
/**
 * \brief Boolean: set to 1 if you want to enable the behavior of REVISE_XMIT_RATE and REVISE_CONFIDENCE
 *                 even when you call the add_contact_to_graph() function. Otherwise set to 0.
 *
 * \details This macro keep the normal behavior of REVISE_XMIT_RATE and REVISE_CONFIDENCE,
 *          so if they (or just one) aren't enabled they will not be revised.
 *
 * \par Notes:
 *          1. You can find this macro helpful when you copy the contacts graph from
 *             an interface for a BP implementation.
 *          2. If you read the contacts graph from a file with an instruction "add contact"
 *             maybe you prefer to disable this macro and instead add a "revise contact" instruction.
 *          3. Just to avoid disambiguity: you can revise a contact only if you're adding
 *             a contact with the same {fromNode, toNode, fromTime}.
 *
 * \hideinitializer
 */
#define ADD_AND_REVISE_CONTACT 1
#endif

/**
 * \brief The contact graph.
 */
static Rbt *contacts = NULL;

static void erase_contact(Contact*);

static void erase_contact_note(ContactNote *note);
static ContactNote* create_contact_note();

/**
 * \brief The time of the next contact that expires.
 */
static time_t timeContactToRemove = MAX_POSIX_TIME;

/******************************************************************************
 *
 * \par Function Name:
 *      create_ContactsGraph
 *
 * \brief  Allocate memory for the contacts graph structure (rbt)
 *
 *
 * \par Date Written:
 *      13/01/20
 *
 * \return int
 *
 * \retval   1   Success case: contacts graph created
 * \retval  -2   Error case: contacts graph cannot be created due to MWITHDRAW error
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  13/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
int create_ContactsGraph()
{
	int result = 1;
	if (contacts == NULL)
	{
		contacts = rbt_create(free_contact, compare_contacts);

		if (contacts != NULL)
		{
			result = 1;
		}
		else
		{
			result = -2;
		}
	}

	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 *      discardAllRoutesFromContactsGraph
 *
 * \brief Delete all the citations for every contact that belongs to the contacts graph
 *
 *
 * \par Date Written:
 *      13/01/20
 *
 * \return void
 *
 * \par Notes:
 *              1. This function doesn't delete any route.
 *              2. If you call this function you must call even the correspective function for the nodes tree.
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  13/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
void discardAllRoutesFromContactsGraph()
{
	Contact *current;
	RbtNode *node;
	delete_function delete_fn;

	for (current = get_first_contact(&node); current != NULL; current = get_next_contact(&node))
	{
		delete_fn = current->citations->delete_data_elt;
		current->citations->delete_data_elt = NULL;
		free_list_elts(current->citations);
		current->citations->delete_data_elt = delete_fn;

	}

	return;
}

/******************************************************************************
 *
 * \par Function Name:
 *      removeExpiredContacts
 *
 * \brief  Delete all the contacts that have a toTime field <= than the time passed as argument.
 *
 * \par Date Written:
 *      13/01/20
 *
 * \return void
 *
 * \param[in]  time  The time used to know who are the expired contacts.
 *
 * \par Notes:
 *              1. For every expired contact we will call the deleteFn, actually that means:
 *                 all routes where the expired contact appears will be deleted.
 *              2. The timeContactToRemove will be redefined.
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  13/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
void removeExpiredContacts(time_t time)
{
	time_t min = MAX_POSIX_TIME;
	Contact *contact;
	RbtNode *node, *next;
	unsigned int tot = 0;

	if (time >= timeContactToRemove)
	{
		debug_printf("Remove the expired contacts.");
		node = rbt_first(contacts);
		while (node != NULL)
		{
			next = rbt_next(node);
			if (node->data != NULL)
			{
				contact = (Contact*) node->data;

				if (contact->toTime <= time)
				{
					rbt_delete(contacts, contact);
					tot++;
				}
				else if (contact->toTime < min)
				{
					min = contact->toTime;
				}
			}

			node = next;
		}

		timeContactToRemove = min;
		debug_printf("Removed %u contacts, next remove contacts time: %ld", tot,
				(long int ) timeContactToRemove);
	}
}

/******************************************************************************
 *
 * \par Function Name:
 *      compare_contacts
 *
 * \brief  Compare two contacts
 *
 *
 * \par Date Written:
 *      13/01/20
 *
 * \return int
 *
 * \retval   0  The contacts are equals (even if they has the same pointed address)
 * \retval  -1  The first contact is less than the second contact
 * \retval   1  The first contact is greater than the second contact
 *
 * \param[in]	*first    The first contact
 * \param[in]	*second   The second contact
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  13/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
int compare_contacts(void *first, void *second)
{
	Contact *a, *b;
	int result = 0;

	if (first == second) //same address pointed
	{
		result = 0;
	}
	else if (first != NULL && second != NULL)
	{
		a = (Contact*) first;
		b = (Contact*) second;
		if (a->fromNode < b->fromNode)
		{
			result = -1;
		}

		else if (a->fromNode > b->fromNode)
		{
			result = 1;
		}
		else if (a->toNode < b->toNode)
		{
			result = -1;
		}
		else if (a->toNode > b->toNode)
		{
			result = 1;
		}
		else if (a->fromTime < b->fromTime)
		{
			result = -1;
		}
		else if (a->fromTime > b->fromTime)
		{
			result = 1;
		}
		else
		{
			result = 0;
		}
	}

	return result;
}

#if (REVISABLE_CONFIDENCE)
/******************************************************************************
 *
 * \par Function Name:
 *      revise_confidence
 *
 * \brief  Revise the contact's confidence
 *
 *
 * \par Date Written:
 *      13/01/20
 *
 * \return int
 *
 * \retval   0  Confidence revised
 * \retval  -1  Contact not found
 * \retval  -2  Arguments error
 *
 * \param[in]	   fromNode     The contact's sender node
 * \param[in]        toNode     The contact's receiver node
 * \param[in]      fromTime     The contact's start time
 * \param[in] newConfidence     The revised confidence
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  13/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
int revise_confidence(unsigned long long fromNode, unsigned long long toNode, time_t fromTime, float newConfidence)
{
	int result = -2;
	Contact *contact = NULL;

	if (fromNode != 0 && toNode != 0 && fromTime >= 0 && newConfidence >= 0.0
			&& newConfidence <= 1.0)
	{
		contact = get_contact(fromNode, toNode, fromTime, NULL);
		result = -1;
		if(contact != NULL)
		{
			contact->confidence = newConfidence;
			result = 0;
		}
	}

	return result;
}
#endif

#if (REVISABLE_CONTACT)
/******************************************************************************
 *
 * \par Function Name:
 *      revise_contact
 *
 * \brief  Revise the contact's confidence and transmit rate (and MTVs)
 *
 *
 * \par Date Written:
 *      13/01/20
 *
 * \return int
 *
 * \retval   0  Contact revised
 * \retval  -1  Contact not found
 * \retval  -2  Arguments error
 *
 * \param[in]	   fromNode     The contact's sender node
 * \param[in]        toNode     The contact's receiver node
 * \param[in]      fromTime     The contact's start time
 * \param[in] newConfidence     The revised confidence
 * \param[in]      xmitRate     The revised transmit rate
 * \param[in]       copyMTV     Set to 1 if you want to copy MTVs.
 *                              Set to 0 otherwise
 * \param[in]           mtv     An array of 3 elements. The MTVs copied if copyMTV is setted to 1.
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  13/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
int revise_contact(unsigned long long fromNode, unsigned long long toNode, time_t fromTime, float newConfidence, unsigned long int xmitRate, int copyMTV, double mtv[])
{
	int result = -2;
	Contact *contact = NULL;
	int i;

	if (fromNode != 0 && toNode != 0 && fromTime >= 0 && newConfidence >= 0.0
			&& newConfidence <= 1.0)
	{
		contact = get_contact(fromNode, toNode, fromTime, NULL);
		result = -1;
		if(contact != NULL)
		{
			contact->confidence = newConfidence;
			contact->xmitRate = xmitRate;
			if(copyMTV != 0)
			{
				for(i = 0; i < 3; i++)
				{
					contact->mtv[i] = mtv[i];
				}
			}
			/* TODO ION DOESN'T DO THIS, but MTVs now aren't accurate
			else
			{
				prevVolume = (double) (contact->xmitRate * ((long unsigned int) (contact->toTime - contact->fromTime)));
				volume = (double) (xmitRate * ((long unsigned int) (toTime - fromTime)));
				for(i = 0; i < 3; i++)
				{
					contact->mtv[i] = volume;
					// TODO contact->mtv[i] = volume - (prevVolume - contact->mtv[i]); //consider also previous booking informations
					// TODO consider that transmit rate is changed so previous booking informations
					// maybe aren't accurate.				}
			}
			*/
			result = 0;
		}
	}

	return result;
}
#endif

#if (REVISABLE_XMIT_RATE)
/******************************************************************************
 *
 * \par Function Name:
 *      revise_xmit_rate
 *
 * \brief  Revise the contact's transmit rate (and MTVs)
 *
 *
 * \par Date Written:
 *      13/01/20
 *
 * \return int
 *
 * \retval   0  xmitRate revised
 * \retval  -1  Contact not found
 * \retval  -2  Arguments error
 *
 * \param[in]	   fromNode     The contact's sender node
 * \param[in]        toNode     The contact's receiver node
 * \param[in]      fromTime     The contact's start time
 * \param[in]      xmitRate     The revised transmit rate
 * \param[in]       copyMTV     Set to 1 if you want to copy MTVs.
 *                              Set to 0 otherwise
 * \param[in]           mtv     An array of 3 elements. The MTVs copied if copyMTV is setted to 1.
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  13/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
int revise_xmit_rate(unsigned long long fromNode, unsigned long long toNode, time_t fromTime, unsigned long int xmitRate, int copyMTV, double mtv[])
{
	int result = -2;
	Contact *contact = NULL;
	int i;

	if (fromNode != 0 && toNode != 0 && fromTime >= 0)
	{
		contact = get_contact(fromNode, toNode, fromTime, NULL);
		result = -1;
		if(contact != NULL)
		{
			contact->xmitRate = xmitRate;
			if(copyMTV != 0)
			{
				for(i = 0; i < 3; i++)
				{
					contact->mtv[i] = mtv[i];
				}
			}
			/* TODO ION DON'T DO THIS, but MTVs now aren't accurate
			else
			{
				prevVolume = (double) (contact->xmitRate * ((long unsigned int) (contact->toTime - contact->fromTime)));
				volume = (double) (xmitRate * ((long unsigned int) (toTime - fromTime)));
				for(i = 0; i < 3; i++)
				{
					contact->mtv[i] = volume;
					// TODO contact->mtv[i] = volume - (prevVolume - contact->mtv[i]); //consider also previous booking informations
					// TODO consider that transmit rate is changed so previous booking informations
					// maybe aren't accurate.
				}
			}
			*/
			result = 0;
		}
	}

	return result;
}
#endif

/******************************************************************************
 *
 * \par Function Name:
 *      erase_contact
 *
 * \brief  Reset all the contact's fields
 *
 *
 * \par Date Written:
 *      13/01/20
 *
 * \return void
 *
 * \param[in]	*contact   The contact for which we want to erase all fields
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  13/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static void erase_contact(Contact *contact)
{
	contact->citations = NULL;
	contact->confidence = 0.0;
	contact->fromNode = 0;
	contact->toNode = 0;
	contact->fromTime = 0;
	contact->toTime = 0;
	contact->type = Registration;
	contact->xmitRate = 0;
	contact->mtv[0] = 0.0;
	contact->mtv[1] = 0.0;
	contact->mtv[2] = 0.0;
	contact->routingObject = NULL;
}

/******************************************************************************
 *
 * \par Function Name:
 *      reset_ContactsGraph
 *
 * \brief Delete any contact that belongs to the graph, but not the contacts graph structure
 *
 *
 * \par Date Written:
 *      13/01/20
 *
 * \return void
 *
 * \par Notes:
 *             1. The deleteFn will be called, actually that means:
 *                all routes where a contact appears will be deleted.
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  13/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
void reset_ContactsGraph()
{
	rbt_clear(contacts);
	timeContactToRemove = MAX_POSIX_TIME;
}

/******************************************************************************
 *
 * \par Function Name:
 *      destroy_ContactsGraph
 *
 * \brief  Delete any contact that belongs to the graph, and the contacts graph itself.
 *
 *
 * \par Date Written:
 *      13/01/20
 *
 * \return  void
 *
 * \par Notes:
 *             1. The deleteFn will be called, actually that means:
 *                all routes where a contact appears will be deleted.
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  13/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
void destroy_ContactsGraph()
{
	rbt_destroy(contacts);
	contacts = NULL;
	timeContactToRemove = MAX_POSIX_TIME;
}

/******************************************************************************
 *
 * \par Function Name:
 *      erase_contact_note
 *
 * \brief  Erase all the fields of the ContactNote passed as argument
 *
 *
 * \par Date Written:
 *      13/01/20
 *
 * \return void
 *
 * \param[in]	*note  The ContactNote for which we want to erase all fields.
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  13/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static void erase_contact_note(ContactNote *note)
{

	if (note != NULL)
	{
		note->arrivalTime = -1;
		note->hopCount = 0;
		note->predecessor = NULL;
		note->suppressed = 0;
		note->visited = 0;
		note->owltSum = 0;
		note->arrivalConfidence = 0.0F;
		note->rangeFlag = 0;
		note->owlt = 0;
	}

	return;
}

/******************************************************************************
 *
 * \par Function Name:
 *      create_contact_note
 *
 * \brief  Allocate (with MWITHDRAW) a memory area for a ContactNote
 *
 *
 * \par Date Written:
 *      13/01/20
 *
 * \return ContactNote*
 *
 * \retval ContactNote*  The new allocated ContactNote
 * \retval NULL          MWITHDRAW error)
 *
 * \par Notes:
 *              1. The new allocated ContactNote will have all fields erased.
 *              2. You must check that the return value of this function is not NULL.
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  13/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static ContactNote* create_contact_note()
{
	ContactNote *note = (ContactNote*) MWITHDRAW(sizeof(ContactNote));

	if (note != NULL)
	{
		erase_contact_note(note);
	}

	return note;
}

/******************************************************************************
 *
 * \par Function Name:
 *      free_contact
 *
 * \brief  Delete the contact pointed by data
 *
 *
 * \par Date Written:
 *      13/01/20
 *
 * \return  void
 *
 * \param[in]	*data   The contact that we want to delete
 *
 * \par Notes:
 *              1. This function delete every route where the contact (data) appears.
 *              2. This function will delete the contact in its totality, included the
 *                 ContactNote and citations's list.
 *              3. The delete function used to deallocate memory is: MDEPOSIT.
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  13/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
void free_contact(void *data)
{
	Contact *contact;
	ListElt *current, *temp;
	ListElt *hop;
	Route *route;
	int deleted;

	if (data != NULL)
	{
		contact = (Contact*) data;
		if (contact->routingObject != NULL)
		{
			MDEPOSIT(contact->routingObject);
		}

		if (contact->citations != NULL)
		{
			current = contact->citations->first;
			while (current != NULL)
			{
				deleted = 0;
				temp = current->next;

				if (current->data != NULL)
				{
					hop = (ListElt*) current->data;
					if (hop->list != NULL)
					{
						if (hop->list->userData != NULL)
						{
							deleted = 1;
							route = (Route*) hop->list->userData;
							delete_cgr_route(route); //this function remove the citation
						}
					}
				}
				if (deleted == 0)
				{
					flush_verbose_debug_printf("Error!!!");
					list_remove_elt(current);
				}

				current = temp;
			}

			MDEPOSIT(contact->citations);

		}
		erase_contact(contact);
		MDEPOSIT(contact);
		data = NULL;
	}
}

/******************************************************************************
 *
 * \par Function Name:
 *      create_contact
 *
 * \brief  Allocate memory for a new contact
 *
 *
 * \par Date Written:
 *      13/01/20
 *
 * \return Contact*
 *
 * \retval Contact*  The pointer to the allocated contact 
 * \retval NULL      MWITHDRAW error
 *
 * \param[in]	fromNode      The contact's sender node
 * \param[in]	toNode        The contact's receiver node
 * \param[in]	fromTime      The contact's start time
 * \param[in]	toTime        The contact's end time
 * \param[in]	xmitRate      In bytes per second
 * \param[in]	confidence    The confidence that the contact will effectively materialize
 * \param[in]	type          The type of the contact (Registration or Scheduled)
 *
 * \par Notes:
 *             1. You must check that the return value of this function is not NULL.
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  13/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
Contact* create_contact(unsigned long long fromNode, unsigned long long toNode, time_t fromTime,
		time_t toTime, long unsigned int xmitRate, float confidence, CtType type)
{
	Contact *contact = NULL;
	double volume;

	contact = (Contact*) MWITHDRAW(sizeof(Contact));

	if (contact != NULL)
	{
		contact->fromNode = fromNode;
		contact->toNode = toNode;
		contact->fromTime = fromTime;
		contact->toTime = toTime;
		contact->xmitRate = xmitRate;
		contact->confidence = confidence;
		contact->type = type;
		/* NOTE: We assume that toTime - fromTime is greater or equal to 0 */
		volume = (double) (xmitRate * ((long unsigned int) (toTime - fromTime)));
		contact->mtv[0] = volume;
		contact->mtv[1] = volume;
		contact->mtv[2] = volume;

		contact->citations = list_create(contact, NULL, NULL, NULL);
		if (contact->citations == NULL)
		{
			MDEPOSIT(contact);
			contact = NULL;
		}
		else
		{
			contact->routingObject = create_contact_note();
			if (contact->routingObject == NULL)
			{
				MDEPOSIT(contact->citations);
				MDEPOSIT(contact);
				contact = NULL;
			}
		}
	}

	return contact;
}

/******************************************************************************
 *
 * \par Function Name:
 *      add_contact_to_graph
 *
 * \brief  Allocate memory for a contact, set the fields of the contacts to the
 *         passed arguments and add the contact to the contacts graph
 *
 *
 * \par Date Written:
 *      13/01/20
 *
 * \return int
 *
 * \retval   2   Success case: Revised xmit rate (and MTVs)
 * \retval   1   Success case: contact added to the contacts graph
 * \retval   0   Arguments error case
 * \retval  -1   Overlapped contact (don't change it)
 * \retval  -2   MWITHDRAW error
 *
 * \param[in]	fromNode           The contact's sender node
 * \param[in]	toNode             The contact's receiver node
 * \param[in]	fromTime           The contact's start time
 * \param[in]	toTime             The contact's end time
 * \param[in]	xmitRate           In bytes per second
 * \param[in]	confidence         The confidence that the contact will effectively materialize
 * \param[in]   copyMTV            Set to 1 if you want to copy the MTV from mtv input parameter.
 *                                 Set to 0 otherwise
 * \param[in]   mtv                The contact's MTV: this must be an array of 3 elements.
 *
 * \par Notes:
 *             1. This function will change timeContactToRemove if the contact->toTime of the
 *                new contact is less than the current timeContactToRemove, in that case
 *                timeContactToRemove will be equals to contact->toTime
 *             2. Set copyMTV to 1 if you want to be compatible with the data gets
 *                for example from an interface.
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  13/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
int add_contact_to_graph(unsigned long long fromNode, unsigned long long toNode, time_t fromTime,
		time_t toTime, long unsigned int xmitRate, float confidence, int copyMTV, double mtv[])
{
	int result = -1;
	int overlapped;
	Contact *contact, *temp;
	RbtNode *elt = NULL;
	CtType contactType;

	if (fromNode == 0 || toNode == 0 || toTime < 0 || (fromTime > toTime) || confidence < 0.0
			|| confidence > 1.0)
	{
		result = 0;
	}
	else
	{
		if (fromTime == (time_t) -1) /*Type : Registration	*/
		{
			if (fromNode == toNode)
			{
				fromTime = MAX_POSIX_TIME;
				toTime = MAX_POSIX_TIME;
				xmitRate = 0;
				confidence = 1.0;
				contactType = Registration;

				contact = get_contact(fromNode, toNode, fromTime, NULL);

				if (contact == NULL)
				{
					contact = create_contact(fromNode, toNode, fromTime, toTime, xmitRate,
							confidence, contactType);
					elt = rbt_insert(contacts, contact);

					result = (elt != NULL ? 1 : -2);

					if (result == -2)
					{
						free_contact(contact);
					}
				}
			}
			else
			{
				result = 0;
			}

		}
		else
		{
			result = -1;
			if (fromTime >= 0) /*Type: Scheduled	*/
			{
				temp = get_first_contact_from_node_to_node(fromNode, toNode, &elt);
				overlapped = 0;
				while (temp != NULL)
				{
					if (temp->fromNode == fromNode && temp->toNode == toNode)
					{
						if(fromTime == temp->fromTime && toTime == temp->toTime)
						{
							// contact exists in contacts graph
#if (REVISABLE_XMIT_RATE && ADD_AND_REVISE_CONTACT)
							if(copyMTV != 0)
							{
								if(temp->xmitRate != xmitRate)
								{
									temp->xmitRate = xmitRate; //update xmitRate
									result = 2;
									// Maybe you want to consider it as a significant change to contact plan
									// It could be interpreted as a new contact, your decision
								}
								//update MTVs
								// TODO previous booking informations ???
								temp->mtv[0] = mtv[0];
								temp->mtv[1] = mtv[1];
								temp->mtv[2] = mtv[2];
							}
							else if(temp->xmitRate != xmitRate) //otherwise don't change previous booking informations...
							{
								temp->xmitRate = xmitRate; //update xmitRate
								//TODO previous booking informations ???
								result = 2;
								// Maybe you want to consider it as a significant change to contact plan
								// It could be interpreted as a new contact, your decision
							}
#endif
#if (REVISABLE_CONFIDENCE && ADD_AND_REVISE_CONTACT)
							// don't consider confidence as rilevant changes to contact plan
							// (don't discard routes for this)
							temp->confidence = confidence;
#endif
							overlapped = 1;
							temp = NULL;
						}
						else if (fromTime >= temp->fromTime && fromTime < temp->toTime)
						{
							overlapped = 1;
							temp = NULL; //I leave the loop
						}
						else if (toTime > temp->fromTime && toTime <= temp->toTime)
						{
							overlapped = 1;
							temp = NULL; //I leave the loop
						}
						else if (toTime <= temp->fromTime)
						{
							//contacts ordered by the fromTime
							temp = NULL; //I leave the loop
						}
						else
						{
							temp = get_next_contact(&elt);
						}
					}
					else
					{
						temp = NULL; //I leave the loop
					}
				}

				if (overlapped == 0)
				{
					contactType = Scheduled;
					contact = create_contact(fromNode, toNode, fromTime, toTime, xmitRate, confidence,
							contactType);

					if(copyMTV != 0)
					{
						contact->mtv[0] = mtv[0];
						contact->mtv[1] = mtv[1];
						contact->mtv[2] = mtv[2];
					}
					elt = rbt_insert(contacts, contact);

					result = ((elt != NULL) ? 1 : -2);

					if (result == -2)
					{
						free_contact(contact);
					}
					else if (timeContactToRemove > toTime)
					{
						timeContactToRemove = toTime;
					}
				}
			} //end if contact scheduled
		}
	}

	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 *      removeAllContacts
 *
 * \brief   Remove all contacts between fromNode and toNode from the contacts graph
 *
 *
 * \par Date Written:
 *      13/01/20
 *
 * \return  void
 *
 * \param[in]	fromNode   The fromNode of the contacts that we want to remove
 * \param[in]	toNode     The toNode of the contacts that we want to remove
 *
 * \par Notes:
 *              1. The free_contact will be called for every contact removed.
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  13/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static void removeAllContacts(unsigned long long fromNode, unsigned long long toNode)
{
	Contact *current;
	RbtNode *node;

	current = get_first_contact_from_node_to_node(fromNode, toNode, &node);

	while (current != NULL)
	{
		node = rbt_next(node);
		rbt_delete(contacts, current);
		if (node != NULL)
		{
			current = (Contact*) node->data;
		}
		else
		{
			current = NULL;
		}

		if (current != NULL)
		{
			if (current->fromNode != fromNode || current->toNode != toNode)
			{
				current = NULL;
			}
		}
	}

	return;
}

/******************************************************************************
 *
 * \par Function Name:
 *      remove_contact_elt_from_graph
 *
 * \brief  Remove the contact that is equal to elt, compared with the compare_contacts function
 *
 *
 * \par Date Written:
 *      13/01/20
 *
 * \return  void
 *
 * \param[in]   *elt  The elt's fields must be the {fromNode, toNode, fromTime} of the contact
 *                    that we want to remove from the graph
 *
 * \par Notes:
 *              1. The free_contact will be called for the contact removed.
 *                 Note that if this contact is elt itself, it will be removed and you have
 *                 a dummy reference to it.
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  13/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
void remove_contact_elt_from_graph(Contact *elt)
{
	if (elt != NULL)
	{
		rbt_delete(contacts, elt);
	}

	return;
}

/******************************************************************************
 *
 * \par Function Name:
 *      remove_contact_from_graph
 *
 * \brief  Remove the contact that has the same {fromNode, toNode, *fromTime} fields
 *
 *
 * \par Date Written:
 *      13/01/20
 *
 * \return  void
 *
 * \param[in]   *fromTime   If it's NULL we remove all contacts that match with
 *                         {fromNode, toNode}, otherwise we remove the contact
 *                         that matches with {fromNode, toNode, *fromTime}
 * \param[in]	fromNode   The sender node of the contact to remove
 * \param[in]   toNode     The receiver node of the contact to remove
 *
 * \par Notes:
 *             1. The free_contact will be called for the contact(s) removed.
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  13/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
void remove_contact_from_graph(time_t *fromTime, unsigned long long fromNode,
		unsigned long long toNode)
{
	Contact arg;
	int ok = 0;

	if (fromTime != NULL)
	{
		erase_contact(&arg);
		if (*fromTime == -1 && fromNode == toNode)
		{
			arg.fromTime = 0;
			ok = 1;
		}
		else if (fromNode != toNode)
		{
			arg.fromTime = *fromTime;
			ok = 1;
		}

		if (ok)
		{
			arg.fromNode = fromNode;
			arg.toNode = toNode;
			rbt_delete(contacts, &arg);
		}
	}
	else
	{
		removeAllContacts(fromNode, toNode);
	}

	return;
}

/* ----------------------------------------------------
 * Functions to search contacts in the graph
 * ----------------------------------------------------
 */

/******************************************************************************
 *
 * \par Function Name:
 *      get_contact
 *
 * \brief  Get the contact that matches with the {fromNode, toNode, fromTime}
 *
 *
 * \par Date Written:
 *      13/01/20
 *
 * \return Contact*
 *
 * \retval Contact*  The contact found
 * \retval NULL      There isn't a contact with this characteristics
 *
 * \param[in]   fromNode    The contact's sender node
 * \param[in]   toNode      The contact's receiver node
 * \param[in]   fromTime    The contact's start time
 * \param[out]  **node      If this argument isn't NULL, at the end it will
 *                          contains the RbtNode that points to the contact returned by the function
 *
 * \par Notes:
 *             1. You must check that the return value of this function is not NULL.
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  13/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
Contact* get_contact(unsigned long long fromNode, unsigned long long toNode, time_t fromTime,
		RbtNode **node)
{
	Contact arg, *result;
	RbtNode *elt;

	result = NULL;
	if (fromNode != 0 && toNode != 0 && fromTime >= 0)
	{
		erase_contact(&arg);
		arg.fromNode = fromNode;
		arg.toNode = toNode;
		arg.fromTime = fromTime;

		elt = rbt_search(contacts, &arg, NULL);
		if (elt != NULL)
		{
			if (elt->data != NULL)
			{
				result = (Contact*) elt->data;
				if (node != NULL)
				{
					*node = elt;
				}
			}
		}
	}

	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 *      get_first_contact
 *
 * \brief  Get the first contact of the contacts graph
 *
 *
 * \par Date Written:
 *      13/01/20
 *
 * \return Contact*
 *
 * \retval Contact*  The first contact
 * \retval NULL      The contacts graph is empty
 *
 * \param[out]  **node   If this argument isn't NULL, at the end it will
 *                       contains the RbtNode that points to the contact returned by the function
 *
 * \par Notes:
 *             1. You must check that the return value of this function is not NULL.
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  13/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
Contact* get_first_contact(RbtNode **node)
{
	Contact *result = NULL;
	RbtNode *currentContact = NULL;

	currentContact = rbt_first(contacts);
	if (currentContact != NULL)
	{
		result = (Contact*) currentContact->data;
		if (node != NULL)
		{
			*node = currentContact;
		}
	}

	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 *      get_first_contact_from_node
 *
 * \brief  Get the first contact of the graph that has contact->fromNode == fromNode
 *
 *
 * \par Date Written:
 *      13/01/20
 *
 * \return Contact*
 *
 * \retval Contact*  The contact found
 * \retval NULL      There isn't a contact with this fromNode field
 *
 * \param[in]   fromNode   The contact's sender node
 * \param[out]  **node     If this argument isn't NULL, at the end it will
 *                         contains the RbtNode that points to the contact returned by the function
 *
 * \par Notes:
 *             1. You must check that the return value of this function is not NULL.
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  13/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
Contact* get_first_contact_from_node(unsigned long long fromNode, RbtNode **node)
{
	Contact arg;
	Contact *result = NULL;
	RbtNode *currentContact = NULL;

	erase_contact(&arg);
	arg.fromNode = fromNode;
	arg.fromTime = -1;
	rbt_search(contacts, &arg, &currentContact);

	if (currentContact != NULL)
	{
		result = (Contact*) currentContact->data;

		if (result->fromNode != fromNode)
		{
			result = NULL;
		}
		else if (node != NULL)
		{
			*node = currentContact;
		}
	}

	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 *      get_first_contact_from_node_to_node
 *
 * \brief Get the first contact of the graph that has
 *        (contact->fromNode == fromNode) && (contact->toNode == toNode)
 *
 *
 * \par Date Written:
 *      13/01/20
 *
 * \return Contact*
 *
 * \retval Contact*  The contact found 
 * \retval NULL      There isn't a contact with this sender node and receiver node
 *
 *
 * \param[in]   fromNode    The contact's sender node
 * \param[in]   toNode      The contact's receiver node
 * \param[out]  **rbtNode   If this argument isn't NULL, at the end it will
 *                          contains the RbtNode that points to the contact returned by the function
 *
 * \par Notes:
 *              1. You must check that the return value of this function is not NULL.
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  13/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
Contact* get_first_contact_from_node_to_node(unsigned long long fromNode, unsigned long long toNode,
		RbtNode **node)
{
	Contact arg;
	Contact *result = NULL;
	RbtNode *currentContact = NULL;

	erase_contact(&arg);
	arg.fromNode = fromNode;
	arg.toNode = toNode;
	arg.fromTime = -1;
	rbt_search(contacts, &arg, &currentContact);

	if (currentContact != NULL)
	{
		result = (Contact*) currentContact->data;

		if (result->fromNode != fromNode || result->toNode != toNode)
		{
			result = NULL;
		}
		else if (node != NULL)
		{
			*node = currentContact;
		}
	}

	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 *      get_next_contact
 *
 * \brief  Get the next contact referring to the current contact pointed by the argument "node"
 *
 *
 * \par Date Written:
 *      13/01/20
 *
 * \return Contact*
 *
 * \retval Contact*  The contact found
 * \retval NULL      There isn't the next contact
 *
 * \param[in,out]  **node  If this arguments isn't NULL, at the end it will
 *                            contains the RbtNode that points to the contact returned by the function
 *
 * \par Notes:
 *              1. You must check that the return value of this function is not NULL.
 *              2. If we find the next contact this function update the RbtNode pointed by node.
 *              3. You can use this function in a delete loop (referring to the current
 *                 implementation of "rbt_delete").
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  13/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
Contact* get_next_contact(RbtNode **node)
{
	Contact *result = NULL;
	RbtNode *temp = NULL;

	if (node != NULL)
	{
		temp = rbt_next(*node);
		if (temp != NULL)
		{
			result = (Contact*) temp->data;
		}

		*node = temp;
	}

	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 *      get_prev_contact
 *
 * \brief  Get the previous contact referring to the current contact pointed by the argument "node"
 *
 *
 * \par Date Written:
 *      13/01/20
 *
 * \retval Contact* The contact found
 * \retval NULL     There isn't the previous contact
 *
 * \param[in,out]  **node  If this arguments isn't NULL, at the end it will
 *                         contains the RbtNode that points to the contact returned by the function
 *
 * \par Notes:
 *              1. You must check that the return value of this function is not NULL.
 *              2. If we find the prev contact this function update the RbtNode pointed by node.
 *              3. Never use this function in a delete loop (referring to the current
 *                 implementation of "rbt_delete").
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  13/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
Contact* get_prev_contact(RbtNode **node)
{
	Contact *result = NULL;
	RbtNode *temp = NULL;

	/*
	 * Non usare questa funzione in un ciclo di "delete", per maggiori dettagli guarda la
	 * struttura di "rbt_delete"
	 */

	if (node != NULL)
	{
		temp = rbt_prev(*node);
		if (temp != NULL)
		{
			result = (Contact*) temp->data;
			*node = temp;
		}
		else
		{
			*node = NULL; //temp == NULL
		}
	}

	return result;
}

#if (LOG == 1)

/******************************************************************************
 *
 * \par Function Name:
 *      printContact
 *
 * \brief  Print the contact pointed by data to a buffer and at the end to_add points to that buffer
 *
 *
 * \par Date Written:
 *      13/01/20
 *
 * \return int
 *
 * \retval      0   Success case
 * \retval     -1   Some errors occurred
 *
 * \param[out]	*file     The file where we want to print the contact's fields
 * \param[in]	*data     The pointer to the contact
 *
 * \par Notes:
 *             1. This function assumes that "to_add" is not NULL.
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  13/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static int printContact(FILE *file, void *data)
{
	Contact *contact;
	int result = -1;
	if (data != NULL && file != NULL)
	{
		result = 0;
		contact = (Contact*) data;
		fprintf(file, "%-15llu %-15llu %-15ld %-15ld %-15lu %-15.2f ", contact->fromNode,
				contact->toNode, (long int) contact->fromTime, (long int) contact->toTime,
				contact->xmitRate, contact->confidence);
		if (contact->citations != NULL)
		{
			fprintf(file, "%lu\n", contact->citations->length);
		}
		else
		{
			fprintf(file, "NULL\n");
		}
	}
	else
	{
		fprintf(file, "\nCONTACT: NULL\n");
	}

	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 *      printContactsGraph
 *
 * \brief  Print the contacts graph
 *
 *
 * \par Date Written:
 *      13/01/20
 *
 * \return int
 *
 * \retval   1   Success case, contacts graph printed
 * \retval   0   The file is NULL
 *
 * \param[in]  file          The file where we want to print the contacts graph
 * \param[in]  currentTime   The time to print together at the contacts graph to keep trace of the
 *                           modification's history
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  13/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
int printContactsGraph(FILE *file, time_t currentTime)
{
	int result = 0;

	if (file != NULL)
	{
		result = 1;

		fprintf(file,
				"\n--------------------------------------------- CONTACTS GRAPH ---------------------------------------------\n");
		fprintf(file, "Time: %ld\n%-15s %-15s %-15s %-15s %-15s %-15s %s\n", (long int) currentTime,
				"FromNode", "ToNode", "FromTime", "ToTime", "XmitRate", "Confidence", "Citations");
		result = printTreeInOrder(contacts, file, printContact);

		if (result == 1)
		{
			fprintf(file,
					"\n----------------------------------------------------------------------------------------------------------\n");
		}
		else
		{
			fprintf(file,
					"\n------------------------------------------- CONTACTS GRAPH ERROR --------------------------------------------\n");
		}
	}
	else
	{
		result = -1;
	}

	return result;
}

#endif
