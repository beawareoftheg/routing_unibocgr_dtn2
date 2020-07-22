/** \file interface_unibocgr_dtn2.c
 *  
 *  \brief    This file provides the implementation of the functions
 *            that make UniboCGR's implementation compatible with DTN2.
 *	
 *  \details  In particular the functions that import the contact plan and the BpPlans.
 *            We provide in output the best routes list.
 *
 ** \copyright Copyright (c) 2020, Alma Mater Studiorum, University of Bologna, All rights reserved.
 *
 *  \author Giacomo Gori, giacomo.gori3@studio.unibo.it
 *
 *  \par Supervisor
 *       Carlo Caini, carlo.caini@unibo.it
 */
#include "interface_unibocgr_dtn2.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>


#include "../ported_from_ion/general_functions_ported_from_ion.h"

#include "../library/commonDefines.h"
#include "../cgr/cgr.h"

#include "../bundles/bundles.h"
#include "../contact_plan/contactPlan.h"

#include "../contact_plan/contacts/contacts.h"
#include "../contact_plan/ranges/ranges.h"
#include "../library/list/list.h"
#include "../ported_from_ion/scalar/scalar.h"

#include "../cgr/cgr_phases.h"
#include "../msr/msr_utils.h"


//Giacomo: DA MODIFICARE
#define NOMINAL_PRIMARY_BLKSIZE	29 // from ION 4.0.0: bpv7/library/libbpP.c
#define MSR 0

/**
 * \brief This time is used by the CGR as time 0.
 */
static time_t reference_time = -1;
/**
 * \brief Boolean used to know if the CGR has been initialized or not.
 */
static int initialized = 0;
/**
 * \brief The list of excluded neighbors for the current bundle.
 */
static List excludedNeighbors = NULL;
/**
 * \brief The current bundle received from DTN2.
 */
static Bundle *Dtn2Bundle = NULL;
/**
 * \brief CgrBundle used during the current call.
 */
static CgrBundle *cgrBundle = NULL;

#define printDebugIonRoute(ionwm, route) do {  } while(0)


#if (MSR == 1)
/******************************************************************************
 *
 * \par Function Name:
 *      get_cgrr_ext_block
 *
 * \brief  Get the CGRRouteBlock stored into CGRR Extension Block
 *
 *
 * \par Date Written:
 *      23/04/20
 *
 * \return int
 *
 * \retval  0  Success case: CGRRouteBlock found
 * \retval -1  CGRRouteBlock not found
 * \retval -2  System error
 *
 * \param[in]   *bundle    The bundle that contains the RGR Extension Block
 * \param[out] **resultBlk The CGRRouteBlock extracted from the CGRR Extension Block, only in success case.
 *                         The resultBLk will be allocated by this function.
 *
 * \warning bundle    doesn't have to be NULL
 * \warning resultBlk doesn't have to be NULL
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  23/04/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static int get_cgrr_ext_block(Bundle *bundle, CGRRouteBlock **resultBlk)
{
	Sdr sdr = getIonsdr();
	int result = 0, i, j;
	Object extBlockElt;
	Address extBlkAddr;
	CGRRouteBlock* cgrrBlk;
	CGRRoute *route;

	OBJ_POINTER(ExtensionBlock, blk);

	/*
	if(localNode == bundle->id.source.ssp.ipn.nodeNbr)
	{
		//TODO retransmission case bug ???
		result = -1; //source node
	}
	*/
	/* Step 1 - Check for the presence of CGRR extension*/

	if (!(extBlockElt = findExtensionBlock(bundle, CGRRBlk, 0, 0, 0)))
	{
		result = -1;
	}
	else
	{

		/* Step 2 - Get deserialized version of CGRR extension block*/

		extBlkAddr = sdr_list_data(sdr, extBlockElt);

		GET_OBJ_POINTER(sdr, ExtensionBlock, blk, extBlkAddr);

		cgrrBlk = (CGRRouteBlock*) MTAKE(sizeof(CGRRouteBlock));

		if (cgrrBlk == NULL)
		{
			result = -2;
		}
		else
		{
			if (cgrr_getCGRRFromExtensionBlock(blk, cgrrBlk) < 0)
			{
				result = -2;
				MRELEASE(cgrrBlk);
			}
			else
			{
				result = 0;
				*resultBlk = cgrrBlk;

				route = &(cgrrBlk->originalRoute);

				for(i = 0; i < route->hopCount; i++)
				{
					route->hopList[i].fromTime -= reference_time;
				}

				for(j = 0; j < cgrrBlk->recRoutesLength; j++)
				{
					route = &(cgrrBlk->recomputedRoutes[j]);

					for(i = 0; i < route->hopCount; i++)
					{
						route->hopList[i].fromTime -= reference_time;
					}
				}
			}
		}
	}


	return result;
}
#endif

/******************************************************************************
 *
 * \par Function Name:
 *      convert_bundle_from_dtn2_to_cgr
 *
 * \brief Convert the characteristics of the bundle from DTN2 to
 *        this CGR's implementation and initialize all the
 *        bundle fields used by the Unibo CGR.
 *
 *
 * \par Date Written:
 *      05/07/20
 *
 * \return int
 *
 * \retval  0  Success case
 * \retval -1  Arguments error
 * \retval -2  MWITHDRAW error
 *
 * \param[in]    toNode             The destination ipn node for the bundle (unsigned long long)
 * \param[in]    current_time       The current time, in differential time from reference_time
 * \param[in]    *Dtn2Bundle        The bundle in DTN2
 * \param[out]   *CgrBundle         The bundle in this CGR's implementation.
 *
 * \par	Notes:
 *             1. This function prints the ID of the bundle in the main log file.
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  05/07/20 | G. Gori		    |  Initial Implementation and documentation.
 *****************************************************************************/
static int convert_bundle_from_dtn2_to_cgr(time_t current_time, Bundle *Dtn2Bundle, CgrBundle *CgrBundle)
{
	//Giacomo: qua il codice è c, forse è meglio passare campi semplici invece che Bundle che è definito in c++?
	//Primo tentativo: faccio come se il codice fosse C++
	//TODO Consider to take count of extensions
	int result = -1;
	time_t offset;
#if (MSR == 1)
	CGRRouteBlock *cgrrBlk;
#endif
#if (CGR_AVOID_LOOP > 0)
	GeoRoute geoRoute;
#endif

	if (Dtn2Bundle != NULL && CgrBundle != NULL)
	{
		std::string ipnName = Dtn2Bundle->dest()->str();
		std::string delimiter1 = ":";
		std::string delimiter2 = ".";
		std::string s = ipnName.substr(ipnName.find(delimiter1) + 1, ipnName.find(delimiter2) - 1);
		std::stringstream convert;
		long destNode;
		convert << s;
		convert >> destNode;
		CgrBundle->terminus_node = destNode;
/*
#if (MSR == 1)
		CgrBundle->msrRoute = NULL;
		result = get_cgrr_ext_block(IonBundle, &cgrrBlk);

		if(result == 0)
		{
			result = set_msr_route(current_time, cgrrBlk, CgrBundle);
			releaseCgrrBlkMemory(cgrrBlk);
		}
#endif
#if (CGR_AVOID_LOOP > 0)
		if(result != -2)
		{
			result = get_rgr_ext_block(IonBundle, &geoRoute);
			if(result == 0)
			{
				result = set_geo_route_list(geoRoute.nodes, CgrBundle);
				MRELEASE(geoRoute.nodes);
			}
		}
#endif*/
		if (result != -2)
		{
			CLEAR_FLAGS(CgrBundle->flags); //reset previous mask
				#ifdef ECOS_ENABLED
            if(Dtn2Bundle->ecos_critical() & BP_MINIMUM_LATENCY)
            {
            	SET_CRITICAL(CgrBundle);
            }
				#endif
			/*Non ho trovato info sul backward propagation
			if (!(IS_CRITICAL(CgrBundle)) && IonBundle->returnToSender)
			{
				SET_BACKWARD_PROPAGATION(CgrBundle);
			}*/
			if(!(Dtn2Bundle->do_not_fragment()))
			{
				SET_FRAGMENTABLE(CgrBundle);
			}
			#ifdef ECOS_ENABLED
			CgrBundle->ordinal = (unsigned int) Dtn2Bundle->ecos_ordinal();
			#endif
			CgrBundle->size = NOMINAL_PRIMARY_BLKSIZE + Dtn2Bundle->durable_size();

			CgrBundle->evc = computeBundleEVC(CgrBundle->size); // SABR 2.4.3

			offset = Dtn2Bundle->creation_ts().seconds_ + EPOCH_2000_SEC - reference_time;
			//offset è la differenza tra la creazione del bundle e il momento di partenza del demone dtnd
			//CgrBundle->expiration_time = IonBundle->expirationTime
			//		- IonBundle->id.creationTime.seconds + offset;

			CgrBundle->expiration_time = Dtn2Bundle->expiration() + offset;
					//Giacomo: trova src e sostituisci a dest
			std::string ipnName2 = Dtn2Bundle->source()->str();
			std::string s2 = ipnName2.substr(ipnName2.find(delimiter1) + 1, ipnName2.find(delimiter2) - 1);
			std::stringstream converts;
			long sendNode;
			converts << s;
			converts >> sendNode;
			CgrBundle->sender_node = sendNode;
			CgrBundle->priority_level = Dtn2Bundle->priority();

			CgrBundle->dlvConfidence = 1;

			// We don't need ID during CGR so we just print it into log file.
			//Giacomo: non sono riusdito ad ottenere totalAduLength
			//Esso è la somma tra tutti i fragmentation offset e il payload dell'ultimo frammento
			// praticamente la lunghezza totale di tutto il bundle intero
			print_log_bundle_id    ((unsigned long long ) sendNode,
					Dtn2Bundle->creation_ts().seconds_, Dtn2Bundle->creation_ts().seqno_,
					10, Dtn2Bundle->frag_offset);
			writeLog("Payload length: %zu.", Dtn2Bundle->payload());

			result = 0;
		}
	}

	return result;
}



/******************************************************************************
 *
 * \par Function Name:
 *      convert_scalar_from_cgr_to_ion
 *
 * \brief  Convert a scalar type from this CGR's implementation to ION.
 *
 *
 * \par Date Written:
 *      19/02/20
 *
 * \return int
 *
 * \retval   0  Success case
 * \retval  -1  Arguments error
 *
 * \param[in]   *cgr_scalar   The scalar type in this CGR's implementation.
 * \param[out]  *ion_scalar   The scalar type in ION.
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 ****************************************************************************
static int convert_scalar_from_cgr_to_ion(CgrScalar *cgr_scalar, Scalar *ion_scalar)
{
	int result = -1;

	if (ion_scalar != NULL && cgr_scalar != NULL)
	{
		ion_scalar->gigs = cgr_scalar->gigs;
		ion_scalar->units = cgr_scalar->units;
		result = 0;
	}

	return result;
}*/



/******************************************************************************
 *
 * \par Function Name:
 *      convert_routes_from_cgr_to_dtn2
 *
 * \brief Convert a list of routes from CGR's format to DTN2's format
 *
 *
 * \par Date Written: 
 *      16/07/20
 *
 * \return int 
 *
 * \retval   0  All routes converted
 * \retval  -1  CGR's contact points to NULL
 *
 * \param[in]     evc             Bundle's estimated volume consumption
 * \param[in]     cgrRoutes       The list of routes in CGR's format
 * \param[out]    *res        	 All the next hops that CGR found, separated by a single space
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  16/07/20 | G. Gori         |  Initial Implementation and documentation.
 *****************************************************************************/
static int convert_routes_from_cgr_to_dtn2(long unsigned int evc, List cgrRoutes, std::string *res)
{
	ListElt *elt;
	Route *current;
	size_t count = 0;
	int result = 0;

	for (elt = cgrRoutes->first; elt != NULL && result >= 0; elt = elt->next)
	{
		if (elt->data != NULL)
		{
			current = (Route*) elt->data;
			std::string toNode = "ipn:";
			std::stringstream streamNode;
			streamNode << toNode << current->neighbor;
			//Giacomo: con ste stringhe è una bega
			res += streamNode.str();
			//Giacomo: da gestire la possibilità di avere + di un risultato
			//res += " ";
			/*
			IonRoute->toNodeNbr = current->neighbor;
			IonRoute->fromTime = current->fromTime + reference_time;
			IonRoute->toTime = current->toTime + reference_time;
			IonRoute->arrivalConfidence = current->arrivalConfidence;
			IonRoute->arrivalTime = current->arrivalTime + reference_time;
			IonRoute->maxVolumeAvbl = current->routeVolumeLimit;
			IonRoute->bundleECCC = evc;
			IonRoute->eto = current->eto + reference_time;
			IonRoute->pbat = current->pbat + reference_time;
			convert_scalar_from_cgr_to_ion(&(current->protected),
					&(IonRoute->committed));
			convert_scalar_from_cgr_to_ion(&(current->overbooked),
					&(IonRoute->overbooked));
			*/
		}
		else
		{
			result = -1;
		}
	}

	return result;
}





/******************************************************************************
 *
 * \par Function Name:
 *      add_contact
 *
 * \brief  Add a contact to the contacts graph of this CGR's implementation.
 *
 *
 * \par Date Written:
 *      02/07/20
 *
 * \return int
 *
 * \retval   1   Contact added
 * \retval   0   Contact's arguments error
 * \retval  -1   The contact overlaps with other contacts
 * \retval  -2   Can't read all info of the contact
 * \retval  -3   Can't read all info of the contact
 * \retval  -4   Can't read fromTime or toTime
 *
 * \param[in]   char*	The line read from the file
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  02/07/20 | G. Gori		    |  Initial Implementation and documentation.
 *****************************************************************************/
static int add_contact(char * fileline)
{
	Contact CgrContact;
	int count = 10, n=0, result = -2;
	long long fn = 0;
	long unsigned int xn = 0;
	CgrContact.type = Scheduled;
	//fromTime
	if(res[count] == '+')
	{
		count++;
		while(res[count] >= 48 && res[count] <= 57) {
			n = n * 10 + res[count] - '0';
			count++;
		}
		count++; 
		CgrContact.fromTime = n - reference_time;
		n=0;
	}
	else result = -4;
	//toTime
	if(res[count] == '+')
	{
		count++;
		while(res[count] >= 48 && res[count] <= 57) {
			n = n * 10 + res[count] - '0';
			count++;
		}
		count++;
		CgrContact.toTime = n - reference_time;
		n=0;
	}
	else result = -4;
	//fromNode
	while(res[count] >= 48 && res[count] <= 57) {
			fn = fn * 10 + res[count] - '0';
			count++;
		}
	count++;
	CgrContact.fromNode = fn ;
	fn=0;
	//toNode
	while(res[count] >= 48 && res[count] <= 57) {
			fn = fn * 10 + res[count] - '0';
			count++;
		}
	count++;
	CgrContact.toNode = fn ;
	fn=0;
	//txrate
	while(res[count] >= 48 && res[count] <= 57) {
			xn = xn * 10 + res[count] - '0';
			count++;
			result=0;
		}
	count++;
	CgrContact.xmitRate = xn ;
	CgrContact.confidence = 1;
	if (result == 0)
	{
		// Try to add contact
		result = addContact(CgrContact.fromNode, CgrContact.toNode, CgrContact.fromTime,
				CgrContact.toTime, CgrContact.xmitRate, CgrContact.confidence, 0, NULL);
		if(result >= 1)
		{
			// result == 2 : xmitRate revised, consider it as "new contact"
			result = 1;
		}

	}
	else
	{
		result = -3;
	}

	return result;
}

/**
 *
 *
 * TODO consider to add a revise contact function, to change contact's confidence and xmitRate
 *
 *
 */

/******************************************************************************
 *
 * \par Function Name:
 *      read_file_contactranges
 *
 * \brief  Read and add all new contacts or ranges of the file with filename 
 * 		  specified in the first parameter to the
 *         contacts graph of thic CGR's implementation.
 *
 * \details Only for Registration and Scheduled contacts. Giacomo:
 *
 *
 * \par Date Written:
 *      02/07/20
 *
 * \return int
 *
 * \retval  ">= 0"  Number of contacts added to the contacts graph
 * \retval     -1   open file error
 *
 * \param[in]   char*	The name of the file to read
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  02/07/20 | G. Gori         |  Initial Implementation and documentation.
 *****************************************************************************/
static int read_file_contactranges(char * filename)
{
	int result = 0, totAdded = 0, stop = 0;
	int skip = 0, count = 0;
	int result_contacts = 0, result_ranges = 0;
	int fd = open(filename, O_RDONLY);
	char car;
	if(fd < 0)
	{
		result = -1;
	}
	else
	{
		while(read(fd,&car,sizeof(char)))
		{
			if(count == 0 && car == '#') skip = 1;
			if(car == '\n')
			{
				if(skip==0)
				{
					lseek(fd,-count, 1);
					char * res;
					res = (char*) malloc(sizeof(char)*count);
					count = 0;
					read(fd,res,sizeof(char)*count);
					//Recognize if it's enough lenght and if it is a contact or range
					if(count > 18 && res[0] == 'a')
					{
						if(res[2] == 'c' && res[3] == 'o' && res[4] == 'n' && res[5] == 't' && res[6] == 'a' && res[7] == 'c' && res[8] == 't')
						{
							result = add_contact(res);
							if(result == 1)
							{
								result_contacts++;
							}
						}
						else if(res[2] == 'r' && res[3] == 'a' && res[4] == 'n' && res[5] == 'g' && res[6] == 'e')
						{
							result = add_range(res);
							if(result == 1)
							{
								result_ranges++;
							}
						}
					}
					free(res);
				}
				else if(skip==1)
				{
					count=0;
					skip=0;
				}
			}
			count++;
		}
		close(fd);
#if (LOG == 1)
		if (result_contacts > 0)
		{
			writeLog("Added %d contacts.", result_contacts);
		}
		if (result_ranges > 0)
		{
			writeLog("Added %d ranges.", result_ranges);
		}
#endif
	}
	result = result_contacts + result_ranges;

	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 *      add_range
 *
 * \brief  Add a range to the ranges graph of this CGR's implementation.
 *
 *
 * \par Date Written:
 *      02/07/20
 *
 * \return int
 *
 * \retval   1   Range added
 * \retval   0   Range's arguments error
 * \retval  -1   The range overlaps with other ranges
 * \retval  -2   MWITHDRAW error
 * \retval  -3   ION's range points to NULL
 *
 * \param[in]  char*		The line that contains the range
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  02/07/20 | G. Gori		    |  Initial Implementation and documentation.
 *****************************************************************************/
static int add_range(char* fileline)
{
	Range CgrRange;
	int result = -1;
	int n = 0;
	unsigned int ow = 0;
	long long fn = 0;
	//fromTime
	if(res[count] == '+')
	{
		count++;
		while(res[count] >= 48 && res[count] <= 57) {
			n = n * 10 + res[count] - '0';
			count++;
		}
		count++; 
		CgrRange.fromTime = n - reference_time;
		n=0;
	}
	else result = -4;
	//toTime
	if(res[count] == '+')
	{
		count++;
		while(res[count] >= 48 && res[count] <= 57) {
			n = n * 10 + res[count] - '0';
			count++;
		}
		count++; 
		CgrRange.fromTime = n - reference_time;
		n=0;
	}
	else result = -4;
	//fromNode
	while(res[count] >= 48 && res[count] <= 57) {
			fn = fn * 10 + res[count] - '0';
			count++;
		}
	count++;
	CgrRange.fromNode = fn ;
	fn=0;
	//toNode
	while(res[count] >= 48 && res[count] <= 57) {
			fn = fn * 10 + res[count] - '0';
			count++;
		}
	count++;
	CgrRange.toNode = fn ;
	//owlt
	while(res[count] >= 48 && res[count] <= 57) {
			ow = ow * 10 + res[count] - '0';
			count++;
			result = 0;
		}
	count++;
	CgrRange.owlt = ow ;
	if (result == 0)
	{
		result = addRange(CgrRange.fromNode, CgrRange.toNode, CgrRange.fromTime, CgrRange.toTime,
				CgrRange.owlt);
		if(result >= 1)
		{
			// result == 2 : owlt revised, consider it as "new range"
			result = 1;
		}
	}
	else
	{
		result = -3;
	}
	return result;
}



/******************************************************************************
 *
 * \par Function Name:
 *      update_contact_plan
 *
 * \brief  
 *
 *
 * \par Date Written:
 *      01/07/20
 *
 * \return int
 *
 * \retval   0  Contact plan has been changed and updated
 * \retval  -1  Contact plan isn't changed
 * \retval  -2  File error
 *
 * \param[in]   filename     The name of the file from which we read contacts
 * \param[in]   update       Set true to update contact plan, false to not update
 *
 * \warning file should exist
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  01/07/20 | G. Gori         |  Initial Implementation and documentation.
 *****************************************************************************/
static int update_contact_plan(char * filename, bool update)
{
	int result = -2;
	int result_read;
   //Currently contacts are read from file, so we don't have any update during execution
   //Set a condition in this if() if you need to check for update
	if (update)
	{
		writeLog("#### Contact plan modified ####");
		result_read = read_file_contactranges(filename);
		if (result_read < 0) //Error
		{
			result = -1;
		}
		else
		{
			result = 0;
		}

		//contactPlanEditTime.tv_sec = ionvdb->lastEditTime.tv_sec;
		//contactPlanEditTime.tv_usec = ionvdb->lastEditTime.tv_usec;

		writeLog("###############################");
		printCurrentState();

	}

	return result;

}

/******************************************************************************
 *
 * \par Function Name:
 *      exclude_neighbors
 *
 * \brief  Function that will exclude all the neighbors that can't be used as first hop
 *         to reach the destination. Currently it only clear the list.
 *
 *
 * \par Date Written:
 *      01/07/20
 *
 * \return int
 *
 * \retval   0   Success case: List converted
 *
 * \warning excludedNeighbors has to be previously initialized
 *
 * \par Notes:
 *           1. This function clear the previous excludedNeighbors list
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  01/07/20 | G. Gori         |  Initial Implementation and documentation.
 *****************************************************************************/
static int exclude_neighbors()
{
	int result = 0;
	free_list_elts(excludedNeighbors); //clear the previous list
	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 *      callUniboCGR
 *
 * \brief  Entry point to call the CGR from DTN2, get the best routes to reach
 *         the destination for the bundle.
 *
 *
 * \par Date Written:
 *      05/07/20
 *
 * \return int
 *
 * \retval    0   Routes found and converted
 * \retval   -1   There aren't route to reach the destination
 * \retval   -2   MWITHDRAW error
 * \retval   -3   Phase one arguments error
 * \retval   -4   CGR arguments error
 * \retval   -5   callCGR arguments error
 * \retval   -6   Can't create IonNode's routing object
 * \retval   -7   Bundle's conversion error
 * \retval   -8   NULL pointer during conversion to ION's contact
 *
 * \param[in]     time              The current time
 * \param[in]     *bundle           The DTN2's bundle that has to be forwarded
 * \param[out]    *matches          The list of best routes found
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  05/07/20 | G. Gori		    |  Initial Implementation and documentation.
 *****************************************************************************/
int callUniboCGR(time_t time, Bundle *bundle, string *res)
{

	int result = -5;
	List cgrRoutes = NULL;

	start_call_log(time - reference_time);

	debug_printf("Entry point interface.");

	if (initialized && bundle != NULL)
	{
		// INPUT CONVERSION: check if the contact plan has been changed, in affermative case update it
		result = update_contact_plan("", false);
		if (result != -2)
		{
			// INPUT CONVERSION: learn the bundle's characteristics and store them into the CGR's bundle struct
			result = convert_bundle_from_dtn2_to_cgr(time - reference_time, bundle, cgrBundle);
			if (result == 0)
			{
				debug_printf("Go to CGR.");
				// Call Unibo-CGR
				result = getBestRoutes(time - reference_time, cgrBundle, excludedNeighbors,
						&cgrRoutes);

				if (result > 0 && cgrRoutes != NULL)
				{
					// OUTPUT CONVERSION: convert the best routes into DTN2's CgrRoute and
					// put them into ION's Lyst
					result = convert_routes_from_cgr_to_dtn2(
							cgrBundle->evc, cgrRoutes, res);
					// ION's contacts MTVs are decreased by ipnfw

					if (result == -1)
					{
						result = -8;
					}
				}
			}
			else
			{
				result = -7;
			}
			reset_bundle(cgrBundle);
		}
	}

	debug_printf("result -> %d\n", result);

#if (LOG == 1)
	if (result < -1)
	{
		writeLog("Fatal error (interface): %d.", result);
	}
	end_call_log();
	// Log interactivity...
	log_fflush();
#endif
	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 *      computeApplicableBacklog
 *
 * \brief  Compute the applicable backlog (SABR 3.2.6.2 b) ) and the total backlog for a neighbor.
 *
 *
 * \par Date Written:
 *      20/07/20
 *
 * \return int
 *
 * \retval   0   Applicable and total backlog computed
 * \retval  -1   Arguments error
 * \retval  -2   Plan not found
 *
 * \param[in]   neighbor                   The neighbor for which we want to compute
 *                                         the applicable and total backlog.
 * \param[in]   priority                   The bundle's cardinal priority
 * \param[in]   ordinal                    The bundle's ordinal priority (used with expedited cardinal priority)
 * \param[out]  *CgrApplicableBacklog      The applicable backlog computed
 * \param[out]  *CgrTotalBacklog           The total backlog computed
 *
 * \par Notes:
 *          1. This function talks with the CL queue, that should be done during
 *             phase two to get a more accurate applicable (and total) backlog
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  20/07/20 | G. Gori         |  Initial Implementation and documentation.
 *****************************************************************************/
int computeApplicableBacklog(unsigned long long neighbor, int priority, unsigned int ordinal, CgrScalar *CgrApplicableBacklog,
		CgrScalar *CgrTotalBacklog)
{
	int result = -1;
	long int byteTot, byteApp, byteCount;
	Bundle * bundle;

	if (CgrApplicableBacklog != NULL && CgrTotalBacklog != NULL)
	{
		//get da implementare su UniboCGRBundleRouter
		RouteEntry* route = UniboCGRBundleRouter::getLinkForNode(neighbor);
		const LinkRef& link = route->link();
		//Deferred
		DeferredList* deferred = UniboCGRBundleRouter::deferred_list(link);
		oasys::ScopeLock l7(deferred->list()->lock(), 
                       "interface_unibocgr_dtn2::computeApplicableBacklog");
		BundleList::iterator iter = deferred->list()->begin();
		while(iter != deferred->list()->end())
		{
			bundle = *iter;
			//Capisci quanti byte occupa il bundle
			byteCount = 0;
			//per ora prendo solo la dim del payload, manca header che forse ha dim fissa?
			byteCount += bundle->durablesize();
			if(bundle->priority() >= priority)
			{
				//byte applicabili
				byteApp += byteCount;
			}
			//in ogni caso li aggiungo ai totali
			byteTot += byteCount;
		}
		//link queue
		size_t dimLinkQueue = link->queue()->size();
		if(dimLinkQueue > 0)
		{
			const BundleList* bl = link->queue();
			oasys::ScopeLock l8(bl->lock(),
										"interface_unibocgr_dtn2::computeApplicableBacklog2");
			BundleList::iterator bli;
			for(bli = bl->begin(); bli != bl->end(); ++iter)
			{
				bundle = *bli;
				byteCount = 0;
				//per ora prendo solo la dim del payload, manca header che forse ha dim fissa?
				byteCount += bundle->durablesize();
				if(bundle->priority() >= priority)
				{
					//byte applicabili
					byteApp += byteCount;
				}
				byteTot += byteCount;
				}
		}
		loadCgrScalar(CgrTotalBacklog, byteTot);
		loadCgrScalar(CgrApplicableBacklog, byteApp);
	}
	else result = -2;
	return result;




	/*CODICE ION
	Sdr sdr;
	char eid[SDRSTRING_BUFSZ];
	VPlan *vplan;
	PsmAddress vplanElt;
	Object planObj;
	BpPlan plan;
	Scalar IonApplicableBacklog, IonTotalBacklog;

	if (CgrApplicableBacklog != NULL && CgrTotalBacklog != NULL && IonBundle != NULL)
	{
		isprintf(eid, sizeof eid, "ipn:" UVAST_FIELDSPEC ".0", neighbor);
		sdr = getIonsdr();
		findPlan(eid, &vplan, &vplanElt);
		if (vplanElt != 0)
		{

			planObj = sdr_list_data(sdr, vplan->planElt);
			sdr_read(sdr, (char*) &plan, planObj, sizeof(BpPlan));
			if (!plan.blocked)
			{
				computePriorClaims(&plan, IonBundle, &IonApplicableBacklog, &IonTotalBacklog);

				convert_scalar_from_ion_to_cgr(&IonApplicableBacklog, CgrApplicableBacklog);
				convert_scalar_from_ion_to_cgr(&IonTotalBacklog, CgrTotalBacklog);

				result = 0;
			}
		}
		else
		{
			result = -2;
		}
	}

	return result;*/
}

/******************************************************************************
 *
 * \par Function Name:
 *      destroy_contact_graph_routing
 *
 * \brief  Deallocate all the memory used by the CGR.
 *
 *
 * \par Date Written:
 *      14/07/20
 *
 * \return  void
 *
 * \param[in]  time  The current time
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  14/07/20 | G. Gori		    |  Initial Implementation and documentation.
 *****************************************************************************/
void destroy_contact_graph_routing(time_t time)
{
	free_list(excludedNeighbors);
	excludedNeighbors = NULL;
	bundle_destroy(cgrBundle);
	cgrBundle = NULL;
	destroy_cgr(time - reference_time);
	initialized = 0;
	//IonBundle = NULL;

	reference_time = -1;

	return;
}

/******************************************************************************
 *
 * \par Function Name:
 *      initialize_contact_graph_routing
 *
 * \brief  Initialize all the data used by the CGR.
 *
 *
 * \par Date Written:
 *      01/07/20
 *
 * \return int
 *
 * \retval   1   Success case: CGR initialized
 * \retval  -1   Error case: ownNode can't be 0
 * \retval  -2   can't open or read file with contacts
 * \retval  -3   Error case: log directory can't be opened
 * \retval  -4   Error case: log file can't be opened
 * \retval  -5   Arguments error
 *
 * \param[in]   ownNode   The node that the CGR will consider as contacts graph's root
 * \param[in]   time      The reference unix time (time 0 for the CGR)
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  01/07/20 | G. Gori         |  Initial Implementation and documentation.
 *****************************************************************************/
int initialize_contact_graph_routing(unsigned long long ownNode, time_t time)
{
	int result = 1;
	char fileName[13] = "contatti.txt";
	if(ownNode != 0 && time >= 0)
	{
		printf("Initialize CGR arguments error\n");
		result = -5;
	}
	if (initialized != 1)
	{
		excludedNeighbors = list_create(NULL, NULL, NULL, MDEPOSIT_wrapper);
		cgrBundle = bundle_create();

		if (excludedNeighbors != NULL && cgrBundle != NULL)
		{
			result = initialize_cgr(0, ownNode);

			if (result == 1)
			{
				initialized = 1;
				reference_time = time;

				writeLog("Reference time (Unix time): %ld s.", (long int) reference_time);

				if(update_contact_plan(fileName) < 0)
				{
					printf("Cannot update contact plan in Unibo-CGR: can't open file");
					result = -2;
				}
			}
			else
			{
				printf("CGR initialize error: %d\n", result);
			}
		}
		else
		{
			free_list(excludedNeighbors);
			bundle_destroy(cgrBundle);
			excludedNeighbors = NULL;
			cgrBundle = NULL;
			result = -2;
		}
	}

	return result;
}




















