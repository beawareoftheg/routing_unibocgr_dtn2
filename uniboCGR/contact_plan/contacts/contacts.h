/** \file contacts.h
 *
 *  \brief  This file provides the definition of the Contact type,
 *          of the ContactNode type and of the CtType, with all the declarations
 *          of the functions to manage the contact graph
 *
 *
 ** \copyright Copyright (c) 2020, Alma Mater Studiorum, University of Bologna, All rights reserved.
 *
 *  \author Lorenzo Persampieri, lorenzo.persampieri@studio.unibo.it
 *
 *  \par Supervisor
 *       Carlo Caini, carlo.caini@unibo.it
 */

#ifndef SOURCES_CONTACTS_PLAN_CONTACTS_CONTACTS_H_
#define SOURCES_CONTACTS_PLAN_CONTACTS_CONTACTS_H_

#include "../../library/list/list_type.h"
#include "../../library/commonDefines.h"
#include "../../ported_from_ion/rbt/rbt_type.h"
#include <sys/time.h>

#ifndef REVISABLE_CONFIDENCE
/**
 * \brief Set to 1 if you want to permits that contact's confidence can be changed.
 *        Set to 0 otherwise.
 */
#define REVISABLE_CONFIDENCE 1
#endif

#ifndef REVISABLE_XMIT_RATE
/**
 * \brief Set to 1 if you want to permits that contact's xmitRate (and MTVs) can be changed.
 *        Set to 0 otherwise.
 *
 * \par Notes:
 *          1. I suggest you to set this macro to 1 if you copy contacts from
 *             another contact plan by an interface.
 */
#define REVISABLE_XMIT_RATE 1
#endif

#if (REVISABLE_CONFIDENCE && REVISABLE_XMIT_RATE)
#undef REVISABLE_CONTACT
#define REVISABLE_CONTACT 1
#endif

typedef struct cgrContactNote ContactNote;

typedef enum
{
	Registration = 1, Scheduled,
} CtType;

typedef struct
{
	/**
	 * \brief Sender node (ipn node number)
	 */
	unsigned long long fromNode;
	/**
	 * \brief Receiver node (ipn node number)
	 */
	unsigned long long toNode;
	/**
	 * \brief Start transit time
	 */
	time_t fromTime;
	/**
	 * \brief Stop transmit time
	 */
	time_t toTime;
	/**
	 * \brief In bytes per second
	 */
	long unsigned int xmitRate;
	/**
	 * \brief Confidence that the contact will materialize
	 */
	float confidence;
	/**
	 * \brief Registration or Scheduled
	 */
	CtType type;
	/**
	 * \brief Remaining volume (for each level of priority)
	 */
	double mtv[3];
	/**
	 * \brief Used by Dijkstra's search
	 */
	ContactNote *routingObject;
	/**
	 * \brief List of ListElt data.
	 *
	 * \details Each citation is a pointer to the element
	 * of the hops list of the Route where the contact appears,
	 * and this element of the hops list points to this contact.
	 */
	List citations;
} Contact;

struct cgrContactNote
{
	/**
	 * \brief Previous contact in the route, used to reconstruct the route at the end of
	 * the Dijkstra's search
	 */
	Contact *predecessor;
	/**
	 * \brief Best case arrival time to the toNode of the contact
	 */
	time_t arrivalTime;
	/**
	 * \brief Boolean used to identify each contact that belongs to the visited set
	 *
	 * \details Values
	 *          -  1  Contact already visited
	 *          -  0  Contact not visited
	 */
	int visited;
	/**
	 * \brief Flag used to identify each contact that belongs to the excluded set
	 */
	int suppressed;
	/**
	 * \brief Ranges sum to reach the toNode
	 */
	unsigned int owltSum;
	/**
	 * \brief Number of hops to reach this contact during Dijkstra's search.
	 */
	unsigned int hopCount;
	/**
	 * \brief Product of the confidence of each contacts in the path to
	 * reach this contact and of the contact's confidence itself
	 */
	float arrivalConfidence;
	/**
	 * \brief Flag to known if we already get the range for this contact during the Dijkstra's search.
	 *
	 * \details Values:
	 *          -  1  Range found at contact's start time
	 *          -  0  Range has yet to be searched
	 *          - -1  Range not found at the contact's start time
	 *
	 * \par Notes:
	 *          1.  Phase one always looks for the range at contact's start time.
	 */
	int rangeFlag;
	/**
	 * \brief The owlt of the range found.
	 */
	unsigned int owlt;
};

#ifdef __cplusplus
extern "C"
{
#endif

extern int compare_contacts(void *this, void *other);
extern Contact* create_contact(unsigned long long fromNode, unsigned long long toNode,
		time_t fromTime, time_t toTime, long unsigned int xmitRate, float confidence, CtType type);
extern void free_contact(void*);

extern int create_ContactsGraph();
extern void destroy_ContactsGraph();
extern void reset_ContactsGraph();

extern void removeExpiredContacts(time_t time);
extern void remove_contact_from_graph(time_t *fromTime, unsigned long long fromNode,
		unsigned long long toNode);
extern void remove_contact_elt_from_graph(Contact *elt);
int add_contact_to_graph(unsigned long long fromNode, unsigned long long toNode, time_t fromTime,
		time_t toTime, long unsigned int xmitRate, float confidence, int copyMTV, double mtv[]);
extern void discardAllRoutesFromContactsGraph();

extern Contact* get_contact(unsigned long long fromNode, unsigned long long toNode, time_t fromTime,
		RbtNode **node);
extern Contact* get_first_contact(RbtNode **node);
extern Contact* get_first_contact_from_node(unsigned long long fromNodeNbr, RbtNode **node);
extern Contact* get_first_contact_from_node_to_node(unsigned long long fromNodeNbr,
		unsigned long long toNodeNbr, RbtNode **node);
extern Contact* get_next_contact(RbtNode **node);
extern Contact* get_prev_contact(RbtNode **node);

#if REVISABLE_CONFIDENCE
extern int revise_confidence(unsigned long long fromNode, unsigned long long toNode, time_t fromTime, float newConfidence);
#endif
#if REVISABLE_XMIT_RATE
extern int revise_xmit_rate(unsigned long long fromNode, unsigned long long toNode, time_t fromTime, unsigned long int xmitRate, int copyMTV, double mtv[]);
#endif
#if REVISABLE_CONTACT
extern int revise_contact(unsigned long long fromNode, unsigned long long toNode, time_t fromTime, float newConfidence, unsigned long int xmitRate, int copyMTV, double mtv[]);
#endif

#if (LOG == 1)
extern int printContactsGraph(FILE *file, time_t currentTime);
#else
#define printContactsGraph(file, currentTime) do {  } while(0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* SOURCES_CONTACTS_PLAN_CONTACTS_CONTACTS_H_ */
