/** \file commonFunctions.h
 *
 *  \brief  The scope of this file is to keep in one place
 *          the functions used by many (or all) files.
 *
 ** \copyright Copyright (c) 2020, Alma Mater Studiorum, University of Bologna, All rights reserved.
 *
 *  \author Lorenzo Persampieri, lorenzo.persampieri@studio.unibo.it
 *
 *  \par Supervisor
 *       Carlo Caini, carlo.caini@unibo.it
 */


#include "commonDefines.h"

/******************************************************************************
 *
 * \par Function Name:
 * 		MDEPOSIT_wrapper
 *
 * \brief Call the MDEPOSIT macro, so deallocate memory pointed by addr.
 *
 * \details Use this function when you need to assign MDEPOSIT to a function pointer.
 *        For any other reason you should use MDEPOSIT.
 *
 *
 * \par Date Written:
 * 		13/05/20
 *
 * \return void
 *
 * \par Revision History:
 *
 *  DD/MM/YY | AUTHOR          |  DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  13/05/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
void MDEPOSIT_wrapper(void *addr)
{
    if(addr != NULL)
    {
        MDEPOSIT(addr);
    }
}
