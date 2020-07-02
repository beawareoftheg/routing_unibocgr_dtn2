/** \file interface_cgr_ion.c
 *  
 *  \brief    This file provides the implementation of the functions
 *            that make this CGR's implementation compatible with ION.
 *	
 *  \details  In particular the functions that import the contact plan and the BpPlans.
 *            We provide in output the best routes list.
 *
 ** \copyright Copyright (c) 2020, Alma Mater Studiorum, University of Bologna, All rights reserved.
 *
 *  \author Lorenzo Persampieri, lorenzo.persampieri@studio.unibo.it
 *
 *  \par Supervisor
 *       Carlo Caini, carlo.caini@unibo.it
 */
#include "interface_cgr_ion.h"
#include <stdio.h>
#include <stdlib.h>
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

#ifndef GET_MTV_FROM_SDR
/**
 * \brief Set to 1 if you want to get the MTVs from ION's contact MTVs stored into SDR.
 *        Otherwise set to 0 (they will be computed as xmitRate*(toTime - fromTime)).
 *
 * \hideinitializer
 */
#define GET_MTV_FROM_SDR 1
#endif

#ifndef DEBUG_ION_INTERFACE
/**
 * \brief Boolean value used to enable some debug's print for this interface.
 * 
 * \details Set to 1 if you want to enable the debug's print, otherwise set to 0.
 *
 * \hideinitializer
 */
#define DEBUG_ION_INTERFACE 0
#endif

#ifndef STORE_ROUTES_IN_ION_SELECTED_ROUTES
/**
 * \brief Boolean value, set to 1 if you want to store the best routes in ION's selected routes.
 *        Set to 0 otherwise.
 *
 * \details Actually this is unneeded for ipnfw.
 *
 * \hideinitializer
 */
#define STORE_ROUTES_IN_ION_SELECTED_ROUTES 0
#endif

#define NOMINAL_PRIMARY_BLKSIZE	29 // from ION 4.0.0: bpv7/library/libbpP.c

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
 * \brief The current bundle received from ION.
 */
static Bundle *IonBundle = NULL;
/**
 * \brief CgrBundle used during the current call.
 */
static CgrBundle *cgrBundle = NULL;

#if(DEBUG_ION_INTERFACE == 1)
/******************************************************************************
 *
 * \par Function Name:
 * 		printDebugIonRoute
 *
 * \brief Print the "ION"'s route to standard output
 *
 *
 * \par Date Written:
 * 	    04/04/20
 *
 * \return void
 *
 * \param[in]   *route   The route that we want to print.
 *
 * \par Notes:
 *            1. This function print only the value of the route that
 *               will effectively used by ION.
 *            2. All the times are differential times from the reference time.
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY | AUTHOR          |  DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  04/04/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static void printDebugIonRoute(PsmPartition ionwm, CgrRoute *route)
{
	int stop = 0;
	Sdr sdr = getIonsdr();
	PsmAddress addr, addrContact;
	SdrObject contactObj;
	IonCXref *contact;
	IonContact contactBuf;
	time_t ref = reference_time;
	if (route != NULL)
	{
		fprintf(stdout, "\nPRINT ION ROUTE\n%-15s %-15s %-15s %-15s %-15s %-15s %s\n", "ToNodeNbr",
				"FromTime", "ToTime", "ETO", "PBAT", "MaxVolumeAvbl", "BundleECCC");
		fprintf(stdout, "%-15llu %-15ld %-15ld %-15ld %-15ld %-15g %lu\n",
				(unsigned long long) route->toNodeNbr, (long int) route->fromTime - ref,
				(long int) route->toTime - ref, (long int) route->eto - ref,
				(long int) route->pbat - ref, route->maxVolumeAvbl, (long unsigned int) route->bundleECCC);
		fprintf(stdout, "%-15s %-15s %-15s %-15s %-15s %s\n", "Confidence", "Hops", "Overbooked (G)",
				"Overbooked (U)", "Protected (G)", "Protected (U)");
		fprintf(stdout, "%-15.2f %-15ld %-15d %-15d %-15d %d\n", route->arrivalConfidence,
				(long int) sm_list_length(ionwm, route->hops), route->overbooked.gigs,
				route->overbooked.units, route->committed.gigs, route->committed.units);
		fprintf(stdout, "%-15s %-15s %-15s %-15s %-15s %-15s %-15s %-15s %s\n", "FromNode",
				"ToNode", "FromTime", "ToTime", "XmitRate", "Confidence", "MTV[Bulk]",
				"MTV[Normal]", "MTV[Expedited]");
		for (addr = sm_list_first(ionwm, route->hops); addr != 0 && !stop;
				addr = sm_list_next(ionwm, addr))
		{
			addrContact = sm_list_data(ionwm, addr);
			stop = 1;
			if (addrContact != 0)
			{
				contact = psp(ionwm, addrContact);
				if (contact != NULL)
				{
					stop = 0;
					contactObj = sdr_list_data(sdr, contact->contactElt);
					sdr_read(sdr, (char*) &contactBuf, contactObj, sizeof(IonContact));
					fprintf(stdout,
							"%-15llu %-15llu %-15ld %-15ld %-15lu %-15.2f %-15g %-15g %g\n",
							(unsigned long long) contact->fromNode,
							(unsigned long long) contact->toNode,
							(long int) contact->fromTime - ref, (long int) contact->toTime - ref,
							(long unsigned int) contact->xmitRate, contact->confidence, contactBuf.mtv[0],
							contactBuf.mtv[1], contactBuf.mtv[2]);
				}
				else
				{
					fprintf(stdout, "Contact: NULL.\n");
				}

			}
			else
			{
				fprintf(stdout, "PsmAddress: 0.\n");
			}
		}

		debug_fflush(stdout);

	}

	return;
}
#else
#define printDebugIonRoute(ionwm, route) do {  } while(0)
#endif


/******************************************************************************
 *
 * \par Function Name:
 *      convert_CtRegistration_from_ion_to_cgr
 *
 * \brief  Convert a Registration contact from ION to this CGR's implementation
 *
 *
 * \par Date Written:
 *     19/02/20
 *
 * \return int
 *
 * \retval   0   Success case
 * \retval  -1   Arguments error
 *
 * \param[in]    *IonContact   The Registration contact in ION.
 * \param[out]   *CgrContact   The Registration contact in this CGR's implementation.
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static int convert_CtRegistration_from_ion_to_cgr(IonCXref *IonContact, Contact *CgrContact)
{
	int result = -1;

	if (IonContact != NULL && CgrContact != NULL)
	{
		CgrContact->fromNode = IonContact->fromNode;
		CgrContact->toNode = IonContact->toNode;
		CgrContact->fromTime = MAX_POSIX_TIME;
		CgrContact->toTime = MAX_POSIX_TIME;
		CgrContact->type = Registration;
		CgrContact->xmitRate = 0;
		CgrContact->confidence = 1.0F;

		result = 0;
	}

	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 *      convert_CtScheduled_from_ion_to_cgr
 *
 * \brief  Convert a Scheduled contact from ION to this CGR's implementation
 *
 *
 * \par Date Written:
 *      19/02/20
 *
 * \return int
 *
 * \retval  0   Success case
 * \retval -1   Arguments error
 *
 * \param[in]    *IonContact   The Scheduled contact in ION.
 * \param[out]   *CgrContact   The Scheduled contact in this CGR's implementation.
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static int convert_CtScheduled_from_ion_to_cgr(IonCXref *IonContact, Contact *CgrContact)
{
	int result = -1;

	if (IonContact != NULL && CgrContact != NULL)
	{
		CgrContact->fromNode = IonContact->fromNode;
		CgrContact->toNode = IonContact->toNode;
		CgrContact->fromTime = IonContact->fromTime - reference_time;
		CgrContact->toTime = IonContact->toTime - reference_time;
		CgrContact->type = Scheduled;
		CgrContact->xmitRate = IonContact->xmitRate;
		CgrContact->confidence = IonContact->confidence;
		// TODO Consider to add MTVs

		result = 0;
	}

	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 *      convert_CtRegistration_from_cgr_to_ion
 *
 * \brief Convert a Registration contact from this CGR's implementation to ION
 *
 *
 * \par Date Written:
 *      19/02/20
 *
 * \return int
 *
 * \retval    0  Success case
 * \retval   -1  Arguments error
 *
 * \param[in]   *CgrContact   The Registration contact in this CGR's implementation.
 * \param[out]  *IonContact   The Registration contact in ION.
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static int convert_CtRegistration_from_cgr_to_ion(Contact *CgrContact, IonCXref *IonContact)
{
	int result = -1;

	if (IonContact != NULL && CgrContact != NULL)
	{
		IonContact->fromNode = CgrContact->fromNode;
		IonContact->toNode = CgrContact->toNode;
		IonContact->fromTime = MAX_POSIX_TIME;
		IonContact->toTime = MAX_POSIX_TIME;
		IonContact->type = CtRegistration;
		IonContact->xmitRate = 0;
		IonContact->confidence = 1.0F;

		result = 0;
	}

	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 *      convert_CtScheduled_from_cgr_to_ion
 *
 * \brief  Convert a Scheduled contact from this CGR's implementation to ION
 *
 *
 * \par Date Written:
 *      19/02/20
 *
 * \return int
 *
 * \retval   0	Success case
 * \retval  -1  Arguments error
 *
 * \param[in]    *CgrContact   The Scheduled contact in this CGR's implementation.
 * \param[out]   *IonContact   The Scheduled contact in ION.
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static int convert_CtScheduled_from_cgr_to_ion(Contact *CgrContact, IonCXref *IonContact)
{
	int result = -1;

	if (IonContact != NULL && CgrContact != NULL)
	{
		IonContact->fromNode = CgrContact->fromNode;
		IonContact->toNode = CgrContact->toNode;
		IonContact->fromTime = CgrContact->fromTime + reference_time;
		IonContact->toTime = CgrContact->toTime + reference_time;
		IonContact->type = CtScheduled;
		IonContact->xmitRate = CgrContact->xmitRate;
		IonContact->confidence = CgrContact->confidence;

		result = 0;
	}

	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 *      convert_contact_from_ion_to_cgr
 *
 * \brief Convert a contact from ION to this CGR's implementation
 *
 *
 * \par Date Written:
 *      19/02/20
 *
 * \return int
 *
 * \retval   0  Success case
 * \retval  -1  Arguments error
 * \retval  -2  Unknown contact's type
 *
 * \param[in]   *IonContact   The contact in ION.
 * \param[out]  *CgrContact   The contact in this CGR's implementation.
 *
 * \par	Notes:
 *             1.  Only Registration and Scheduled contacts are allowed.
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static int convert_contact_from_ion_to_cgr(IonCXref *IonContact, Contact *CgrContact)
{
	int result = -1;
	if (IonContact != NULL && CgrContact != NULL)
	{
		// TODO consider other types of contacts,
		// TODO but SABR talks about only scheduled contacts
		if (IonContact->type == CtRegistration)
		{
			result = convert_CtRegistration_from_ion_to_cgr(IonContact, CgrContact);
		}
		else if (IonContact->type == CtScheduled)
		{
			result = convert_CtScheduled_from_ion_to_cgr(IonContact, CgrContact);
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
 *      convert_contact_from_cgr_to_ion
 *
 * \brief Convert a contact from this CGR's implementation to ION
 *
 *
 * \par Date Written:
 *      19/02/20
 *
 * \return int
 *
 * \retval  0  Success case
 * \retval -1  Arguments error
 * \retval  -2  Unknown contact's type
 *
 * \param[in]    *CgrContact   The contact in this CGR's implementation.
 * \param[out]   *IonContact   The contact in ION.
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static int convert_contact_from_cgr_to_ion(Contact *CgrContact, IonCXref *IonContact)
{
	int result = -1;
	if (IonContact != NULL && CgrContact != NULL)
	{
		// TODO consider other types of contacts,
		// TODO but SABR talks about only scheduled contacts
		if (CgrContact->type == Registration)
		{
			result = convert_CtRegistration_from_cgr_to_ion(CgrContact, IonContact);
		}
		else if (CgrContact->type == Scheduled)
		{
			result = convert_CtScheduled_from_cgr_to_ion(CgrContact, IonContact);
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
 *      convert_range_from_ion_to_cgr
 *
 * \brief  Convert a range from ION to this CGR's implementation
 *
 *
 * \par Date Written:
 *      19/02/20
 *
 * \return int
 *
 * \retval   0   Success case
 * \retval  -1   Arguments error
 *
 * \param[in]    *IonRange  The range in ION.
 * \param[out]   *CgrRange  The range in this CGR's implementation.
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static int convert_range_from_ion_to_cgr(IonRXref *IonRange, Range *CgrRange)
{
	int result = -1;

	if (IonRange != NULL && CgrRange != NULL)
	{
		CgrRange->fromNode = IonRange->fromNode;
		CgrRange->toNode = IonRange->toNode;
		CgrRange->fromTime = IonRange->fromTime - reference_time;
		CgrRange->toTime = IonRange->toTime - reference_time;
		CgrRange->owlt = IonRange->owlt;

		result = 0;
	}

	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 *      convert_range_from_cgr_to_ion
 *
 * \brief  Convert a range from this CGR's implementation to ION
 *
 *
 * \par Date Written:
 *      19/02/20
 *
 * \return int
 *
 * \retval  0  Success case
 * \retval -1  Arguments error
 *
 * \param[in]   *CgrRange   The range in this CGR's implementation.
 * \param[out]  *IonRange   The range in ION.
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static int convert_range_from_cgr_to_ion(Range *CgrRange, IonRXref *IonRange)
{
	int result = -1;

	if (IonRange != NULL && CgrRange != NULL)
	{
		IonRange->fromNode = CgrRange->fromNode;
		IonRange->toNode = CgrRange->toNode;
		IonRange->fromTime = CgrRange->fromTime + reference_time;
		IonRange->toTime = CgrRange->toTime + reference_time;
		IonRange->owlt = CgrRange->owlt;

		result = 0;
	}

	return result;
}

#if (CGR_AVOID_LOOP > 0)
/******************************************************************************
 *
 * \par Function Name:
 *      get_rgr_ext_block
 *
 * \brief  Get the GeoRoute stored into RGR Extension Block
 *
 *
 * \par Date Written:
 *      23/04/20
 *
 * \return int
 *
 * \retval  0  Success case: GeoRoute found
 * \retval -1  GeoRoute not found
 * \retval -2  System error
 *
 * \param[in]   *bundle    The bundle that contains the RGR Extension Block
 * \param[out]  *resultBlk The GeoRoute extracted from the RGR Extension Block, only in success case.
 *                         The resultBLk must be allocated by the caller.
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
static int get_rgr_ext_block(Bundle *bundle, GeoRoute *resultBlk)
{
	Sdr sdr = getIonsdr();
	int result = 0;
	Object extBlockElt;
	Address extBlkAddr;

	OBJ_POINTER(ExtensionBlock, blk);

	/* Step 1 - Check for the presence of RGR extension*/

	if (!(extBlockElt = findExtensionBlock(bundle, RGRBlk, 0, 0, 0)))
	{
		result = -1;
	}
	else
	{
		/* Step 2 - Get deserialized version of RGR extension block*/

		extBlkAddr = sdr_list_data(sdr, extBlockElt);

		GET_OBJ_POINTER(sdr, ExtensionBlock, blk, extBlkAddr);

		result = rgr_read(blk, resultBlk);

		if(result == -1)
		{
			result = -2; // system error
		}
		else if(result < -1)
		{
			result = -1; // geo route not found
		}
		else
		{
			result = 0; // geo route found
		}
	}

	return result;
}
#endif

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
 *      convert_bundle_from_ion_to_cgr
 *
 * \brief Convert the characteristics of the bundle from ION to
 *        this CGR's implementation and initialize all the
 *        bundle fields used by the CGR.
 *
 *
 * \par Date Written:
 *      19/02/20
 *
 * \return int
 *
 * \retval  0  Success case
 * \retval -1  Arguments error
 * \retval -2  MWITHDRAW error
 *
 * \param[in]    toNode             The destination ipn node for the bundle
 * \param[in]    current_time       The current time, in differential time from reference_time
 * \param[in]    *IonBundle         The bundle in ION
 * \param[out]   *CgrBundle         The bundle in this CGR's implementation.
 *
 * \par	Notes:
 *             1. This function prints the ID of the bundle in the main log file.
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *  24/03/20 | L. Persampieri  |  Added geoRouteString
 *****************************************************************************/
static int convert_bundle_from_ion_to_cgr(unsigned long long toNode, time_t current_time, Bundle *IonBundle, CgrBundle *CgrBundle)
{
	int result = -1;
	time_t offset;
#if (MSR == 1)
	CGRRouteBlock *cgrrBlk;
#endif
#if (CGR_AVOID_LOOP > 0)
	GeoRoute geoRoute;
#endif

	if (IonBundle != NULL && CgrBundle != NULL)
	{

		CgrBundle->terminus_node = toNode;

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
#endif
		if (result != -2)
		{
			CLEAR_FLAGS(CgrBundle->flags); //reset previous mask

            if(IonBundle->ancillaryData.flags & BP_MINIMUM_LATENCY)
            {
            	SET_CRITICAL(CgrBundle);
            }

			if (!(IS_CRITICAL(CgrBundle)) && IonBundle->returnToSender)
			{
				SET_BACKWARD_PROPAGATION(CgrBundle);
			}
			if(!(IonBundle->bundleProcFlags & BDL_DOES_NOT_FRAGMENT))
			{
				SET_FRAGMENTABLE(CgrBundle);
			}

			//TODO search probe field in ION's bundle...

			CgrBundle->ordinal = (unsigned int) IonBundle->ordinal;

			//size computation ported by ION 4.0.0
			CgrBundle->size = NOMINAL_PRIMARY_BLKSIZE + IonBundle->extensionsLength
					+ IonBundle->payload.length;

			CgrBundle->evc = computeBundleEVC(CgrBundle->size); // SABR 2.4.3

			offset = IonBundle->id.creationTime.seconds + EPOCH_2000_SEC - reference_time;

			CgrBundle->expiration_time = IonBundle->expirationTime
					- IonBundle->id.creationTime.seconds + offset;
			CgrBundle->sender_node = IonBundle->clDossier.senderNodeNbr;
			CgrBundle->priority_level = IonBundle->priority;

			CgrBundle->dlvConfidence = IonBundle->dlvConfidence;

			// We don't need ID during CGR so we just print it into log file.
			print_log_bundle_id((unsigned long long ) IonBundle->id.source.ssp.ipn.nodeNbr,
					IonBundle->id.creationTime.seconds, IonBundle->id.creationTime.count,
					IonBundle->totalAduLength, IonBundle->id.fragmentOffset);
			writeLog("Payload length: %u.", IonBundle->payload.length);

			result = 0;
		}
	}

	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 *      convert_scalar_from_ion_to_cgr
 *
 * \brief  Convert a scalar type from ION to this CGR's implementation.
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
 * \param[in]   *ion_scalar   The scalar type in ION.
 * \param[out]  *cgr_scalar   The scalar type in this CGR's implementation
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static int convert_scalar_from_ion_to_cgr(Scalar *ion_scalar, CgrScalar *cgr_scalar)
{
	int result = -1;

	if (ion_scalar != NULL && cgr_scalar != NULL)
	{
		cgr_scalar->gigs = ion_scalar->gigs;
		cgr_scalar->units = ion_scalar->units;
		result = 0;
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
 *****************************************************************************/
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
}

/******************************************************************************
 *
 * \par Function Name:
 *      convert_hops_list_from_cgr_to_ion
 *
 * \brief  Convert a list of contacts from this CGR's implementation to ION.
 * 
 * \details  This function is thinked to convert the hops list of a Route
 *           and only for this scope.
 *
 *
 * \par Date Written:
 *      19/02/20
 *
 * \return  int
 * 
 * \retval  ">= 0"  Number of contacts converted
 * \retval     -1   CGR's contact NULL
 * \retval     -2   Memory allocation error
 * \retval     -3   Contact not found in the ION's contacts graph
 *
 * \param[in]   ionwm      The partition of the ION's contacts graph
 * \param[in]   *ionvdb    The ion's volatile database
 * \param[in]   CgrHops    The list of contacts of this CGR's implementation
 * \param[out]  IonHops    The list of contact in ION's format.
 *
 * \warning ionvdb doesn't have to be NULL
 * \warning CgrHops doesn't have to be NULL
 * \warning IonHops doesn't have to be 0
 *
 * \par	Notes:
 *                1.    All the contacts will be searched in the ION's contacts graph, and
 *                      then the contact found will be added in the list.
 *                2.    The ION's list mantains the same order of the CGR's list.
 *                3.    To the ION's contact this function adds the citations to the
 *                      hop of the list where is the contact.
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static int convert_hops_list_from_cgr_to_ion(PsmPartition ionwm, IonVdb *ionvdb, List CgrHops,
		PsmAddress IonHops)
{
	ListElt *elt;
	IonCXref IonContact, *IonTreeContact;
	PsmAddress tree_node, citation, contactAddr;
	int result = 0;

	for (elt = CgrHops->first; elt != NULL && result >= 0; elt = elt->next)
	{
		if (convert_contact_from_cgr_to_ion((Contact*) elt->data, &IonContact) == 0)
		{
			tree_node = sm_rbt_search(ionwm, ionvdb->contactIndex, rfx_order_contacts, &IonContact,
					0);
			if (tree_node != 0)
			{
				contactAddr = sm_rbt_data(ionwm, tree_node);
				IonTreeContact = (IonCXref*) psp(ionwm, contactAddr);
				if (IonTreeContact != NULL)
				{
					citation = sm_list_insert_last(ionwm, IonHops, contactAddr);

					if (citation == 0)
					{
						result = -2;
					}
					else
					{
						result++;

						if (IonTreeContact->citations == 0)
						{
							IonTreeContact->citations = sm_list_create(ionwm);
							if (IonTreeContact->citations == 0)
							{
								result = -2;
							}
						}

						if (result != -2)
						{
							if (sm_list_insert_last(ionwm, IonTreeContact->citations, citation)
									== 0)
							{
								result = -2;
							}
							else
							{
								result++;
							}
						}
					}
				}
				else
				{
					result = -3;
				}
			}
			else
			{
				result = -3;
			}
		}
		else
		{
			result = -1;
		}
	}

	return result;
}

#if (STORE_ROUTES_IN_ION_SELECTED_ROUTES)
/******************************************************************************
 *
 * \par Function Name:
 *      search_route_in_ion_selected_routes
 *
 * \brief   Search a route in the selectedRoutes of the ION's routing object of the terminus node.
 *          The route must have an equal hops list.
 *
 *
 * \par Date Written:
 *      19/02/20
 *
 * \return int
 *
 * \retval   0  Route found
 * \retval  -1  Route not found
 *
 * \param[in]    ionwm        The partition of the ION's contacts graph
 * \param[in]    *route       The CGR's route that we want to search in ION
 * \param[in]    *rtgObj      The routing object where we search the route
 * \param[out]   **IonRoute   The route found (only if result == 0)
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static int search_route_in_ion_selected_routes(PsmPartition ionwm, Route *route,
		CgrRtgObject *rtgObj, CgrRoute **IonRoute)
{
	int result = -1, found, stop;
	PsmAddress selElt, hopEltSelected, addr;
	ListElt *hopEltCgr;
	CgrRoute *selectedRoute;
	IonCXref *contactSelected;
	Contact *contactCgr;

	if (route != NULL && rtgObj != NULL)
	{
		found = 0;
		for (selElt = sm_list_first(ionwm, rtgObj->selectedRoutes); selElt && !found; selElt =
				sm_list_next(ionwm, selElt))
		{
			selectedRoute = psp(ionwm, sm_list_data(ionwm, selElt));
			if (selectedRoute != NULL)
			{
				if (route->neighbor == selectedRoute->toNodeNbr)
				{
					if (route->hops->length == sm_list_length(ionwm, selectedRoute->hops))
					{
						hopEltSelected = sm_list_first(ionwm, selectedRoute->hops);
						stop = 0;
						for (hopEltCgr = route->hops->first; hopEltCgr != NULL && !stop; hopEltCgr =
								hopEltCgr->next)
						{
							contactCgr = (Contact*) hopEltCgr->data;
							addr = sm_list_data(ionwm, hopEltSelected);
							if (addr != 0)
							{
								contactSelected = (IonCXref*) psp(ionwm, addr);

								if (contactCgr != NULL && contactSelected != NULL)
								{
									if (contactCgr->fromNode != contactSelected->fromNode
											|| contactCgr->toNode != contactSelected->toNode
											|| ((contactCgr->fromTime + reference_time)
													!= contactSelected->fromTime))
									{
										stop = 1;
									}
								}
								else
								{
									stop = 1;
								}

								hopEltSelected = sm_list_next(ionwm, hopEltSelected);
							}
							else
							{
								stop = 1;
							}
						}

						if (!stop)
						{
							found = 1;
							*IonRoute = psp(ionwm, sm_list_data(ionwm, selElt));
							result = 0;
						}

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
 *      convert_routes_from_cgr_to_ion
 *
 * \brief Convert a list of routes from CGR's format to ION's format
 *
 *
 * \par Date Written: 
 *      19/02/20
 *
 * \return int 
 *
 * \retval   0  All routes converted
 * \retval  -1  CGR's contact points to NULL
 * \retval  -2  Memory allocation error
 *
 * \param[in]     ionwm           The partition of the ION's contacts graph
 * \param[in]     *ionvdb         The ION's volatile database
 * \param[in]     *terminusNode   The node for which the routes have been computed
 * \param[in]     evc             Bundle's estimated volume consumption
 * \param[in]     cgrRoutes       The list of routes in CGR's format
 * \param[out]    IonRoutes       The list converted (only if return value is 0)
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static int convert_routes_from_cgr_to_ion(PsmPartition ionwm, IonVdb *ionvdb, IonNode *terminusNode,
		long unsigned int evc, List cgrRoutes, Lyst IonRoutes)
{
	ListElt *elt;
	PsmAddress addr, hops;
	CgrRoute *IonRoute = NULL;
	Route *current;
#if (STORE_ROUTES_IN_ION_SELECTED_ROUTES)
	CgrRtgObject *rtgObj = (CgrRtgObject*) psp(ionwm, terminusNode->routingObject);
	PsmAddress ref;
#endif

	int result = 0;

	for (elt = cgrRoutes->first; elt != NULL && result >= 0; elt = elt->next)
	{
		if (elt->data != NULL)
		{
			current = (Route*) elt->data;
#if (STORE_ROUTES_IN_ION_SELECTED_ROUTES)
			// TODO currently it is only a cost that doesn't introduce benefits...
			// TODO consider to remove this search into ION's selectedRoutes...
			if (search_route_in_ion_selected_routes(ionwm, current, rtgObj, &IonRoute) == 0)
			{
				//update values for the forwarding of the current bundle
				IonRoute->arrivalTime = current->arrivalTime + reference_time;
				IonRoute->maxVolumeAvbl = current->routeVolumeLimit;
				IonRoute->bundleECCC = evc;
				IonRoute->eto = current->eto + reference_time;
				IonRoute->pbat = current->pbat + reference_time;
				convert_scalar_from_cgr_to_ion(&(current->protected), &(IonRoute->committed));
				convert_scalar_from_cgr_to_ion(&(current->overbooked), &(IonRoute->overbooked));

				printDebugIonRoute(ionwm, IonRoute);
				if (lyst_insert_last(IonRoutes, (void *) IonRoute) == NULL)
				{
					result = -2;
				}
			}
			else
			{
#endif
				addr = psm_zalloc(ionwm, sizeof(CgrRoute));
				hops = sm_list_create(ionwm);

				if (addr != 0 && hops != 0)
				{
#if (STORE_ROUTES_IN_ION_SELECTED_ROUTES)
					// TODO currently it is only a cost that doesn't introduce benefits...
					// TODO consider to remove this insertion into ION's selectedRoutes
					ref = sm_list_insert_last(ionwm, rtgObj->selectedRoutes, addr);
					IonRoute->referenceElt = ref;
					if (ref != 0)
					{
#endif
						IonRoute = (CgrRoute*) psp(ionwm, addr);
						memset((char*) IonRoute, 0, sizeof(CgrRoute));
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
						if (convert_hops_list_from_cgr_to_ion(ionwm, ionvdb, current->hops, hops)
								>= 0)
						{
							IonRoute->hops = hops;
							printDebugIonRoute(ionwm, IonRoute);
							if (lyst_insert_last(IonRoutes, (void*) IonRoute) == NULL)
							{
								result = -2;
							}
						}
						else
						{
							result = -2;
							removeRoute(ionwm, addr);
						}
#if (STORE_ROUTES_IN_ION_SELECTED_ROUTES)
					}
					else
					{
						result = -2;
					}
#endif
				}
				else
				{
					result = -2;
				}
#if (STORE_ROUTES_IN_ION_SELECTED_ROUTES)
			}
#endif
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
 *      convert_PsmAddress_to_IonCXref
 *
 * \brief  Convert a PsmAddress to ION's contact type
 *
 *
 * \par Date Written:
 *      19/02/20
 *
 * \return IonCXref*
 *
 * \retval  IonCXref*	The contact converted
 * \retval  NULL        If the address is a NULL pointer
 *
 * \param[in]  ionwm     The partition of the ION's contacts graph
 * \param[in]  address   The address of the contact in the partition
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static IonCXref* convert_PsmAddress_to_IonCXref(PsmPartition ionwm, PsmAddress address)
{
	IonCXref *contact = NULL;

	if (address != 0)
	{
		contact = psp(ionwm, address);
	}

	return contact;
}

/******************************************************************************
 *
 * \par Function Name:
 *      convert_PsmAddress_to_IonRXref
 *
 * \brief Convert a PsmAddress to ION's range type
 *
 *
 * \par Date Written:
 *      19/02/20
 *
 * \return IonRXref*
 *
 * \retval  IonRXref*  The contact converted
 * \retval  NULL       If the address is a NULL pointer
 *
 * \param[in]   ionwm     The partition of the ION's ranges graph
 * \param[in]   address   The address of the range in the partition
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static IonRXref* convert_PsmAddress_to_IonRXref(PsmPartition ionwm, PsmAddress address)
{
	IonRXref *range = NULL;

	if (address != 0)
	{
		range = psp(ionwm, address);
	}

	return range;
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
 *      19/02/20
 *
 * \return int
 *
 * \retval   1   Contact added
 * \retval   0   Contact's arguments error
 * \retval  -1   The contact overlaps with other contacts
 * \retval  -2   MWITHDRAW error
 * \retval  -3   ION's contact points to NULL
 *
 * \param[in]   *IonContact  The contact in ION's format
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static int add_contact(IonCXref *ContactInION)
{
	Contact CgrContact;
	int result;
	double mtv[3];
#if (GET_MTV_FROM_SDR)
	Sdr sdr = getIonsdr();
	Object contactObj;
	IonContact contactBuf;
#endif

	result = convert_contact_from_ion_to_cgr(ContactInION, &CgrContact);

	if (result == 0)
	{
		if (CgrContact.type == Registration)
		{
			CgrContact.fromTime = -1;
		}

#if (GET_MTV_FROM_SDR)
		// TODO CONSIDER TO MOVE MTVs INTO CONVERT CONTACT FUNCTION
		sdr = getIonsdr();
		contactObj = sdr_list_data(sdr, ContactInION->contactElt);
		sdr_read(sdr, (char *) &contactBuf, contactObj, sizeof(IonContact));
		mtv[0] = contactBuf.mtv[0];
		mtv[1] = contactBuf.mtv[1];
		mtv[2] = contactBuf.mtv[2];

		// Try to add contact
		// Use the MTV passed as argument
		result = addContact(CgrContact.fromNode, CgrContact.toNode, CgrContact.fromTime,
				CgrContact.toTime, CgrContact.xmitRate, CgrContact.confidence, 1, mtv);
		if(result >= 1)
		{
			// result == 2 : xmitRate revised, consider it as "new contact"
			result = 1;
		}
#else
		// Initialize to known value...
		mtv[0] = 0;
		mtv[1] = 0;
		mtv[2] = 0;

		// Try to add contact
		// Compute MTV as [xmitRate * (toTime - fromTime)]
		result = addContact(CgrContact.fromNode, CgrContact.toNode, CgrContact.fromTime,
				CgrContact.toTime, CgrContact.xmitRate, CgrContact.confidence, 0, mtv);
		if(result >= 1)
		{
			// result == 2 : xmitRate revised, consider it as "new contact"
			result = 1;
		}
#endif

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
 *      add_new_contacts
 *
 * \brief  Add all new contacts of the ION's contacts graph to the
 *         contacts graph of thic CGR's implementation.
 *
 * \details Only for Registration and Scheduled contacts.
 *
 *
 * \par Date Written:
 *      19/02/20
 *
 * \return int
 *
 * \retval  ">= 0"  Number of contacts added to the contacts graph
 * \retval     -2   MWITHDRAW error
 *
 * \param[in]   ionwm    The ION's contacts graph partition
 * \param[in]   *ionvdb  The ION's volatile database
 *
 * \warning ionvdb doesn't have to be NULL.
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static int add_new_contacts(PsmPartition ionwm, IonVdb *ionvdb)
{
	int result = 0, totAdded = 0, stop = 0;
	PsmAddress nodeAddr;
	IonCXref *currentIonContact = NULL;

	for (nodeAddr = sm_rbt_first(ionwm, ionvdb->contactIndex); nodeAddr != 0 && !stop; nodeAddr =
			sm_rbt_next(ionwm, nodeAddr))
	{
		currentIonContact = convert_PsmAddress_to_IonCXref(ionwm, sm_rbt_data(ionwm, nodeAddr));

		if (currentIonContact->type == CtRegistration || currentIonContact->type == CtScheduled)
		{
			result = add_contact(currentIonContact);

			if (result == 1)
			{
				totAdded++;
			}
			else if (result == -2) //MWITHDRAW error
			{
				stop = 1;
			}
		}
	}

	if (!stop)
	{
		result = totAdded;
	}

	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 *      remove_deleted_contacts
 *
 * \brief  Remove all the contacts that are not anymore
 *         in the ION's contacts graph from the contacts graph
 *         of this CGR's implementation.
 *
 *
 * \par Date Written:
 *      19/02/20
 *
 * \return int
 *
 * \retval  ">= 0"  Number of contacts deleted from the contacts graph
 *
 * \param[in]  ionwm     The ION's contacts graph partition
 * \param[in]  *ionvdb   The ION's volatile database
 *
 * \warning ionvdb doesn't have to be NULL.
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static int remove_deleted_contacts(PsmPartition ionwm, IonVdb *ionvdb)
{
	int result = 0;
	Contact *CgrContact, *nextCgrContact;
	RbtNode *node;
	IonCXref IonContact;

	CgrContact = get_first_contact(&node);
	while (CgrContact != NULL)
	{
		nextCgrContact = get_next_contact(&node);
		convert_contact_from_cgr_to_ion(CgrContact, &IonContact);

		if (sm_rbt_search(ionwm, ionvdb->contactIndex, rfx_order_contacts, &IonContact, 0) == 0)
		{
			remove_contact_elt_from_graph(CgrContact);
			result++;
		}
		CgrContact = nextCgrContact;
	}

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
 *      19/02/20
 *
 * \return int
 *
 * \retval   1   Range added
 * \retval   0   Range's arguments error
 * \retval  -1   The range overlaps with other ranges
 * \retval  -2   MWITHDRAW error
 * \retval  -3   ION's range points to NULL
 *
 * \param[in]  *IonRange  The range in ION's format
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static int add_range(IonRXref *IonRange)
{
	Range CgrRange;
	int result;

	result = convert_range_from_ion_to_cgr(IonRange, &CgrRange);

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
 *      add_new_rangess
 *
 * \brief  Add all new ranges of the ION's ranges graph to the
 *         ranges graph of thic CGR's implementation.
 *
 *
 * \par Date Written:
 *      19/02/20
 *
 * \return int
 * 
 * \retval  ">= 0"   Number of ranges added to the ranges graph
 * \retval     -2    MWITHDRAW error
 *
 * \param[in]  ionwm     The ION's ranges graph partition
 * \param[in]  *ionvdb   The ION's volatile database
 *
 * \warning ionvdb doesn't have to be NULL.
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static int add_new_ranges(PsmPartition ionwm, IonVdb *ionvdb)
{
	int result = 0, totAdded = 0, stop = 0;
	PsmAddress nodeAddr;
	IonRXref *currentIonRange = NULL;

	for (nodeAddr = sm_rbt_first(ionwm, ionvdb->rangeIndex); nodeAddr != 0 && !stop; nodeAddr =
			sm_rbt_next(ionwm, nodeAddr))
	{
		currentIonRange = convert_PsmAddress_to_IonRXref(ionwm, sm_rbt_data(ionwm, nodeAddr));
		result = add_range(currentIonRange);

		if (result >= 0)
		{
			totAdded++;
		}
		else if (result == -2) //MWITHDRAW error
		{
			stop = 1;
		}
	}

	if (!stop)
	{
		result = totAdded;
	}

	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 *      remove_deleted_contacts
 *
 * \brief  Remove all the ranges that are not anymore
 *         in the ION's ranges graph from the ranges graph
 *         of this CGR's implementation.
 *
 *
 * \par Date Written:
 *      19/02/20
 *
 * \return int
 *
 * \retval  ">= 0"  Number of ranges deleted from the ranges graph
 *
 * \param[in]   ionwm    The ION's ranges graph partition
 * \param[in]   *ionvdb  The ION's volatile database
 *
 * \warning ionvdb doesn't have to be NULL.
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static int remove_deleted_ranges(PsmPartition ionwm, IonVdb *ionvdb)
{
	int result = 0;
	Range *CgrRange, *nextCgrRange;
	RbtNode *node;
	IonRXref IonRange;

	CgrRange = get_first_range(&node);
	while (CgrRange != NULL)
	{
		nextCgrRange = get_next_range(&node);
		convert_range_from_cgr_to_ion(CgrRange, &IonRange);

		if (sm_rbt_search(ionwm, ionvdb->rangeIndex, rfx_order_ranges, &IonRange, 0) == 0)
		{
			remove_range_elt_from_graph(CgrRange);
			result++;
		}
		CgrRange = nextCgrRange;
	}

	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 *      update_contact_plan
 *
 * \brief  Check if the ION's contact plan changed and in affermative case
 *         update the CGR's contact plan.
 *
 *
 * \par Date Written:
 *      19/02/20
 *
 * \return int
 *
 * \retval   0  Contact plan has been changed and updated
 * \retval  -1  Contact plan isn't changed
 * \retval  -2  MWITHDRAW error
 *
 * \param[in]   ionwm     The ION's contact plan partition
 * \param[in]   *ionvdb   The ION's volatile database
 *
 * \warning ionvdb doesn't have to be NULL.
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static int update_contact_plan(PsmPartition ionwm, IonVdb *ionvdb)
{
	int result = -1;
	int result_contacts, result_ranges;

	if (ionvdb->lastEditTime.tv_sec > contactPlanEditTime.tv_sec
			|| (ionvdb->lastEditTime.tv_sec == contactPlanEditTime.tv_sec
					&& ionvdb->lastEditTime.tv_usec > contactPlanEditTime.tv_usec))
	{

		writeLog("#### Contact plan modified ####");

		// TODO consider to remove all contacts and ranges and then copy the ION's RBTs
		// TODO maybe it is more efficient
		// TODO (so discard all routes and nodes here)

		result_contacts = remove_deleted_contacts(ionwm, ionvdb);
		result_ranges = remove_deleted_ranges(ionwm, ionvdb);
#if (LOG == 1)
		if (result_contacts > 0)
		{
			writeLog("Deleted %d contacts.", result_contacts);
		}
		if (result_ranges > 0)
		{
			writeLog("Deleted %d ranges.", result_ranges);
		}
#endif
		result_contacts = add_new_contacts(ionwm, ionvdb);
		result_ranges = add_new_ranges(ionwm, ionvdb);
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
		if (result_contacts == -2 || result_ranges == -2) //MWITHDRAW error
		{
			result = -2;
		}
		else
		{
			result = 0;
		}

		contactPlanEditTime.tv_sec = ionvdb->lastEditTime.tv_sec;
		contactPlanEditTime.tv_usec = ionvdb->lastEditTime.tv_usec;

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
 * \brief  Exclude all the neighbors that can't be used as first hop
 *         to reach the destination.
 *
 *
 * \par Date Written:
 *      19/02/20
 *
 * \return int
 *
 * \retval   0   Success case: List converted
 * \retval  -2   MWITHDRAW error
 *
 * \param[in]    ionwm           The ION's memory partition
 * \param[in]    *terminusNode   The destination node for the bundle
 *
 * \warning terminusNode doesn't have to be NULL
 * \warning excludedNeighbors has to be previously initialized
 *
 * \par Notes:
 *           1. This function clear the previous excludedNeighbors list
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
static int exclude_neighbors(PsmPartition ionwm, IonNode *terminusNode)
{
	// extracted from a function of ION 3.7.0 and adapted to use
	// our list library implementation

	PsmAddress embElt;
	Embargo *embargo;
	unsigned long long *node;
	int result = 0;

	free_list_elts(excludedNeighbors); //clear the previous list

	for (embElt = sm_list_first(ionwm, terminusNode->embargoes); embElt != 0 && result >= 0;
			embElt = sm_list_next(ionwm, embElt))
	{
		embargo = (Embargo*) psp(ionwm, sm_list_data(ionwm, embElt));

		if (!(embargo->probeIsDue))
		{
			/*	(Omit the embargoed node from the list
			 *	of excluded nodes if it's now time to
			 *	probe that node for renewed acceptance
			 *	of bundles destined for this destination
			 *	node.)					*/
			node = (unsigned long long*) MWITHDRAW(sizeof(unsigned long long));
			if (node != NULL)
			{
				*node = embargo->nodeNbr;
				if (list_insert_last(excludedNeighbors, node) == NULL)
				{
					MDEPOSIT(node);
					result = -2;
				}
				else
				{
					result++;
				}
			}
		}
	}

	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 *      callCGR
 *
 * \brief  Entry point to call the CGR from ION, get the best routes to reach
 *         the destination for the bundle.
 *
 *
 * \par Date Written:
 *      19/02/20
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
 * \param[in]     *ionvdb           The ION's volatile database
 * \param[in]     ionwm             The ION's memory partition
 * \param[in]     *cgrvdb           The ION's CGR volatile database
 * \param[in]     *bundle           The ION's bundle that has to be forwarded
 * \param[in]     *terminusNode     The destination node for the bundle
 * \param[out]    IonRoutes         The list of best routes found
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
int callCGR(time_t time, IonVdb *ionvdb, PsmPartition ionwm, CgrVdb *cgrvdb, Bundle *bundle,
		IonNode *terminusNode, Lyst IonRoutes)
{
	int result = -5;
	List cgrRoutes = NULL;

	start_call_log(time - reference_time);

	debug_printf("Entry point interface.");

	if (initialized && bundle != NULL && terminusNode != NULL && ionwm != NULL && ionvdb != NULL && cgrvdb != NULL)
	{
		// INPUT CONVERSION: check if the contact plan has been changed, in affermative case update it
		result = update_contact_plan(ionwm, ionvdb);
		if (result != -2)
		{
			result = create_ion_node_routing_object(terminusNode, ionwm, cgrvdb);
			if (result == 0)
			{
				// INPUT CONVERSION: learn the bundle's characteristics and store them into the CGR's bundle struct
				result = convert_bundle_from_ion_to_cgr(terminusNode->nodeNbr, time - reference_time, bundle, cgrBundle);
				if (result == 0)
				{
					// Check for embargoes...
					result = exclude_neighbors(ionwm, terminusNode);
					if (result >= 0)
					{
						IonBundle = bundle;
						debug_printf("Go to CGR.");
						// Call Unibo-CGR
						result = getBestRoutes(time - reference_time, cgrBundle, excludedNeighbors,
								&cgrRoutes);

						if (result > 0 && cgrRoutes != NULL)
						{
							// OUTPUT CONVERSION: convert the best routes into ION's CgrRoute and
							// put them into ION's Lyst
							result = convert_routes_from_cgr_to_ion(ionwm, ionvdb, terminusNode,
									cgrBundle->evc, cgrRoutes, IonRoutes);
							// ION's contacts MTVs are decreased by ipnfw

							if (result == -1)
							{
								result = -8;
							}
						}
					}
				}
				else
				{
					result = -7;
				}

				reset_bundle(cgrBundle);
			}
			else
			{
				result = -6;
			}
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
 *      19/02/20
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
 *  19/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
int computeApplicableBacklog(unsigned long long neighbor, int priority, unsigned int ordinal, CgrScalar *CgrApplicableBacklog,
		CgrScalar *CgrTotalBacklog)
{
	int result = -1;
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

	return result;
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
 *      19/02/20
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
 *  19/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
void destroy_contact_graph_routing(time_t time)
{
	free_list(excludedNeighbors);
	excludedNeighbors = NULL;
	bundle_destroy(cgrBundle);
	cgrBundle = NULL;
	destroy_cgr(time - reference_time);
	initialized = 0;
	IonBundle = NULL;

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
 *      19/02/20
 *
 * \return int
 *
 * \retval   1   Success case: CGR initialized
 * \retval  -1   Error case: ownNode can't be 0
 * \retval  -2   MWITHDRAW error
 * \retval  -3   Error case: log directory can't be opened
 * \retval  -4   Error case: log file can't be opened
 * \retval  -5   Arguments error
 *
 * \param[in]   ownNode   The node that the CGR will consider as contacts graph's root
 * \param[in]   time      The reference time (time 0 for the CGR)
 * \param[in]   ionwm     The Working Memory partition
 * \param[in]   ionvdb    The ION Virtual Database, from which we get the contact plan
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  19/02/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
int initialize_contact_graph_routing(unsigned long long ownNode, time_t time, PsmPartition ionwm, IonVdb *ionvdb)
{
	int result = 1;

	if(ownNode != 0 && time >= 0 && ionwm != NULL && ionvdb != NULL)
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

				if(update_contact_plan(ionwm, ionvdb) == -2)
				{
					printf("Cannot update contact plan in Unibo-CGR");
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
