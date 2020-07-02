/** \file list_type.h
 *
 * \brief This file provides the definition of a List element and of
 *        a ListElt element.
 *
 ** \copyright Copyright (c) 2020, Alma Mater Studiorum, University of Bologna, All rights reserved.
 *
 *  \author Lorenzo Persampieri, lorenzo.persampieri@studio.unibo.it
 *
 *  \par Supervisor
 *       Carlo Caini, carlo.caini@unibo.it
 */

#ifndef SOURCES_LIBRARY_LIST_LIST_TYPE_H_
#define SOURCES_LIBRARY_LIST_LIST_TYPE_H_

typedef struct nodeElt ListElt;
typedef struct list *List;

typedef int (*compare_function)(void*, void*);
typedef void (*delete_function)(void*);

struct nodeElt
{
	/**
	 * \brief The list to which this ListElt belongs
	 */
	List list;
	/**
	 * \brief The previous element in the list
	 */
	ListElt *prev;
	/**
	 * \brief The next element in the list
	 */
	ListElt *next;
	void *data;
};

struct list
{
	/**
	 * \brief The data to which this list belongs (back-reference)
	 */
	void *userData;
	/**
	 * \brief The first element in the list
	 */
	ListElt *first;
	/**
	 * \brief The last element in the list
	 */
	ListElt *last;
	/**
	 * \brief Number of elements in the list
	 */
	long unsigned int length;
	/**
	 * \brief Compare the data field of the ListElt in the list
	 */
	compare_function compare;
	/**
	 * \brief The function called to delete the data field of the ListElts in the list
	 */
	delete_function delete_data_elt;
	/**
	 * \brief The function called to delete the userData of the list
	 */
	delete_function delete_userData;
};

#endif /* SOURCES_LIBRARY_LIST_LIST_TYPE_H_ */
