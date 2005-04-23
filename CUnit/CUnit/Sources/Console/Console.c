/*
 *  CUnit - A Unit testing framework library for C.
 *  Copyright (C) 2001  Anil Kumar
 *  Copyright (C) 2004  Anil Kumar, Jerry St.Clair
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 *  Contains the Console Test Interface  implementation.
 *
 *  Created By     : Anil Kumar on ...(in month of Aug 2001)
 *  Last Modified  : 19/Aug/2001
 *  Comment        : Added initial console interface functions without
 *                   any run functionality.
 *  Email          : aksaharan@yahoo.com
 *
 *  Modified       : 24/Aug/2001 by Anil Kumar
 *  Comment        : Added compare_strings, show_errors, list_suites,
 *                   list_tests function declarations.
 *  Email          : aksaharan@yahoo.com
 *
 *  Modified       : 17-Jul-2004 (JDS)
 *  Comment        : New interface, doxygen comments, reformat console output
 *  Email          : jds2@users.sourceforge.net
 */

/** @file
 * Console test interface with interactive output (implementation).
 */
/** @addtogroup Console
 @{
*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>

#include "CUnit.h"
#include "TestDB.h"
#include "Util.h"
#include "TestRun.h"
#include "Console.h"

/** Console interface status flag. */
typedef enum
{
  CONTINUE = 1,   /**< Continue processing commands in current menu. */
  MOVE_UP,        /**< Move up to the previous menu. */
  STOP            /**< Stop processing (user selected 'Quit'). */
} STATUS;

/** currently running suite. */
static CU_pSuite f_pRunningSuite = NULL;

/* Forward declaration of module functions */
static void console_registry_level_run(CU_pTestRegistry pRegistry);
static STATUS console_suite_level_run(CU_pSuite pSuite);

static CU_ErrorCode console_run_all_tests(CU_pTestRegistry pRegistry);
static CU_ErrorCode console_run_suite(CU_pSuite pSuite);
static CU_ErrorCode console_run_single_test(CU_pSuite pSuite, CU_pTest pTest);

static void console_test_start_message_handler(const CU_pTest pTest, const CU_pSuite pSuite);
static void console_test_complete_message_handler(const CU_pTest pTest, const CU_pSuite pSuite, const CU_pFailureRecord pFailure);
static void console_all_tests_complete_message_handler(const CU_pFailureRecord pFailure);
static void console_suite_init_failure_message_handler(const CU_pSuite pSuite);

static CU_ErrorCode select_test(CU_pSuite pSuite, CU_pTest* pTest);
static CU_ErrorCode select_suite(CU_pTestRegistry pRegistry, CU_pSuite* pSuite);

static void list_suites(CU_pTestRegistry pRegistry);
static void list_tests(CU_pSuite pSuite);
static void show_failures(void);

/*------------------------------------------------------------------------*/
/** Run registered CUnit tests using the console interface. */
void CU_console_run_tests(void)
{
  CU_pTestRegistry pRegistry = CU_get_registry();

  /*
   *   To avoid user from cribbing about the output not coming onto
   *   screen at the moment of SIGSEGV.
   */
  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);

  fprintf(stdout, "\n\n     CUnit : A Unit testing framework for C."
                  "\n     http://cunit.sourceforge.net/\n\n");

  if (NULL == pRegistry) {
    fprintf(stderr, "\n\nFATAL ERROR - Test registry is not initialized.\n");
    CU_set_error(CUE_NOREGISTRY);
  }
  else {
    CU_set_test_start_handler(console_test_start_message_handler);
    CU_set_test_complete_handler(console_test_complete_message_handler);
    CU_set_all_test_complete_handler(console_all_tests_complete_message_handler);
    CU_set_suite_init_failure_handler(console_suite_init_failure_message_handler);

    console_registry_level_run(pRegistry);
  }
}

/*------------------------------------------------------------------------*/
/** Main loop for console interface.
 * Displays actions and responds based on user imput.
 * @param pRegistry The CU_pTestRegistry to use for testing (non-NULL).
 */
static void console_registry_level_run(CU_pTestRegistry pRegistry)
{
  int chChoice;
  char szTemp[256];
  CU_pSuite pSuite = NULL;
  STATUS eStatus = CONTINUE;

  assert(pRegistry);

  while (CONTINUE == eStatus)
  {
    fprintf(stdout, "\n*************** CUNIT CONSOLE - MAIN MENU ***********************"
                    "\n(R)un all, (S)elect suite, (L)ist suites, Show (F)ailures, (Q)uit"
                    "\nEnter Command : ");
    chChoice = getchar();
    fgets(szTemp, sizeof(szTemp), stdin);

    switch (tolower(chChoice)) {
      case 'r':
        console_run_all_tests(pRegistry);
        break;

      case 's':
        if (CUE_SUCCESS == select_suite(pRegistry, &pSuite)) {
          if (STOP == console_suite_level_run(pSuite))
            eStatus = STOP;
        }
        else {
          fprintf(stdout, "\nSuite not found.\n");
        }
        break;

      case 'l':
        list_suites(pRegistry);
        break;

      case 'f':
        show_failures();
        break;

      case 'q':
        eStatus = STOP;
        break;

      /* To stop gcc from cribbing */
      default:
        break;
    }
  }
}

/*------------------------------------------------------------------------*/
/** Run a selected suite within the console interface.
 * Displays actions and responds based on user imput.
 * @param pSuite The suite to use for testing (non-NULL).
 */
static STATUS console_suite_level_run(CU_pSuite pSuite)
{
  int chChoice;
  char szTemp[256];
  CU_pTest pTest = NULL;
  STATUS eStatus = CONTINUE;

  assert(pSuite);

  while (CONTINUE == eStatus) {

    fprintf(stdout, "\n*************** CUNIT CONSOLE - SUITE MENU *******************************"
                    "\n(R)un All, (S)elect test, (L)ist tests, Show (F)ailures, (M)ove up, (Q)uit"
                    "\nEnter Command : ");
    chChoice = getchar();
    fgets(szTemp, sizeof(szTemp), stdin);

    switch (tolower(chChoice)) {
      case 'r':
        console_run_suite(pSuite);
        break;

      case 's':
        if (CUE_SUCCESS == select_test(pSuite, &pTest)) {
          console_run_single_test(pSuite, pTest);
        }
        break;

      case 'l':
        list_tests(pSuite);
        break;

      case 'f':
        show_failures();
        break;

      case 'm':
        eStatus = MOVE_UP;
        break;

      case 'q':
        eStatus = STOP;
        break;

      /* To stop gcc from cribbing */
      default:
        break;
    }
  }
  return eStatus;
}

/*------------------------------------------------------------------------*/
/** Run all tests within the console interface.
 * The test registry is changed to the specified registry
 * before running the tests, and reset to the original
 * registry when done.
 * @param pRegistry The CU_pTestRegistry containing the tests
 *                  to be run (non-NULL).
 * @return An error code indicating the error status
 *         during the test run.
 */
static CU_ErrorCode console_run_all_tests(CU_pTestRegistry pRegistry)
{
  CU_pTestRegistry pOldRegistry = NULL;
  CU_ErrorCode result;

  assert(pRegistry);

  f_pRunningSuite = NULL;

  pOldRegistry = CU_set_registry(pRegistry);
  result = CU_run_all_tests();
  CU_set_registry(pOldRegistry);
  return result;
}

/*------------------------------------------------------------------------*/
/** Run a specified suite within the console interface.
 * @param pSuite The suite to be run (non-NULL).
 * @return An error code indicating the error status
 *         during the test run.
 */
static CU_ErrorCode console_run_suite(CU_pSuite pSuite)
{
  f_pRunningSuite = NULL;
  return CU_run_suite(pSuite);
}

/*------------------------------------------------------------------------*/
/** Run a specific test for the specified suite within 
 * the console interface.
 * @param pSuite The suite containing the test to be run (non-NULL).
 * @param pTest  The test to be run (non-NULL).
 * @return An error code indicating the error status
 *         during the test run.
 */
static CU_ErrorCode console_run_single_test(CU_pSuite pSuite, CU_pTest pTest)
{
  f_pRunningSuite = NULL;
  return CU_run_test(pSuite, pTest);
}

/*------------------------------------------------------------------------*/
/** Read the name of a test from standard input and
 * locate the test having the specified name.
 * A pointer to the located test is stored in pTest
 * upon return.
 * @param pSuite The suite to be queried.
 * @param ppTest Pointer to location to store the selected test.
 * @return CUE_SUCCESS if a test was successfully selected,
 *         CUE_NOTEST otherwise.  On return, ppTest points
 *         to the test selected.
 */
static CU_ErrorCode select_test(CU_pSuite pSuite, CU_pTest* ppTest)
{
  char szTestName[MAX_TEST_NAME_LENGTH];

  fprintf(stdout,"\nEnter Test Name : ");
  fgets(szTestName, MAX_TEST_NAME_LENGTH, stdin);
  sscanf(szTestName, "%[^\n]s", szTestName);

  *ppTest = CU_get_test_by_name(szTestName, pSuite);

  return (*ppTest) ? CUE_SUCCESS : CUE_NOTEST;
}

/*------------------------------------------------------------------------*/
/** Read the name of a suite from standard input and
 * locate the suite having the specified name.
 * The input string is used to locate the suite having the
 * indicated name, which is returned in pSuite.  ppSuite will
 * be NULL if there is no suite registered in pRegistry having
 * the input name.  Returns NULL if the suite is successfully
 * located, non-NULL otherwise.
 * @param pRegistry The CU_pTestRegistry to query (non-NULL).
 * @param ppSuite Pointer to location to store the selected suite.
 * @return CUE_SUCCESS if a suite was successfully selected,
 *         CUE_NOSUITE otherwise.  On return, ppSuite points
 *         to the suite selected.
 */
static CU_ErrorCode select_suite(CU_pTestRegistry pRegistry, CU_pSuite* ppSuite)
{
  char szSuiteName[MAX_SUITE_NAME_LENGTH];

  assert(pRegistry);

  fprintf(stdout,"\n\nEnter Suite Name : ");
  fgets(szSuiteName, MAX_SUITE_NAME_LENGTH, stdin);
  sscanf(szSuiteName, "%[^\n]s", szSuiteName);

  *ppSuite = CU_get_suite_by_name(szSuiteName, pRegistry);

  return (*ppSuite) ? CUE_SUCCESS : CUE_NOSUITE;
}

/*------------------------------------------------------------------------*/
/** List the suites in a registry to standard output.
 * @param pRegistry The CU_pTestRegistry to query (non-NULL).
 */
static void list_suites(CU_pTestRegistry pRegistry)
{
  CU_pSuite pCurSuite = NULL;
  int i;

  assert(pRegistry);
  if (0 == pRegistry->uiNumberOfSuites) {
    fprintf(stdout, "\nNo suites registered.\n");
    return;
  }

  assert(pRegistry->pSuite);

  fprintf(stdout, "\n--------------------- Registered Suites --------------------------");
  fprintf(stdout, "\n     Suite Name                          Init?  Cleanup?  # Tests\n");

  for (i = 1, pCurSuite = pRegistry->pSuite; pCurSuite; pCurSuite = pCurSuite->pNext, ++i) {
    fprintf(stdout, "\n%3d. %-34.33s   %3s     %3s       %3d",
            i,
            pCurSuite->pName,
            pCurSuite->pInitializeFunc ? "YES" : "NO",
            pCurSuite->pCleanupFunc ? "YES" : "NO",
            pCurSuite->uiNumberOfTests);
  }
  fprintf(stdout, "\n------------------------------------------------------------------"
                  "\nTotal Number of Suites : %-d\n", pRegistry->uiNumberOfSuites);
}

/*------------------------------------------------------------------------*/
/** List the tests in a suite to standard output.
 * @param pSuite The suite to query (non-NULL).
 */
static void list_tests(CU_pSuite pSuite)
{
  CU_pTest pCurTest = NULL; 
  unsigned int uiCount;

  assert(pSuite);
  if (0 == pSuite->uiNumberOfTests) {
    fprintf(stdout, "\nSuite %s contains no tests.\n", pSuite->pName);
    return;
  }

  assert(pSuite->pTest);

  fprintf(stdout, "\n--------------- Test List ---------------------------------");
  fprintf(stdout, "\n      Test Names (Suite: %s)\n", pSuite->pName);

  for (uiCount = 1, pCurTest = pSuite->pTest; pCurTest; uiCount++, pCurTest = pCurTest->pNext) {
    fprintf(stdout, "\n%3d.  %s", uiCount, pCurTest->pName);
  }
  fprintf(stdout, "\n-----------------------------------------------------------"
                  "\nTotal Number of Tests : %-d\n", pSuite->uiNumberOfTests);
}

/*------------------------------------------------------------------------*/
/** Display the record of test failures on standard output. */
static void show_failures(void)
{
  int i;
  CU_pFailureRecord pFailure = CU_get_failure_list();

  if (NULL == pFailure) {
    fprintf(stdout, "\nNo failures.\n");
  }
  else {

    fprintf(stdout, "\n--------------- Test Run Failures -------------------------");
    fprintf(stdout, "\n   src_file:line# : (suite:test) : failure_condition\n");

    for (i = 1 ; pFailure ; pFailure = pFailure->pNext, i++) {
      fprintf(stdout, "\n%d. %s:%d : (%s : %s) : %s", i,
          pFailure->strFileName ? pFailure->strFileName : "",
          pFailure->uiLineNumber,
          pFailure->pSuite ? pFailure->pSuite->pName : "",
          pFailure->pTest ? pFailure->pTest->pName : "",
          pFailure->strCondition ? pFailure->strCondition : "");
    }
    fprintf(stdout, "\n-----------------------------------------------------------"
                    "\nTotal Number of Failures : %-d\n", i - 1);
  }
}

/*------------------------------------------------------------------------*/
/** Handler function called at start of each test.
 * @param pTest  The test being run.
 * @param pSuite The suite containing the test.
 */
static void console_test_start_message_handler(const CU_pTest pTest, const CU_pSuite pSuite)
{
  assert(pTest);
  assert(pSuite);
  
  /* Comparing the Addresses rather than the Group Names. */
  if (!f_pRunningSuite || f_pRunningSuite != pSuite) {
    fprintf(stdout, "\nRunning Suite : %s", pSuite->pName);
    fprintf(stdout, "\n\tRunning test : %s", pTest->pName);
    f_pRunningSuite = pSuite;
  }
  else {
    fprintf(stdout, "\n\tRunning test : %s", pTest->pName);
  }
}

/*------------------------------------------------------------------------*/
/** Handler function called at completion of each test.
 * @param pTest   The test being run.
 * @param pSuite  The suite containing the test.
 * @param pFailure Pointer to the 1st failure record for this test.
 */
static void console_test_complete_message_handler(const CU_pTest pTest, const CU_pSuite pSuite, const CU_pFailureRecord pFailure)
{
  /*
   *   For console interface do nothing. This is useful only for the test
   *   interface where UI is involved.  Just silence compiler warnings.
   */
  CU_UNREFERENCED_PARAMETER(pTest);
  CU_UNREFERENCED_PARAMETER(pSuite);
  CU_UNREFERENCED_PARAMETER(pFailure);
}

/*------------------------------------------------------------------------*/
/** Handler function called at completion of all tests in a suite.
 * @param pFailure Pointer to the test failure record list.
 */
static void console_all_tests_complete_message_handler(const CU_pFailureRecord pFailure)
{
  CU_pRunSummary pRunSummary = CU_get_run_summary();
  CU_pTestRegistry pRegistry = CU_get_registry();

  CU_UNREFERENCED_PARAMETER(pFailure); /* not used in console interface */

  assert(pRunSummary);
  assert(pRegistry);

  fprintf(stdout,"\n\n--Run Summary: Type      Total     Ran  Passed  Failed"
                   "\n               suites %8u%8u     n/a%8u"
                   "\n               tests  %8u%8u%8u%8u"
                   "\n               asserts%8u%8u%8u%8u\n",
          pRegistry->uiNumberOfSuites,
          pRunSummary->nSuitesRun,
          pRunSummary->nSuitesFailed,
          pRegistry->uiNumberOfTests,
          pRunSummary->nTestsRun,
          pRunSummary->nTestsRun - pRunSummary->nTestsFailed,
          pRunSummary->nTestsFailed,
          pRunSummary->nAsserts,
          pRunSummary->nAsserts,
          pRunSummary->nAsserts - pRunSummary->nAssertsFailed,
          pRunSummary->nAssertsFailed);
}
 
/*------------------------------------------------------------------------*/
/** Handler function called when suite initialization fails.
 * @param pSuite The suite for which initialization failed.
 */
static void console_suite_init_failure_message_handler(const CU_pSuite pSuite)
{
  assert(pSuite);

  fprintf(stdout, "\nWARNING - Suite initialization failed for %s.",
          pSuite->pName);
}

/** @} */
