/** \file commonDefines.h
 *
 *  \brief  The scope of this file is to keep in one place
 *          the definitions and includes used by many files.
 *
 ** \copyright Copyright (c) 2020, Alma Mater Studiorum, University of Bologna, All rights reserved.
 *
 *  \author Lorenzo Persampieri, lorenzo.persampieri@studio.unibo.it
 *
 *  \par Supervisor
 *       Carlo Caini, carlo.caini@unibo.it
 */


#ifndef LIBRARY_COMMONDEFINES_H_
#define LIBRARY_COMMONDEFINES_H_

#include <sys/time.h>
#include <stdlib.h>

#ifndef CGR_BUILD_FOR_ION
#define CGR_BUILD_FOR_ION 1
#endif

#if (CGR_BUILD_FOR_ION)
#include "ion.h"
#define MWITHDRAW(size) allocFromIonMemory(__FILE__, __LINE__, size)
#define MDEPOSIT(addr) releaseToIonMemory(__FILE__, __LINE__, addr)
#else
#define MWITHDRAW(size) malloc(size)
#define MDEPOSIT(addr) free(addr)
#endif

// just to use with function pointer
extern void MDEPOSIT_wrapper(void *addr);

#ifndef LOG
/**
 * \brief Boolean: Set to 1 if you want to print various log files, otherwise set to 0.
 *
 * \hideinitializer
 */
#define LOG 1
#endif

#ifndef DEBUG_CGR
/**
 * \brief Boolean: Set to 1 if you want some utility functions for debugging, 0 otherwhise
 *
 * \hideinitializer
 */
#define DEBUG_CGR 0
#endif

#ifndef CGR_DEBUG_FLUSH
/**
 * \brief Boolean: Set to 1 if you want to flush all debug print and log when you are
 *        in a debugging session, set to 0 otherwise.
 *
 * \par Notes:
 *          1. Remember that flushes every print is a bad practice, use it only for debug.
 *
 * \hideinitializer
 */
#define CGR_DEBUG_FLUSH 0
#endif

#define	MAX_POSIX_TIME	2147483647

#define CLEAR_FLAGS(flags) ((flags) = 0)

#if (CGR_DEBUG_FLUSH == 1)
/**
 * \brief   Flush the stream (fflush), compiled only with CGR_DEBUG_FLUSH enabled.
 *
 * \hideinitializer
 */
#define debug_fflush(file) fflush(file)
#else
#define debug_fflush(file) do {  } while(0)
#endif

#if (DEBUG_CGR == 1)

/**
 * \brief   Debug utility function: print the file, line and function that called this macro
 *          and then print the string passed as arguments.
 *
 * \details Use exactly like a printf function.
 *
 * \hideinitializer
 */
#define verbose_debug_printf(f_, ...) do { \
	fprintf(stdout, "At line %d of %s, %s(): ", __LINE__, __FILE__, __FUNCTION__); \
	fprintf(stdout, (f_), ##__VA_ARGS__); \
	fputc('\n', stdout); \
	debug_fflush(stdout); \
} while(0)

/**
 * \brief   Debug utility function: print the file, line and function that called this macro
 *          and then print the string passed as arguments. At the end flush the stream.
 *
 * \details Use exactly like a printf function.
 *
 * \hideinitializer
 */
#define flush_verbose_debug_printf(f_, ...) do { \
	fprintf(stdout, "At line %d of %s, %s(): ", __LINE__, __FILE__, __FUNCTION__); \
	fprintf(stdout, (f_), ##__VA_ARGS__); \
	fputc('\n', stdout); \
	fflush(stdout); \
} while(0)

/**
 * \brief   Debug utility function: print the function that called this macro
 *          and then print the string passed as arguments. At the end flush the stream;
 *
 * \details Use exactly like a printf function.
 *
 * \hideinitializer
 */
#define flush_debug_printf(f_, ...) do { \
	fprintf(stdout, "%s(): ", __FUNCTION__); \
	fprintf(stdout, (f_), ##__VA_ARGS__); \
	fputc('\n', stdout); \
	fflush(stdout); \
} while(0)

/**
 * \brief   Debug utility function: print the function that called this macro
 *          and then print the string passed as arguments.
 *
 * \details Use exactly like a printf function.
 *
 * \hideinitializer
 */
#define debug_printf(f_, ...) do { \
	fprintf(stdout, "%s(): ", __FUNCTION__); \
	fprintf(stdout, (f_), ##__VA_ARGS__); \
	fputc('\n', stdout); \
	debug_fflush(stdout); \
} while(0)

#else
	// empty define
#define debug_printf(f_, ...) do { } while(0)
#define verbose_debug_printf(f_, ...) do { } while(0)
#define flush_debug_printf(f_, ...) do {  } while(0)
#define flush_verbose_debug_printf(f_, ...) do {  } while(0)
#endif

#include "log/log.h"

/********* CHECK MACROS ERROR *********/
/**
 * \cond
 */
#if (LOG != 0 && LOG != 1)
fatal error
// Intentional compilation error
// LOG has to be 0 or 1
#endif

#if (DEBUG_CGR != 0 && DEBUG_CGR != 1)
fatal error
// Intentional compilation error
// DEBUG_CGR has to be 0 or 1
#endif

#if (CGR_DEBUG_FLUSH != 0 && CGR_DEBUG_FLUSH != 1)
fatal error
// Intentional compilation error
// CGR_DEBUG_FLUSH has to be 0 or 1
#endif
/**
 * \endcond
 */
/**************************************/

#endif /* LIBRARY_COMMONDEFINES_H_ */
