/** \file log.c
 * 
 *  \brief  This file provides the implementation of a library
 *          to print various log files. Only for Unix-like systems.
 *
 ** \copyright Copyright (c) 2020, Alma Mater Studiorum, University of Bologna, All rights reserved.
 *
 *  \author Lorenzo Persampieri, lorenzo.persampieri@studio.unibo.it
 *
 *  \par Supervisor
 *       Carlo Caini, carlo.caini@unibo.it
 */

#include "../../contact_plan/contacts/contacts.h"
#include "../../contact_plan/nodes/nodes.h"
#include "../../contact_plan/ranges/ranges.h"
#include "log.h"
#include "../list/list.h"
#include <dirent.h>

#if (LOG == 1)

/**
 * \brief The main log file.
 */
static FILE *file_log = NULL;
/**
 * \brief File where we print the contacts graph.
 */
static FILE *file_contacts = NULL;
/**
 * \brief File where we print the ranges graph.
 */
static FILE *file_ranges = NULL;
/**
 * \brief Path of the logs directory.
 */
static char log_dir[256];
/**
 * \brief Boolean: '1' if we already checked the existence of the
 * log directory, '0' otherwise.
 */
static char log_dir_exist = '0';
/**
 * \brief Length of the log_dir string. (strlen)
 */
static long unsigned int len_log_dir = 0;
/**
 * \brief The time used by the log files.
 */
static time_t currentTime = -1;
/**
 * \brief The last time when the logs have been printed.
 */
static time_t lastFlushTime = 0;
/**
 * \brief The buffer used to print the logs in the main log file.
 */
static char buffer[256]; //don't touch the size of the buffer

/******************************************************************************
 *
 * \par Function Name:
 *      writeLog
 *
 * \brief Write a log line on the main log file
 *
 *
 * \par Date Written:
 *      24/01/20
 *
 * \return void
 *
 * \param[in]  *format   Use like a printf function
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  24/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
void writeLog(const char *format, ...)
{
	va_list args;

	if (file_log != NULL)
	{
		va_start(args, format);

		fprintf(file_log, "%s", buffer); //[            time]:
		vfprintf(file_log, format, args);
		fputc('\n', file_log);

		debug_fflush(stdout);

		va_end(args);
	}
	return;
}

/******************************************************************************
 *
 * \par Function Name:
 *      writeLogFlush
 *
 * \brief Write a log line on the main log file and flush the output stream
 *
 *
 * \par Date Written:
 *      03/04/20
 *
 * \return void
 *
 * \param[in]  *format   Use like a printf function
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  03/04/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
void writeLogFlush(const char *format, ...)
{
	va_list args;

	if (file_log != NULL)
	{
		va_start(args, format);

		fprintf(file_log, "%s", buffer); //[            time]:
		vfprintf(file_log, format, args);
		fputc('\n', file_log);
		fflush(file_log);

		va_end(args);
	}
	return;
}


/******************************************************************************
 *
 * \par Function Name:
 *      log_fflush
 *
 * \brief Flush the stream
 *
 *
 * \par Date Written:
 *      11/04/20
 *
 * \return void
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  11/04/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
void log_fflush()
{
	if(file_log != NULL)
	{
		fflush(file_log);
		lastFlushTime = currentTime;
	}
}

/******************************************************************************
 *
 * \par Function Name:
 *      setLogTime
 *
 * \brief  Set the time that will be printed in the next log lines.
 *
 *
 * \par Date Written:
 *      24/01/20
 *
 * \return void
 *
 * \param[in]  time   	The time that we want to print in the next log lines
 *
 * \par Notes:
 *            1.   Set the first 19 characters of the buffer
 *                 used to print the log (translated: "[    ...xxxxx...]: "
 *                 where ...xxxxx... is the time)
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  24/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
void setLogTime(time_t time)
{
	if (time != currentTime && time >= 0)
	{
		currentTime = time;
		sprintf(buffer, "[%15ld]: ", (long int) currentTime);
		//set the first 19 characters of the buffer
		if(currentTime - lastFlushTime > 5) // After 5 seconds.
		{
			log_fflush();
		}
	}
}

/******************************************************************************
 *
 * \par Function Name:
 *      print_string
 *
 * \brief  Print a string to the indicated file
 *
 *
 * \par Date Written:
 *      24/01/20
 *
 * \return int
 *
 * \retval   0   Success case
 * \retval  -1   Arguments error
 * \retval  -2   Write error
 *
 * \param[in]  file       The file where we want to print the string
 * \param[in]  *toPrint   The string that we want to print
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  24/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
int print_string(FILE *file, char *toPrint)
{
	int result = -1;
	if (file != NULL && toPrint != NULL)
	{
		result = fprintf(file, "%s", toPrint);

		result = (result >= 0) ? 0 : -2;

		debug_fflush(stdout);
	}

	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 *      createLogDir
 *
 * \brief  Create the main directory where the log files will be putted in
 *
 *
 * \par Date Written:
 *      24/01/20
 *
 * \return int
 *
 * \retval   0   We already called this function and the directory exists
 * \retval   1   Directory created correctly
 * \retval  -1   Error case: the directory cannot be created
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  24/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
int createLogDir()
{
	/*
	 * Currently the directory is created under the current directory
	 * with the name: cgr_log
	 *
	 * Relative path: ./cgr_log
	 */

	int result = 0;
//long unsigned int len;
//char *homedir;
	if (log_dir_exist == '0')
	{
		/*
		 homedir = getenv("HOME");
		 if (homedir != NULL)
		 {
		 strcpy(log_dir, homedir);
		 len = strlen(log_dir);
		 if (len == 0 || (len > 0 && log_dir[len - 1] != '/'))
		 {
		 log_dir[len] = '/';
		 log_dir[len + 1] = '\0';
		 }
		 strcat(log_dir, "cgr_log/");
		 }
		 */
		strcpy(log_dir, "./cgr_log/");
		if (mkdir(log_dir, 0777) != 0 && errno != EEXIST)
		{
			perror("Error CGR log dir cannot be created");
			result = -1;
			len_log_dir = 0;
		}
		else
		{
			result = 1;
			log_dir_exist = '1';
			len_log_dir = strlen("./cgr_log/");
		}
	}

	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 *      openBundleFile
 *
 * \brief  Open the file where the cgr will print the characteristics of the
 *         bundle and all the decisions taken for the forwarding of that bundle
 *
 *
 * \par Date Written:
 *      24/01/20
 *
 * \return FILE*
 * 
 * \retval  FILE*   The file opened
 * \retval  NULL    Error case
 *
 * \param[in]  num  The * in the file's name (see note 2.)
 *
 *       \par Notes:
 *                    1. The file will be opened in write only mode.
 *                    2. Currently the name of the file follows the pattern: call_#*
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  24/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
FILE* openBundleFile(unsigned int num)
{
	FILE *file = NULL;

	if (len_log_dir > 0)
	{
		sprintf(log_dir + len_log_dir, "call_#%u", num);
		file = fopen(log_dir, "w");

		log_dir[len_log_dir] = '\0';
	}

	return file;
}

/******************************************************************************
 *
 * \par Function Name:
 *      closeBundleFile
 *
 * \brief  Close the "call file"
 *
 *
 * \par Date Written:
 *      24/01/20
 *
 * \return null
 *
 * \param[in,out]  **file_call  The FILE to close, at the end file_call will points to NULL
 *
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  24/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
void closeBundleFile(FILE **file_call)
{
	if (file_call != NULL && *file_call != NULL)
	{
		fflush(*file_call);
		fclose(*file_call);
		*file_call = NULL;
	}
}

/******************************************************************************
 *
 * \par Function Name:
 *      cleanLogDir
 *
 * \brief Clean the previously create directory where the log will be printed.
 *
 *
 * \par Date Written:
 *      24/01/20
 *
 * \return int
 *
 * \retval   1   Success case
 * \retval   0   If we never called 'createLogDir' or the directory
 *               wasn't created due to an error.
 * \retval  -1   The directory cannot be opened
 *
 * \par Notes:
 * 			1. The only file that will not be deleted is "log.txt"
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  24/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
int cleanLogDir()
{
	int result = 0;
	DIR *dir;
	struct dirent *file;

	if (log_dir_exist == '1')
	{
		result = 1;

		len_log_dir = strlen(log_dir);
		if ((dir = opendir(log_dir)) != NULL)
		{
			while ((file = readdir(dir)) != NULL)
			{
				if ((strcmp(file->d_name, ".") != 0) && (strcmp(file->d_name, "..") != 0)
						&& (strcmp(file->d_name, "log.txt") != 0) && file->d_type == DT_REG)
				{
					strcpy(log_dir + len_log_dir, file->d_name);
					remove(log_dir);
					log_dir[len_log_dir] = '\0';
				}
			}

			closedir(dir);
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
 *      openLogFile
 *
 * \brief Open the main log file
 *
 *
 * \par Date Written:
 *      24/01/20
 *
 * \return int
 * 
 * \retval   1   Success case, main log file opened.
 * \retval   0   If we never called 'createLogDir' or the directory
 *               wasn't created due to an error or the main log file already exists
 * \retval  -1   The file cannot be opened for some reason
 *
 * \par Notes:
 * 			1. The file will be created in write only mode.
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  24/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
int openLogFile()
{
	int result = 0;
	long unsigned int len;

	if (file_log != NULL)
	{
		result = 1;
	}
	else if (log_dir_exist == '1')
	{
		len = strlen(log_dir);
		strcat(log_dir, "log.txt");

		file_log = fopen(log_dir, "w");

		if (file_log == NULL)
		{
			perror("Error file ./cgr_log/log.txt cannot be opened");
			result = -1;
		}
		else
		{
			result = 1;
		}

		log_dir[len] = '\0';
	}

	return result;
}

/******************************************************************************
 *
 * \par Function Name:
 *      closeLogFile
 *
 * \brief  Close the main log file
 *
 *
 * \par Date Written:
 *      24/01/20
 *
 * \return  void
 *
 * \par Notes:
 * 			1. fd_log will be setted to -1.
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  24/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
void closeLogFile()
{
	if (file_log != NULL)
	{
		fflush(file_log);
		fclose(file_log);
		file_log = NULL;
	}
}

/******************************************************************************
 *
 * \par Function Name:
 *      printCurrentState
 *
 * \brief  Print all the informations to keep trace of the current state of the graphs
 *
 *
 * \par Date Written:
 *      30/01/20
 *
 * \return void
 *
 * \par Notes:
 * 			1.	The contacts graph is printed in append mode in the file "contacts.txt"
 * 			2.	The ranges graph is printed in append mode in the file "ranges.txt"
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  30/01/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
void printCurrentState()
{
	long unsigned int len = 0;
	if (log_dir_exist == '1')
	{
		len = strlen(log_dir);
		log_dir[len] = '\0';
		strcat(log_dir, "contacts.txt");
		if ((file_contacts = fopen(log_dir, "a")) < 0)
		{
			perror("Error contacts graph's file cannot be opened");
			return;
		}
		log_dir[len] = '\0';
		strcat(log_dir, "ranges.txt");
		if ((file_ranges = fopen(log_dir, "a")) < 0)
		{
			perror("Error contacts graph's file cannot be opened");
			fclose(file_contacts);
			return;
		}
		log_dir[len] = '\0';

		printContactsGraph(file_contacts, currentTime);
		printRangesGraph(file_ranges, currentTime);

		fflush(file_contacts);
		fclose(file_contacts);
		fflush(file_ranges);
		fclose(file_ranges);
		file_contacts = NULL;
		file_ranges = NULL;
	}
}

/******************************************************************************
 *
 * \par Function Name:
 *      print_ull_list
 *
 * \brief  Print a list of unsigned long long element
 *
 *
 * \par Date Written:
 *      29/03/20
 *
 * \return int
 *
 * \retval   0   Success case
 * \retval  -1   Arguments error
 * \retval  -2   Write error
 *
 * \param[in]  file        The file where we want to print the list
 * \param[in]  list        The list to print
 * \param[in]  *brief      A description printed before the list
 * \param[in]  *separator  The separator used between the elements
 *
 * \warning brief must be a well-formed string
 * \warning separator must be a well-formed string
 * \warning All the elements of the list (ListElt->data) have to be != NULL
 *
 * \par Revision History:
 *
 *  DD/MM/YY |  AUTHOR         |   DESCRIPTION
 *  -------- | --------------- | -----------------------------------------------
 *  29/03/20 | L. Persampieri  |  Initial Implementation and documentation.
 *****************************************************************************/
int print_ull_list(FILE *file, List list, char *brief, char *separator)
{
	int len, result = -1, temp;
	ListElt *elt;
	if (file != NULL && list != NULL && brief != NULL && separator != NULL)
	{
		result = 0;
		len = fprintf(file, "%s", brief);
		if (len < 0)
		{
			result = -2;
		}
		for (elt = list->first; elt != NULL && result == 0; elt = elt->next)
		{
			temp = fprintf(file, "%llu%s", *((unsigned long long*) elt->data),
					(elt == list->last) ? "" : separator);
			if (temp >= 0)
			{
				len += temp;
			}
			else
			{
				result = -2;
			}
			if (len > 85)
			{
				temp = fputc('\n', file);

				if (temp < 0)
				{
					result = -2;
				}

				len = 0;
			}
		}

		temp = fputc('\n', file);
		if (temp < 0)
		{
			result = -2;
		}

	}

	return result;
}

#endif
