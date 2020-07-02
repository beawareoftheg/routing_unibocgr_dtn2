/** \file list.h
 *
 *  \brief  This file provides the declarations of the list's functions implemented in list.c
 *
 ** \copyright Copyright (c) 2020, Alma Mater Studiorum, University of Bologna, All rights reserved.
 *
 *  \author Lorenzo Persampieri, lorenzo.persampieri@studio.unibo.it
 *
 *  \par Supervisor
 *       Carlo Caini, carlo.caini@unibo.it
 */

#ifndef CGR_LIST_H
#define CGR_LIST_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "list_type.h"
#include "../commonDefines.h"

extern List list_create(void*, delete_function, compare_function, delete_function);
extern void sort_list(List);

extern unsigned long int list_get_length(List);
extern void* list_get_userData(List);
extern void* listElt_get_data(ListElt*);
extern List listElt_get_list(ListElt*);

extern int move__a_elt__before__b_elt(ListElt *a_elt, ListElt *b_elt);
extern int move_elt_to_other_list(ListElt *elt, List other);

extern ListElt* list_get_first_elt(List);
extern ListElt* list_get_last_elt(List);
extern ListElt* list_get_next_elt(ListElt*);
extern ListElt* list_get_prev_elt(ListElt*);
extern List list_get_equals_elements(List, void*);
extern ListElt* list_search_elt_by_data(List, void*);

extern ListElt* list_insert_before(ListElt*, void*);
extern ListElt* list_insert_after(ListElt*, void*);
extern ListElt* list_insert_first(List, void*);
extern ListElt* list_insert_last(List, void*);

extern void list_remove_first(List);
extern void list_remove_last(List);
extern void list_remove_elt(ListElt*);
extern void list_remove_elt_by_data(List, void*);
extern void remove_secondList_from_firstList(List firstList, List secondList);

extern void free_list(List);
extern void free_list_elts(List);

#ifdef __cplusplus
}
#endif

#endif

