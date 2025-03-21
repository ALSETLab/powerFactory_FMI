#ifndef LIBFMI_LIB
/* This is a wrapper file including all source files. Use this when you need to compile everything into a single
   compilation unit, where all internal functions and variables are static. This is necessary when combining
   the source code with others FMUs exported by CATIA DBM or Dymola.*/

/* set to be able to compile everything in a single compilation unit, which enables static functions
and symbols, which in turn enables creation of static libraries without name clashes */
#ifndef FMU_SOURCE_SINGLE_UNIT
#define FMU_SOURCE_SINGLE_UNIT
#endif
/* must set explicitly for FMI code */
#if !defined(DYMOLA_STATIC)
#define DYMOLA_STATIC static
#endif
#if !defined(DYMOLA_STATIC_IMPORT)
#define DYMOLA_STATIC_IMPORT extern
#endif
/* for Sundials code */
#if !defined(SUNDIALS_EXPORT)
#define SUNDIALS_EXPORT 
#endif
#if !defined(RT)
#define RT
#endif

#if defined(RTTWINCAT)
#define NO_LOCALE
#define NO_EXTERNAL_DLL
#endif

/*dsblock1.c has dependecies on definitions from this header*/
#include "fmiModelIdentifier.h"

/* dsmodel */
#include "dsmodel.c"
#include "extendedIncludes.h"

/* libfmi */
#if (FMI_VERSION >= FMI_2)
#include "fmi2Functions.c"
#else
#include "fmiFunctions.c"
#endif
#include "fmiCommonFunctions_int.c"
#include "fmiMEFunctions_int.c"
#include "util.c"
#include "fmiCoSimFunctions_int.c"
#if (FMI_VERSION >= FMI_2) && !(defined(NO_FILE) || defined(RTTWINCAT))
#include "tpl.c"
#include "mmap.c"
#endif
#ifndef ONLY_INCLUDE_INLINE_INTEGRATION
#include "integration.c"
#include "jac.c"

/* CVODE */
#include "cvode.c"
#include "cvode_dense.c"
#include "cvode_direct.c"
#include "cvode_io.c"
#include "nvector_serial.c"
#include "sundials_dense.c"
#include "sundials_direct.c"
#include "sundials_math.c"
#include "sundials_nvector.c"
#ifdef FMU_SOURCE_CODE_EXPORT_SPARSE
#include "cvode_sparse.c"
#include "cvode_superlumt.c"
#include "sundials_sparse.c"
#endif /* FMU_SOURCE_CODE_EXPORT_SPARSE */

#endif /*ONLY_INCLUDE_INLINE_INTEGRATION*/
#endif
