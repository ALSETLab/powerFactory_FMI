/* Implementation of util.h */

#include "util.h"
#include "result.h"
#include "dsblock.h"
#ifndef ONLY_INCLUDE_INLINE_INTEGRATION
#include "integration.h"
#endif
#include "fmiFunctions_fwd.h"
#include "cexch.h"


#include "adymosim.h"
#ifndef ONLY_INCLUDE_INLINE_INTEGRATION
#include <cvode/cvode.h>
/* Sundials */
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#ifndef FMU_SOURCE_SINGLE_UNIT
DYMOLA_STATIC const char* loggFuncCall = "FunctionCall";
DYMOLA_STATIC const char* loggUndef = "";
DYMOLA_STATIC const char* loggQiErr = "";
DYMOLA_STATIC const char* loggBadCall ="IllegalFunctionCall";
DYMOLA_STATIC const char* loggSundials ="Sundials";
DYMOLA_STATIC const char* loggMemErr = "memoryAllocation";
DYMOLA_STATIC const char* loggStats = "CvodeStatistics";
#endif

/* ----------------- local variables ----------------- */

/* when compiling as a single complilation unit, this is already defined */
#ifndef FMU_SOURCE_SINGLE_UNIT
extern Component* globalComponent2;
#endif

/* ----------------- function definitions ----------------- */

extern void ModelicaMessage(const char *string);
extern void ModelicaError(const char *string);

/* -------------------------------------------------------- */
DYMOLA_STATIC void util_logger(Component* comp, FMIString instanceName, FMIStatus status,
							   FMIString category, FMIString message, ...)
{
	char buf[1024];
	va_list ap;
	int capacity;
	int n = 0;

	if (category == loggFuncCall && !comp->loggFuncCall || comp->loggingOn == FMIFalse && (status < FMIWarning)) {
		return;
	}

	
	capacity = sizeof(buf) - 1;
	if (comp->logbufp > comp->logbuf) {
		/* add buffered messages */
#if defined(_MSC_VER) && _MSC_VER>=1400
		n = _snprintf_s(buf, capacity, _TRUNCATE, "%s", comp->logbuf);
#else
		buf[capacity]=0;
		n = snprintf(buf, capacity, "%s", comp->logbuf);
#endif
		if (n >= capacity || n <= 0) {
			goto done;
		}
	}
	va_start(ap, message);
#if defined(_MSC_VER) && _MSC_VER>=1400
	vsnprintf_s(buf + n, capacity - n, _TRUNCATE, message, ap);
#else
	buf[capacity]=0;
	vsnprintf(buf + n, capacity - n, message, ap);
#endif
	va_end(ap);
#if (FMI_VERSION >= FMI_3)
	comp->loggMessage(comp->ie, status, category, buf);
#elif (FMI_VERSION >= FMI_2)
	comp->functions->logger(comp->functions->componentEnvironment, instanceName, status, category, "%s", buf);
#else
	comp->functions.logger(comp, instanceName, status, category, "%s", buf);
#endif

done:
	comp->logbufp = comp->logbuf;
}

/* -------------------------------------------------------- */
DYMOLA_STATIC void util_buflogger(Component* comp, FMIString instanceName, FMIStatus status,
								  FMIString category, FMIString message, ...)
{
	va_list ap;
	int capacity;
	int n;

	if (comp->loggingOn == FMIFalse && (status < FMIWarning)) {
		return;
	}

	capacity = (int) (sizeof(comp->logbuf) - (comp->logbufp - comp->logbuf) - 1);
	if (capacity <= 0) {
		goto fail;
	}

	va_start(ap, message);
#if defined(_MSC_VER) && _MSC_VER>=1400
	n = vsnprintf_s(comp->logbufp, capacity, _TRUNCATE, message, ap);
#else
	n = vsnprintf(comp->logbufp, capacity, message, ap);
#endif
	if (n >= capacity || n <= 0) {
		goto fail;
	}
	va_end(ap);
	comp->logbufp += n;
	return;

fail:
	comp->logbufp = comp->logbuf;
}

/* -------------------------------------------------------- */
DYMOLA_STATIC FMIString util_strdup(FMIString s)
{
	static const FMIString nullString = "<NULL>";
	static const int maxLen = 1024;
	FMIString pos = s;
	int len;
	char* newS;

	if (s == NULL) {
		s = nullString;
	}
	len = (int) strlen(s);
	if (len > maxLen) {
		len = maxLen;
	}

	
	newS = (FMIChar*) ALLOCATE_MEMORY(len + 1, sizeof(FMIChar));
	if (newS == NULL) {
		return NULL;
	}
	strncpy(newS, s, len);
	return newS;
}
#ifndef ONLY_INCLUDE_INLINE_INTEGRATION
/* ------------------------------------------------------ */
DYMOLA_STATIC int util_check_flag(void *flagvalue, char *funcname, int opt, Component* comp)
{
	int *errflag;

	/* Check if SUNDIALS function returned NULL pointer - no memory allocated */
	if (opt == 0 && flagvalue == NULL) {
		util_logger(comp, comp->instanceName, FMIError, loggSundials,
			"SUNDIALS_ERROR: %s() failed - returned NULL pointer", funcname);
		return 1;
	}

	/* Check if flag < 0 */
	else if (opt == 1) {
		errflag = (int *) flagvalue;
		if (*errflag < 0) {
			char * CVodeFlagName = CVodeGetReturnFlagName(*errflag);
			util_logger(comp, comp->instanceName, FMIError, loggSundials,
				"SUNDIALS_ERROR: %s() failed with flag = %s",
				funcname, CVodeFlagName);
			free(CVodeFlagName);
			return 1;
		}
	}

	return 0;
}
#endif /* ONLY_INCLUDE_INLINE_INTEGRATION */

DYMOLA_STATIC void copyDStatesToDid(struct DYNInstanceData*did_, double* dStates, double* previousVars);
DYMOLA_STATIC void copyDStatesFromDid(struct DYNInstanceData*did_, double* dStates, double* previousVars);


/* ------------------------------------------------------ */
DYMOLA_STATIC int GetDymolaOneIteration(struct DYNInstanceData*);
DYMOLA_STATIC void SetDymolaOneIteration(struct DYNInstanceData*, int val);
DYMOLA_STATIC void SetDymolaJacobianPointers(struct DYNInstanceData*did_, double * QJacobian_,double * QBJacobian_,double * QCJacobian_,double * QDJacobian_,int QJacobianN_,
	int QJacobianNU_,int QJacobianNY_,double * QJacobianSparse_,int * QJacobianSparseR_,int * QJacobianSparseC_,int QJacobianNZ_);
DYMOLA_STATIC int dsblock_did(int *idemand_, int *icall_,
	double *time, double x_[], double xd_[], double u_[],
	double dp_[], int ip_[], Dymola_bool lp_[],
	double f_[], double y_[], double w_[], double qz_[],
	double duser_[], int iuser_[], void*cuser_[], struct DYNInstanceData*,
	int *ierr_);

DYMOLA_STATIC int util_refresh_cache(Component* comp, int idemand, const char* label, FMIBoolean* iterationConverged)
{
#if (FMI_VERSION >= FMI_2)
	if(comp->fmi2ComputeInit){
		FMIStatus ret = FMIOK;
		int saveDymolaOneIteration = GetDymolaOneIteration(comp->did);
		int saveIcall = comp->icall;
		comp->fmi2ComputeInit = FMIFalse;
		if(comp->isCoSim){
			ret = fmiEnterSlaveInitializationMode_(comp, comp->relativeTolerance, comp->tStart, comp->StopTimeDefined, comp->tStop);
		}else if(!comp->isCoSim){
			ret = fmiEnterModelInitializationMode_(comp, FMIFalse, 0);
		}
		if(ret != FMIOK)
			return ret; /*Shold be error or fatal;*/
		comp->icall=saveIcall;
		SetDymolaOneIteration(comp->did,saveDymolaOneIteration);
	}
	switch (comp->mStatus) {
	    case modelInstantiated:
			if (GetDymolaOneIteration(comp->did) == 0) {
				SetDymolaOneIteration(comp->did, 5);
			}
			break;
		case modelInitializationMode:
			if (GetDymolaOneIteration(comp->did) == 0) {
				SetDymolaOneIteration(comp->did, 5);
			}
			break;
		case modelEventMode:
		case modelEventMode2:
		case modelEventModeExit:
			if(comp->firstEventCall){
				SetDymolaOneIteration(comp->did, 2);
				comp->firstEventCall=FMIFalse;
			}else if (GetDymolaOneIteration(comp->did) == 0) {
				SetDymolaOneIteration(comp->did, 5);
			}
			idemand=iDemandEventHandling;
			break;
		default:
			/*assert(GetDymolaOneIteration(comp->did) == 0);*/
			break;
	}
#endif
	comp->QiErr=0;
	copyDStatesToDid(comp->did,comp->dStates,comp->previousVars);
	if (comp->did) {
		globalComponent2=comp;
		dsblock_did(&idemand, &comp->icall, &comp->time, comp->states, 0,
			comp->inputs, comp->parameters, 0, 0, comp->derivatives, comp->outputs, comp->auxiliary,                                
			comp->crossingFunctions, comp->duser, comp->iuser, (void**) comp->sParameters, comp->did, &comp->QiErr);
		globalComponent2=0;
	} else {
		dsblock_(&idemand, &comp->icall, &comp->time, comp->states, 0,             
			comp->inputs, comp->parameters, 0, 0, comp->derivatives,       
			comp->outputs, comp->auxiliary,                                
			comp->crossingFunctions, comp->duser, comp->iuser, (void**) comp->sParameters, &comp->QiErr);
	}
	copyDStatesFromDid(comp->did,comp->dStates,comp->previousVars);
	/*Only check convergence criteria for functions that need it i.e. event handling*/
	if(iterationConverged){
		*iterationConverged = (GetDymolaOneIteration(comp->did)  == 0) ? FMITrue : FMIFalse;
	}
	/*Always reset*/
	SetDymolaOneIteration(comp->did, 0);
	comp->valWasSet = 0;
	if (idemand == 5 && (comp->QiErr == -995 || comp->QiErr == -993)) {
		comp->anyNonEvent = FMITrue;
	} else {
		comp->anyNonEvent = FMIFalse;
	}
	if (comp->QiErr>=-995 && comp->QiErr<=-990) comp->QiErr=0; /* Ignore special cases for now, except efficient minor events */
	if (comp->QiErr == 0) {
		return 0;
	} 
	
	if (label != NULL) {
		util_logger(comp, comp->instanceName, FMIError, loggQiErr,                     
			"%s: dsblock_ failed, QiErr = %d", label, comp->QiErr);
	}
	return comp->QiErr;
}

/* ------------------------------------------------------ */
DYMOLA_STATIC FMIStatus util_error(Component* comp)
{	
	/*Termination should be done exteranly when error is thrown*/
	switch(comp->mStatus) {
	    case modelInstantiated:
			/* internally state "error" and "terminated" are equivalent */
			comp->mStatus = modelTerminated;
			break;
		case modelTerminated:
			break;
		default:
			if (comp->isCoSim == FMITrue) {
#ifndef FMU_SKIP_CO_SIM
				fmiTerminateSlave_(comp);
#endif
			} else {
#if (FMI_VERSION >= FMI_2)
				fmiTerminateModel_(comp);
#elif !defined(FMU_SKIP_MODEL_EXCHANGE)
				fmiTerminate_(comp);
#endif
			}
			break;
	}
	return FMIError;
}

/* ------------------------------------------------------ */
extern void InitializeDymosimStruct(struct BasicDDymosimStruct* basicD, struct BasicIDymosimStruct* basicI);
extern void SetDymolaEventOptional(struct DYNInstanceData*did_, int val);
extern int GetDymolaHaveEventIterated(struct DYNInstanceData*did_);
DYMOLA_STATIC FMIStatus util_initialize_model(FMIComponent c, FMIBoolean toleranceControlled, FMIReal relativeTolerance, FMIBoolean complete)
{
	Component* comp = (Component*) c;
	FMIStatus status = FMIOK;
	int QiErr = 0;
	/* Start of integration */
	int idemand = iDemandStart;
	comp->terminationByModel = FMIFalse;
/* #ifdef _DEBUG fails on dSPACE platforms where _DEBUG is always defined, 0 or 1. */
#if _DEBUG
	/* catch numerical exceptions */
	/*_EM_INVALID_ is not compatible with the underlying handling of NaN */
	_controlfp(0, _EM_ZERODIVIDE | _EM_OVERFLOW);
#endif
#if (FMI_VERSION < FMI_2)
	if (comp->mStatus != modelInstantiated) {
		util_logger(comp, comp->instanceName, FMIError, loggBadCall, "model cannot be initialized in current state(%d)", comp->mStatus);
		return util_error(comp);
	}
#endif
	/* Ignore toleranceControlled for external integration according to recommendation from Hans Olsson:
	The tolerance for the equations to be solved at events are based on the crossing function
	tolerance (to avoid chattering), which is normally stricter than relativeTolerance.
	Hence, relativeTolerance is normally not significant. */
	if (toleranceControlled == FMITrue) {
		util_logger(comp, comp->instanceName, FMIWarning, loggBadCall, "fmiInitialize: tolerance control not supported");
	}
	InitializeDymosimStruct(comp->dstruct, comp->istruct);
#ifdef OVERIDE_DYMOLA_EVENT_OPTIONAL
	SetDymolaEventOptional(comp->did,  1);
#else
  #ifdef ONLY_INCLUDE_INLINE_INTEGRATION
	if(comp->isCoSim){
		SetDymolaEventOptional(comp->did,  1);
	}else{
		SetDymolaEventOptional(comp->did,  0);
	}
  #else
	if(comp->isCoSim && comp->istruct->mInlineIntegration){
			SetDymolaEventOptional(comp->did,  1);
			comp->istruct->mFindEvent = 1;
	}else{
			SetDymolaEventOptional(comp->did,  0);
	}
  #endif
#endif
	comp->icall = iDemandStart - 1;
	if (complete ==FMIFalse) {
		SetDymolaOneIteration(comp->did, 2);
	} else {
		SetDymolaOneIteration(comp->did, 0);
	}
	QiErr = util_refresh_cache(comp, idemand, NULL, NULL);
	if (QiErr != 0) {
		status = FMIError;
		util_logger(comp, comp->instanceName, status, loggQiErr,
			"fmiInitialize: dsblock_ failed, QiErr = %d", QiErr);
		util_logger(comp, comp->instanceName, status, loggUndef,
			"Unless otherwise indicated by error messages, possible errors are (non-exhaustive):\n"
			"The model references external data that is not present on the target machine, at least not with the same location.\n");
		return util_error(comp);
	} 

	if (comp->storeResult == FMITrue) {
		if(result_setup(comp)){
			status = FMIFatal;
			util_logger(comp, comp->instanceName, status, loggUndef,
			"fmiInitialize: Error when initializing result file, out of memory\n");
			return status;
		}
	}

#ifndef FMU_SOURCE_CODE_EXPORT
	if (GetAdditionalFlags(21)) {
		comp->basetype = (FMIInteger*) ALLOCATE_MEMORY(comp->nStates, sizeof(FMIInteger));
		if (comp->basetype)
			result_get_basetype((int*) comp->basetype);
	}
#endif

	return status;
}

/* ------------------------------------------------------ */
DYMOLA_STATIC FMIStatus util_event_update(FMIComponent c, FMIBoolean intermediateResults,
#if (FMI_VERSION >= FMI_2)
/* needs another argument since not in eventInfo for FMI 2*/
FMIBoolean* terminateSolution
#else
fmiEventInfo* eventInfo
#endif
)
{
	Component* comp = (Component*) c;
	FMIStatus status = FMIOK;
	int QiErr = 0;
	FMIBoolean converged = 0;

	/* make sure idemand up to auxilliary variables are computed prior to starting event iteration */
	/* (necessary for e.g. co-simulating Modelica.Electrical.Machines.Examples.SynchronousInductionMachines.SMEE_Generator) */
	{
		static IDemandLevel idemand[] = {iDemandVariable, iDemandEventHandling};
		int i;
		for (i = 0; i < 2; i++) {
			/* TODO: figure out why intermediate results are not retrieved if dsblock_ is first called with idemand == iDemandVariable */
			if (i == 0) {
				if (intermediateResults == FMITrue || comp->istruct->mInlineIntegration) {
					continue;
				}
			} else {
				/* configure actual event iteration */
				if (intermediateResults == FMITrue)
				{
					if (comp->eventIterationOnGoing) {
						SetDymolaOneIteration(comp->did, 3);
					} else {
						SetDymolaOneIteration(comp->did, 2);
						comp->eventIterationOnGoing = 1;
					}
				} else {
					if (comp->eventIterationOnGoing) {
						SetDymolaOneIteration(comp->did, 4);
					} else {
						SetDymolaOneIteration(comp->did, 0);
					}
				}
				comp->icall = 0;
			}
			QiErr = util_refresh_cache(comp, idemand[i], NULL, &converged);
#if (FMI_VERSION >= FMI_2)
			*terminateSolution = FMIFalse;
#else
			eventInfo->terminateSimulation = FMIFalse;
#endif
			if (comp->steadyStateReached) {
				util_logger(comp, comp->instanceName, FMIOK, loggUndef,
					"event updating: simulation terminated after a steady state was reached");
#if (FMI_VERSION >= FMI_2)
				*terminateSolution = FMITrue;
#else
				eventInfo->terminateSimulation = FMITrue;
#endif
				return FMIOK;
			}			
#ifndef FMU_SOURCE_CODE_EXPORT
			if (comp->maxRunTimeReached) {
				const double max_run_time = GetAdditionalReals(4);
				util_logger(comp, comp->instanceName, FMIError, loggUndef,
					"event updating: Stopping simulation after reaching the embedded\nmax run time (%g seconds) for this simulator.", max_run_time);
#if (FMI_VERSION >= FMI_2)
				*terminateSolution = FMITrue;
#else
				eventInfo->terminateSimulation = FMITrue;
#endif
				return util_error(comp);
			}
#endif
			if (QiErr>=-995 && QiErr<=-990) QiErr=0; /* Ignore special cases for now */
			if (QiErr != 0) {
				if (QiErr == -999) {
					util_logger(comp, comp->instanceName, FMIOK, loggUndef,
						"event updating: simulation terminated by model");
#if (FMI_VERSION >= FMI_2)
					*terminateSolution = FMITrue;
#else
					eventInfo->terminateSimulation = FMITrue;
#endif
					return FMIOK;
				} else {
					util_logger(comp, comp->instanceName, FMIError, loggQiErr,
						"event updating: dsblock_ failed, QiErr = %d", QiErr);
					return util_error(comp);
				}
			}
		}

#if (FMI_VERSION >= FMI_2)
		/* for FMI 2 we only use this function internally for co-simulation and then always expect converged */
		assert(intermediateResults == FMIFalse && converged == FMITrue);
		comp->eventIterRequired = !converged;
#else
		if (converged == FMIFalse)
		{
			/* more iterations needed */
			assert(intermediateResults == FMITrue);
			eventInfo->iterationConverged = FMIFalse;
		} else {
			eventInfo->iterationConverged = FMITrue;
			comp->eventIterationOnGoing = 0;
		}
#endif
	}

	/* for FMI 2 we only use this internally for co-simulation and do not use these values */
#if (FMI_VERSION < FMI_2)
	/* always fmiFalse since we hide states that might be exchanged */
	eventInfo->stateValueReferencesChanged = FMIFalse;
	/* TODO: introduce a flag in dymosim that can be checked for this purpose (for now it is faster to
	to fetch states each time than to conclude the truth value from copy + compare) */
	eventInfo->stateValuesChanged = (comp->nStates > 0) ? FMITrue : FMIFalse;
	eventInfo->upcomingTimeEvent = (comp->dstruct->mNextTimeEvent < (1.0E37 - 1)) ? FMITrue : FMIFalse;
	if (eventInfo->upcomingTimeEvent == FMITrue) {
		eventInfo->nextEventTime = comp->dstruct->mNextTimeEvent;
	}
#endif
	comp->recalJacobian = 1;
	return status;
}

/* ------------------------------------------------------ */
DYMOLA_STATIC FMIStatus util_initialize_slave(FMIComponent c,
	                                          FMIReal      relativeTolerance,
											  FMIReal      tStart,
											  FMIBoolean   StopTimeDefined,
											  FMIReal      tStop)
{
	FMIStatus status;
	FMIReal relTol = relativeTolerance;
#if (FMI_VERSION < FMI_2)
	FMIEventInfo eventInfo;
#endif
	Component* comp = (Component*) c;

#if (FMI_VERSION >= FMI_2)
	status = util_initialize_model(c, FMIFalse, 0, FMIFalse);
#else
	status = fmiInitialize_(c, FMIFalse, 0, &eventInfo);
#endif /* FMI_2 */
	if (status != FMIOK) {
		return status;
	}
	comp->StopTimeDefined = StopTimeDefined;
	comp->tStop = tStop;
	comp->valWasSet = 0;
#ifndef ONLY_INCLUDE_INLINE_INTEGRATION
	if(!comp->istruct->mInlineIntegration){
		switch(integration_setup(c, relTol)){
		case integrationOK:
			break;
		case integrationError:
			return util_error(comp);
		case integrationFatal:
		default:
			return FMIFatal;
		}
	}
#endif
	return FMIOK;
}

#if (FMI_VERSION >= FMI_2)
/* ------------------------------------------------------ */
DYMOLA_STATIC FMIStatus util_exit_model_initialization_mode(FMIComponent c, const char* label, ModelStatus nextStatus)
{
	Component* comp = (Component*) c;
	int QiErr;
	
	if (comp->mStatus != modelInitializationMode) {
		util_logger(comp, comp->instanceName, FMIError, loggBadCall, "%s: may only called in initialization mode", label);
		return util_error(comp);
	}

	SetDymolaOneIteration(comp->did, 4);
	QiErr = util_refresh_cache(comp, iDemandStart, NULL, NULL);
	if (QiErr != 0) {
		return util_error(comp);
	}
	/* reset */
	SetDymolaOneIteration(comp->did, 0);
	if (comp->storeResult == FMITrue) {
		result_sample(comp, FMITrue); 
	}
	comp->mStatus = nextStatus;
	return FMIOK;
}
#endif /* FMI_2 */

/* ------------------------------------------------------ */
extern struct DymolaTimes* GetDymolaTimers(struct DYNInstanceData*, int*);

DYMOLA_STATIC void util_print_dymola_timers(FMIComponent c){
	Component* comp = (Component*) c;
	int DymolaTimerStructsLen_=0;
	struct DymolaTimes* DymolaTimerStructs_=0;

	DymolaTimerStructs_=GetDymolaTimers(comp->did, &DymolaTimerStructsLen_);

	if (DymolaTimerStructsLen_>0 && DymolaTimerStructs_) {
	   double overhead=0,overheadSum=0;
	   int i=0;
	   int nrFound=0;
	   for(i=0;i<DymolaTimerStructsLen_;++i){
		   if (DymolaTimerStructs_[i].numAccum>0) {
			   if (nrFound==0 || DymolaTimerStructs_[i].minimAccum<overhead){
				   overhead=DymolaTimerStructs_[i].minimAccum;
			   }
			   nrFound+=DymolaTimerStructs_[i].numAccum;
		   }
	   }
	   if (nrFound>0) {
		  size_t len, maxlen = 14;
		  for (i = 0; i < DymolaTimerStructsLen_; ++i) {
			  if (DymolaTimerStructs_[i].numAccum > 0) {
				  len = strlen(DymolaTimerStructs_[i].name);
				  if (len > maxlen) maxlen = len;
			  }
		  }
		  if (maxlen > 100) maxlen = 100;

		  util_logger(comp, comp->instanceName, FMIOK, loggUndef,"\nProfiling information for the blocks.\n"
			   "Estimated overhead per call %11.2f[us] total %12.3f[s]\n"
			   "the estimated overhead has been subtracted below.\n"
			   "Name of block%*s, Block, Total CPU[s], Mean[us]    ( Min[us]    to Max[us]    ),   Called\n",
			  overhead*1e6,overhead*nrFound, maxlen - 13, "");
	
		   for(i=0;i<DymolaTimerStructsLen_;++i) if (DymolaTimerStructs_[i].numAccum>0) {
			   double overheadTotal=overhead*DymolaTimerStructs_[i].numAccum;
			   util_logger(comp, "", FMIOK, loggUndef,"%-*.*s: %5d, %12.3f, %11.2f (%11.2f to %11.2f), %8d\n", maxlen, maxlen,
				   DymolaTimerStructs_[i].name,
				   i,DymolaTimerStructs_[i].totalAccum-overheadTotal,
				   ((DymolaTimerStructs_[i].totalAccum-overheadTotal)/DymolaTimerStructs_[i].numAccum)*1e6,
				   (DymolaTimerStructs_[i].minimAccum-overhead)*1e6,(DymolaTimerStructs_[i].maximAccum-overhead)*1e6,
				   DymolaTimerStructs_[i].numAccum);
		   }
	   }
	}
	return;
}

DYMOLA_STATIC void  unSuportedFunctionCall(FMIComponent c, FMIString funcName){
	Component * comp = (Component*) c;
	util_logger(comp, comp->instanceName, FMIError, loggBadCall,
		"%s: This function is currently not supported",funcName);
}

#if (FMI_VERSION >= FMI_3)
extern size_t arraySizes[];
#endif
DYMOLA_STATIC FMIStatus checkSizes(FMIComponent c, const FMIValueReference * vr, size_t nVr, size_t nv, FMIString funcName, FMIString getSet){
	if(nVr != nv){
#if (FMI_VERSION >= FMI_3)
		size_t nbrElements = 0, i;
		for(i = 0; i< nVr; ++i){
			unsigned int aiCat = FMI_CATEGORY(vr[i]);
			if(aiCat == dsInput2){
				nbrElements+=arraySizes[FMI_INDEX(vr[i])];
			}else{
				nbrElements++;
			}
		}
		if(nbrElements != nv){
#endif
			Component *comp = (Component*) c;
			util_logger(comp, comp->instanceName, FMIError, loggBadCall,
			"%s%s: sizes of nValuereferences (%d) and nValues (%d) does not match", getSet, funcName, nVr, nv);
			return FMIError;
#if (FMI_VERSION >= FMI_3)
		}
#endif
	}
	return FMIOK;
}


/* ------------------------------------------------------ */
typedef struct {
	void* objAddr;
	int count;
} UtilExternalObjRefCount;
#define MAX_EXTERNAL_OBJECTS_REF_COUNTERS 128

