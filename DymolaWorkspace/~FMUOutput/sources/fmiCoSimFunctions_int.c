/* The generic implementation of the FMI co-simulation interface. */

#ifndef FMU_SKIP_CO_SIM

/* need to include first so that correct files are included */
#include "conf.h"
#include "util.h"
#ifndef ONLY_INCLUDE_INLINE_INTEGRATION
#include "integration.h"
#endif
#include "result.h"
#if !defined(FMU_SOURCE_CODE_EXPORT) || defined(FMU_SOURCE_CODE_EXPORT_SPARSE)
#include "cexch.h"
#endif

#include "fmiFunctions_fwd.h"

#include "adymosim.h"

#include <math.h>
#include <assert.h>
#ifndef DYMOLA_STATIC_IMPORT
#define DYMOLA_STATIC_IMPORT DYMOLA_STATIC
#endif

/* ----------------- macros ----------------- */

#if (FMI_VERSION >= FMI_2)
#define CS_RESULT_SAMPLE(atEvent)                                         \
if (comp->storeResult == FMITrue) {                                       \
	if(result_sample(comp, atEvent)){                                     \
		util_logger(comp, comp->instanceName, FMIFatal, loggUndef,        \
		"%s: Error when sampling result file, out of memory\n", label);   \
		return FMIFatal;                                                  \
	}                                                                     \
	if (noSetFMUStatePriorToCurrentPoint == FMITrue) {                    \
		if(result_flush(comp)){                                           \
			util_logger(comp, comp->instanceName, FMIFatal, loggUndef,    \
			"%s: Error when sampling result file, out of memory\n", label);\
			return FMIFatal;                                              \
		}                                                                 \
	}                                                                     \
}
#else
#define CS_RESULT_SAMPLE(atEvent)                                         \
if (comp->storeResult == FMITrue) {                                       \
		if(result_sample(comp, atEvent)){                                 \
		util_logger(comp, comp->instanceName, FMIFatal, loggUndef,        \
		"%s: Error when sampling result file, out of memory\n", label);   \
		return FMIFatal;                                                  \
	}                                                                     \
}
#endif

/* ----------------- local function declarations ----------------- */

static FMIStatus handleEvent(FMIComponent c, FMIBoolean forceEvent);


/* ----------------- local variables ----------------- */

/* when compiling as a single complilation unit, this is already defined */
#ifndef FMU_SOURCE_SINGLE_UNIT
extern Component* globalComponent2;
#endif

/* ----------------- function definitions ----------------- */

/* For 2.0 this is replaced by a function common for both ME and Co-sim. */
#if (FMI_VERSION < FMI_2)
DYMOLA_STATIC const char* fmiGetTypesPlatform_()
{
	return fmiPlatform;
}
#endif
DYMOLA_STATIC int fmiUser_Initialize();
DYMOLA_STATIC int fmiUser_Terminate();
DYMOLA_STATIC int fmuUser_Reset();
DYMOLA_STATIC void DYNPropagateDidToThread(struct DYNInstanceData* did);
/* ---------------------------------------------------------------------- */
#if (FMI_VERSION >= FMI_3)
DYMOLA_STATIC FMIComponent fmi3InstantiateCoSimulation_(fmi3String instanceName,
														fmi3String instantiationToken,
														fmi3String resourcePath,
														fmi3Boolean visible,
														fmi3Boolean loggingOn,
														fmi3Boolean eventModeUsed,
														fmi3Boolean earlyReturnAllowed,
														const fmi3ValueReference requiredIntermediateVariables[],
														size_t nRequiredIntermediateVariables,
														fmi3InstanceEnvironment instanceEnvironment,
														fmi3LogMessageCallback logMessage,
														fmi3IntermediateUpdateCallback intermediateUpdate){

	static FMIString label = "fmi3InstantiateCoSimulation";
	Component* comp = (Component*) fmi3InstantiateModelExchange_(instanceName, instantiationToken, resourcePath, visible, loggingOn, instanceEnvironment, logMessage);
	/* ignore argument "visible" since we never provide any GUI */
	if (comp == NULL) {
		return NULL;
	}
	comp->isCoSim=FMITrue;
	comp->eventModeUsed = eventModeUsed;
	comp->earlyReturnAllowed = earlyReturnAllowed;
#if 0
	if( eventModeUsed){
		util_logger(comp, comp->instanceName, fmi3Warning, loggBadCall, "%s: eventModeUsed, currently not supported", label);
	}
	if(earlyReturnAllowed){
		util_logger(comp, comp->instanceName, fmi3Warning, loggBadCall, "%s: earlyReturnAllowed, currently not supported", label);
	}
#endif
	if(nRequiredIntermediateVariables){
		size_t i, nrInput = 0, nrOutput=0;
		for (i = 0; i < nRequiredIntermediateVariables; ++i) {
			switch (FMI_CATEGORY(requiredIntermediateVariables[i])) {
			case dsOutput:
				/* Ignore - but count for the future*/
				nrOutput++; 
				break;
			case dsInput:
				nrInput++;
				break;
			default:
				util_logger(comp, comp->instanceName, fmi3Warning, loggBadCall, "%s: requiredIntermediateVariables[%d] = neither input nor input not supported", label, i);
			}
		}
		comp->intermediateUpdate = intermediateUpdate;
		comp->nRequiredIntermediateInputs = nrInput;
		comp->nRequiredIntermediateOutputs = nrOutput;
	}
#ifndef ONLY_INCLUDE_INLINE_INTEGRATION
	if(integration_allocate_ipData(comp) != FMIOK){
		free(comp);
		return NULL;
	}
#endif

	util_logger(comp, comp->instanceName, FMIOK, loggFuncCall, "%s", label);
	return comp;

}

#elif (FMI_VERSION >= FMI_2)
DYMOLA_STATIC FMIComponent fmiInstantiateSlave_(FMIString instanceName,
												FMIString fmuGUID,
												FMIString fmuResourceLocation,
												const FMICallbackFunctions* functions,
												FMIBoolean visible,
												FMIBoolean loggingOn)
{
	static FMIString label = "fmi2Instantiate";
	Component* comp = (Component*) fmiInstantiateModel_(instanceName, fmuGUID, fmuResourceLocation, functions, visible, loggingOn);
	/* ignore argument "visible" since we never provide any GUI */
	if (comp == NULL) {
		return NULL;
	}

	if (functions->stepFinished != NULL) {
		util_logger(comp, comp->instanceName, FMIWarning, loggBadCall,
			"%s: Callback function stepFinished != NULL but asynchronous fmiDoStep is not supported",label);
	}
#ifndef ONLY_INCLUDE_INLINE_INTEGRATION
	if(integration_allocate_ipData(comp) != FMIOK){
		free(comp);
		return NULL;
	}
#endif
	util_logger(comp, comp->instanceName, FMIOK, loggFuncCall, "%s", label);
	return comp;
}
#else

FMIComponent fmiInstantiateSlave_(FMIString  instanceName,
                                  FMIString  fmuGUID,
                                  FMIString  fmuLocation,
                                  FMIString  mimeType,
                                  FMIReal    timeout,
                                  FMIBoolean visible,
                                  FMIBoolean interactive,
                                  fmiCoSimCallbackFunctions functions,
                                  FMIBoolean loggingOn)
{
	static FMIString label = "fmiInstantiateSlave";
	static FMIString ignore = ", argument will be ignored";
	Component* comp;
	static fmiMECallbackFunctions meFunctions;
	meFunctions.logger = functions.logger;


	/* Since we don't support tool coupling, the following arguments are ignored:
	fmuLocation, mimeType */

	comp = (Component*) fmiInstantiateModel_(instanceName, fmuGUID, meFunctions, loggingOn);
	if (comp == NULL) {
		return NULL;
	}	
	if (functions.stepFinished != NULL) {
		util_logger(comp, comp->instanceName, FMIWarning, loggBadCall,
			"%s: Callback function stepFinished != NULL but asynchronous fmiDoStep is not supported%s",label,ignore);
	}

	if (timeout > 0) {
		util_logger(comp, comp->instanceName, FMIWarning, loggBadCall, "%s: timeout != 0 is not supported%s",label,ignore);
	}
	/* ignore argument "visible" since we never provide any GUI */ 
	if (interactive) {
		util_logger(comp, comp->instanceName, FMIWarning, loggBadCall, "%s: interactive not supported%s",label,ignore);
	}
#ifndef ONLY_INCLUDE_INLINE_INTEGRATION
	if(integration_allocate_ipData(comp) != FMIOK){
		free(comp);
		return NULL;
	}
#endif
	util_logger(comp, comp->instanceName, FMIOK, loggFuncCall, "%s", label);
	return comp;

}
#endif /* FMI_2 */

/* ---------------------------------------------------------------------- */
DYMOLA_STATIC void fmiFreeSlaveInstance_(FMIComponent c)
{
	integration_free_ipData((Component*)c);
    fmiFreeModelInstance_(c);
}
extern const double cvodeTolerance;
#if (FMI_VERSION >= FMI_2)
/* ---------------------------------------------------------------------- */
DYMOLA_STATIC FMIStatus fmiEnterSlaveInitializationMode_(FMIComponent c,
	                                                     FMIReal      relativeTolerance,
														 FMIReal      tStart,
														 FMIBoolean   StopTimeDefined,
														 FMIReal      tStop)
{
	static FMIString label = "fmi2EnterInitializationMode";
	Component* comp = (Component*) c;
	FMIStatus status;
	if(!comp->relativeToleranceDefined){
		relativeTolerance = cvodeTolerance;
	}
	status = util_initialize_slave(c, relativeTolerance, tStart, StopTimeDefined, tStop);
	if (status != FMIOK) {
		return status;
	}
	if(comp->istruct->mInlineIntegration && tStart> 0.0 && comp->dstruct->mDymolaFixedStepSize > 0.0){
		double h = comp->dstruct->mDymolaFixedStepSize;
		double quota = tStart/h;
		comp->inlineStepCounter =  (unsigned long long) quota;
	}else{
		comp->inlineStepCounter = 0;
	}
	util_logger(comp, comp->instanceName, FMIOK, loggFuncCall, "%s", label);
	return status;
}

/* ---------------------------------------------------------------------- */
FMIStatus fmiExitSlaveInitializationMode_(FMIComponent c)
{
	static FMIString label = "fmiExitInitializationMode";
	Component* comp = (Component*) c;
	FMIStatus status;

	util_logger(comp, comp->instanceName, FMIOK, loggFuncCall, "%s...", label);
	status = util_exit_model_initialization_mode(c, label, modelContinousTimeMode);
	if (status != FMIOK) {
		return status;
	}

#if (FMI_VERSION >= FMI_3)
	if (comp->eventModeUsed)
		comp->mStatus = modelEventMode;
	else
		comp->mStatus = modelContinousTimeMode;
#else
	comp->mStatus = modelContinousTimeMode;
#endif
	util_logger(comp, comp->instanceName, FMIOK, loggFuncCall, "%s completed", label);
	return FMIOK;
}

#else /* FMI_2 */
/* ---------------------------------------------------------------------- */
DYMOLA_STATIC FMIStatus fmiInitializeSlave_(FMIComponent c,
											FMIReal      tStart,
											FMIBoolean   StopTimeDefined,
											FMIReal      tStop)
{
	static FMIString label = "fmiInitializeSlave";
	Component* comp = (Component*) c;
	FMIStatus status;
	comp->isCoSim=FMITrue;
	if (fmiUser_Initialize())
		return util_error(comp);
	util_logger(comp, comp->instanceName, FMIOK, loggFuncCall, "%s...", label);
#ifndef ONLY_INCLUDE_INLINE_INTEGRATION
	if(!comp->istruct->mInlineIntegration){
		util_logger(comp, comp->instanceName, FMIOK, loggUndef, "%s cvode tolerance is %g", label, cvodeTolerance);
	}
#endif
	status = util_initialize_slave(c, cvodeTolerance, tStart, StopTimeDefined, tStop);
	if (status != FMIOK) {
		return status;
	}
	comp->mStatus = modelContinousTimeMode;
	util_logger(comp, comp->instanceName, FMIOK, loggFuncCall, "%s completed", label);
	return status;
}
#endif /* FMI_2 */

/* ---------------------------------------------------------------------- */
DYMOLA_STATIC void copyDStatesToDid(struct DYNInstanceData*did_, double* dStates, double* previousVars);
DYMOLA_STATIC void copyDStatesFromDid(struct DYNInstanceData*did_, double* dStates, double* previousVars);
DYMOLA_STATIC int dsblock_did(int *idemand_, int *icall_,
	double *time, double x_[], double xd_[], double u_[],
	double dp_[], int ip_[], Dymola_bool lp_[],
	double f_[], double y_[], double w_[], double qz_[],
	double duser_[], int iuser_[], void*cuser_[], struct DYNInstanceData*,
	int *ierr_);

DYMOLA_STATIC FMIStatus fmiTerminateSlave_(FMIComponent c)
{	
#if (FMI_VERSION >= FMI_2)
	static FMIString label = "fmi2Terminate";
#else
	static FMIString label = "fmiTerminateSlave";
#endif
	Component* comp = (Component*) c;
	FMIStatus status =FMIOK;
	FMIStatus status2 = FMIOK;
	util_logger(comp, comp->instanceName, FMIOK, loggFuncCall, "%s...", label);
	if (comp->mStatus == modelTerminated) {
		util_logger(comp, comp->instanceName, FMIWarning, loggBadCall, "%s: already terminated, ignoring call", label);
		return FMIWarning;
	}
	if(fmiUser_Terminate())
		return util_error(comp);
	if(comp->QiErr == 0 && comp->terminationByModel == FMIFalse){
		/*Special case for terminal, call dsblock_ directly instead of
		using util_refresh_cache to avoid messy logic*/
		int terminal = iDemandTerminal;
		copyDStatesToDid(comp->did,comp->dStates,comp->previousVars);
		if (comp->did) {
			globalComponent2=comp;
			dsblock_did(&terminal, &comp->icall, &comp->time, comp->states, 0,
			comp->inputs, comp->parameters,
			0, 0, comp->derivatives, comp->outputs, comp->auxiliary,                                
			comp->crossingFunctions, comp->duser, comp->iuser,
			(void**) comp->sParameters, comp->did, &comp->QiErr);
			globalComponent2=0;
		} else {
			dsblock_(&terminal, &comp->icall, &comp->time, comp->states, 0,             
			comp->inputs, comp->parameters, 0, 0, comp->derivatives,       
			comp->outputs, comp->auxiliary,                                
			comp->crossingFunctions, comp->duser, comp->iuser,
			(void**) comp->sParameters, &comp->QiErr);
		}
		copyDStatesFromDid(comp->did,comp->dStates,comp->previousVars);
		if (comp->QiErr>=-995 && comp->QiErr<=-990) comp->QiErr=0; /* Ignore special cases for now */
		if(!(comp->QiErr == 0 || comp->QiErr==-999)){
			status = FMIError;
			util_logger(comp, comp->instanceName, FMIError, loggQiErr,
				"%s: calling terminal section of dsblock_ failed, QiErr = %d",
				label,comp->QiErr);
		}
	}
#ifndef ONLY_INCLUDE_INLINE_INTEGRATION
	if(!comp->istruct->mInlineIntegration){
		if(integration_teardown(c) == integrationFatal)
			return FMIFatal;
	}
#endif
	util_print_dymola_timers(c);
#ifndef FMU_SOURCE_CODE_EXPORT
	if (comp->storeResult == FMITrue) {
		if(result_teardown(comp)){
			status = FMIFatal;
			util_logger(comp, comp->instanceName, status, loggUndef,
				"%s: Error when terminating result file, out of memory\n", label);
			return status;
		}
		
	}
	if (comp->basetype) {
		FREE_MEMORY(comp->basetype);
	}
#endif /* FMU_SOURCE_CODE_EXPORT */

	if (GetAdditionalFlags(9) && GetAdditionalFlags(10) && !comp->steadyStateReached) {
		if (GetAdditionalFlags(22)) {
			status = FMIWarning;
			util_logger(comp, comp->instanceName, status, loggUndef,
				"%s: Steady-state detection is not supported for models with dynamic state selection.\nSteady-state detection was disabled.\n", label);
		} else {
			status = FMIError;
			util_logger(comp, comp->instanceName, status, loggUndef,
				"%s: The stop time was reached before a steady state was detected\n", label);
		}
	}

	comp->mStatus = modelTerminated;
	util_logger(comp, comp->instanceName, FMIOK, loggFuncCall, "%s completed", label);
	return status;
}

extern void declareNew2_(double*, double*, double*, double*, double*, double*, double*, void**,int*, int, struct DeclarePhase*);
DYMOLA_STATIC size_t DYNGetMaxAuxStrLen();
DYMOLA_STATIC FMIStatus fmiResetSlave_(FMIComponent c)
{
#if (FMI_VERSION >= FMI_2)
	static FMIString label = "fmi2Reset";
#else
	static FMIString label = "fmiResetSlave";
#endif
	Component* comp = (Component*) c;
	FMIStatus status = FMIOK;
	int QiErr = 0;
	const size_t maxStringSize = DYNGetMaxAuxStrLen();
	size_t i;
	if(fmuUser_Reset())
		return util_error(comp);
	util_logger(comp, comp->instanceName, FMIOK, loggFuncCall, "%s...", label);

	switch (comp->mStatus) {
		case modelContinousTimeMode:
		case modelInitializationMode:
#if (FMI_VERSION >= FMI_2)
		case modelEventMode:
		case modelEventMode2:   
		case modelEventModeExit:
#endif
			if(comp->isCoSim){
				status = fmiTerminateSlave_(c);
#if (FMI_VERSION >= FMI_2)
			}else{

	            status = fmiTerminateModel_(c);
#endif
			}
			if (status != FMIOK && status != FMIWarning) {
				return status;
			}
			/* fall-through */
		case modelTerminated:
			/* reset states and parameters */
			if(comp->tsParameters == NULL && comp->nSPar>0){
			  comp->tsParameters = (FMIChar**) ALLOCATE_MEMORY(comp->nSPar, sizeof(FMIChar*));
			  if (comp->tsParameters == NULL) {
				  util_logger(comp, comp->instanceName, FMIError, loggMemErr, "%s failed: out of Memory", label);
				  fmiFreeSlaveInstance_(comp);
				  return FMIError;
			  }
			}
			/*Clear aux and outputs to handle cases when dsmodel assumes default 0 start value*/
			for (i = 0; i < comp->nAux; ++i) {
				comp->auxiliary[i] = 0;
			}
			for (i = 0; i < comp->nOut; ++i) {
				comp->outputs[i] = 0;
			}

			declareNew2_(comp->states, comp->derivatives, comp->parameters, comp->inputs, comp->outputs, comp->auxiliary, comp->statesNominal, (void**) comp->tsParameters, &QiErr, 0, 0);
			for(i = 0; i < comp->nStates; ++i){
				if(comp->statesNominal[i] == 0.0)
					comp->statesNominal[i] = 1.0;
			}
			for(i=0; i < comp->nSPar; ++i){
				size_t len;
				len=strlen(comp->tsParameters[i]);
				if (len>maxStringSize) len=maxStringSize;
				memcpy((comp->sParameters)[i], comp->tsParameters[i], len+1);
				(comp->sParameters)[i][maxStringSize]='\0';
			}

			if (QiErr != 0) {
				util_logger(comp, comp->instanceName, FMIError, loggQiErr,
					"%s: declare_ failed, QiErr = %d", label, QiErr);
				return util_error(comp);
			}
			comp->mStatus = modelInstantiated;
			comp->recalJacobian = 1;
			break;
		case modelInstantiated:
			util_logger(comp, comp->instanceName, FMIOK, loggBadCall, "%s: already reset, ignoring call", label);
			return FMIOK;
	}

	util_logger(comp, comp->instanceName, status, loggFuncCall, "%s completed", label);
	return status;
}

/* ---------------------------------------------------------------------- */
DYMOLA_STATIC FMIStatus fmiSetRealInputDerivatives_(FMIComponent c,
													const  FMIValueReference vr[],
													size_t nvr,
													const  FMIInteger order[],
													const  FMIReal value[])
{
#if (FMI_VERSION >= FMI_2)
	static FMIString label = "fmi2SetRealInputDerivatives";
#else
	static FMIString label = "fmiSetRealInputDerivatives";
#endif
	Component* comp = (Component*) c;
#ifdef ONLY_INCLUDE_INLINE_INTEGRATION
	util_logger(comp, comp->instanceName, FMIError, loggBadCall, "%s is not suported with inline integration",label);
	return util_error(comp);
#else
	size_t nu = comp->nIn; 
	size_t i;
	if(comp->istruct->mInlineIntegration){
		util_logger(comp, comp->instanceName, FMIError, loggBadCall, "%s is not supported with inline integration",label);
		return util_error(comp);
    }
	if(	!comp->ipData){
		util_logger(comp, comp->instanceName, FMIFatal, loggMemErr,
			"%s: integration data not properly allocated.",  label);
		return FMIFatal;
	}

	if (nvr == 0 || nvr > nu) {
		util_logger(comp, comp->instanceName, FMIError, loggBadCall,
			"%s: invalid nvr = %d (number of inputs = %d), ignoring call", label, nvr, nu);
		return util_error(comp);
	}

	for (i = 0; i < nvr; i++) {
		const FMIValueReference r = vr[i];
		int ix = FMI_INDEX(r);

		if (order[i] == 1) {
			switch (FMI_CATEGORY(r)) {
			case dsInput:
				comp->ipData->inputDerivatives[ix] = value[i];
				break;
			default:
				util_logger(comp, comp->instanceName, FMIError, loggBadCall, "%s: variable is not input", label);
				return util_error(comp);
			}
		} else {
			util_logger(comp, comp->instanceName, FMIError, loggBadCall,
						"%s: derivative order %d is not supported", label, order[i]);
			return util_error(comp);
		}
	}
	memcpy(comp->ipData->inputsT0, comp->inputs, comp->nIn * sizeof(FMIReal));
	comp->ipData->derivativeTime = comp->time;
	comp->ipData->useDerivatives = 1;
	util_logger(comp, comp->instanceName, FMIOK, loggFuncCall, "%s", label);
	return FMIOK;
#endif /* ONLY_INCLUDE_INLINE_INTEGRATION */
}

#if (FMI_VERSION >= FMI_3)
DYMOLA_STATIC FMIStatus fmi3GetOutputDerivatives_(fmi3Instance c,
                                                const fmi3ValueReference vr[],
                                                size_t nvr,
                                                const fmi3Int32 order[],
                                                fmi3Float64 value[],
                                                size_t nValues)
#else
DYMOLA_STATIC FMIStatus fmiGetRealOutputDerivatives_(FMIComponent c,
													 const   FMIValueReference vr[],
													 size_t  nvr,
													 const   FMIInteger order[],
													 FMIReal value[])
#endif
{

#if (FMI_VERSION >= FMI_3)
	static FMIString label = "fmi3GetRealOutputDerivatives";
	static FMIString setget = "";
#elif (FMI_VERSION >= FMI_2)
	static FMIString label = "fmi2GetRealOutputDerivatives";
#else
	static FMIString label = "fmiGetRealOutputDerivatives";
#endif
	Component* comp = (Component*) c;
#ifdef ONLY_INCLUDE_INLINE_INTEGRATION
	util_logger(comp, comp->instanceName, FMIError, loggBadCall, "%s is not suported with inline integration",label);
	return FMIWarning;
#else
	InterpolationData* ipData = comp->ipData;
	FMIReal dt = comp->time - ipData->timePrev;
	size_t ny = comp->nOut; 
	size_t i;
#if FMI_VERSION >= FMI_3
	ERROR_RETURN_CHECK(checkSizes(c, vr, nvr, nValues, label, setget))
#endif
	if(comp->istruct->mInlineIntegration){
		util_logger(comp, comp->instanceName, FMIError, loggBadCall, "%s is not suported with inline integration",label);
		return util_error(comp);
	}
	if(	comp->mStatus == modelInstantiated){
		util_logger(comp, comp->instanceName, FMIError, loggBadCall,
#if (FMI_VERSION >= FMI_3)
			"%s: fmi3EnterInitializationMode must be called before calling %s", label, label);
#elif (FMI_VERSION >= FMI_2)
			"%s: fmi2EnterInitializationMode must be called before calling %s", label, label);
#else
			"%s: fmiInitializeSlave must be called before calling %s", label, label);
#endif
		return util_error(comp);
	}
	if (nvr == 0 || nvr > ny) {
		util_logger(comp, comp->instanceName, FMIWarning, loggBadCall,
			"%s: invalid nvr = %d (number of outputs = %d)", label, nvr, ny);
		return FMIWarning;
	}

	/* fetch new output values */
	if (util_refresh_cache(comp, iDemandOutput, label, NULL)) {
		return util_error(comp);
	}

	for (i = 0; i < nvr; i++) {
		const FMIValueReference r = vr[i];
		int ix = FMI_INDEX(r);

		if (order[i] == 1) {
			switch (FMI_CATEGORY(r)) {
			case dsOutput:
				if (dt <= 0) {
					util_logger(comp, comp->instanceName, FMIWarning, loggUndef,
						"%s: time interval for estimate is %f, returning 0", label, dt);
					value[i] = 0;
				}
				value[i] = (comp->outputs[ix] - comp->ipData->outputsPrev[ix]) / dt;
				break;
			default:
				util_logger(comp, comp->instanceName, FMIError, loggBadCall, "%s: variable is not output", label);
				return util_error(comp);
				break;
			}
		} else if (order[i] > 1) {
			value[i] = 0;
		} else {
			util_logger(comp, comp->instanceName, FMIError, loggBadCall, "%s: derivative order 0 is not allowed", label);
			return util_error(comp);
		}
	}
	util_logger(comp, comp->instanceName, FMIOK, loggFuncCall, "%s", label);
	return FMIOK;
#endif /* ONLY_INCLUDE_INLINE_INTEGRATION */
}


/* ---------------------------------------------------------------------- */
DYMOLA_STATIC 
#if (FMI_VERSION >= FMI_3)
FMIStatus fmi3DoStep_(FMIComponent c, FMIReal currentCommunicationPoint, FMIReal communicationStepSize,
					 FMIBoolean noSetFMUStatePriorToCurrentPoint, FMIBoolean* eventEncountered,
					 FMIBoolean* terminateSimulation, FMIBoolean* earlyReturn, FMIReal* lastSuccessfulTime) 
{
	static FMIString label = "fmi3DoStep";
#elif (FMI_VERSION >= FMI_2)
FMIStatus fmiDoStep_(FMIComponent c, FMIReal currentCommunicationPoint, FMIReal communicationStepSize, 
					 FMIBoolean noSetFMUStatePriorToCurrentPoint)
{
	static FMIString label = "fmi2DoStep";
#else
FMIStatus fmiDoStep_(FMIComponent c, FMIReal currentCommunicationPoint, FMIReal communicationStepSize,
					 FMIBoolean newStep)
{
	static FMIString label = "fmiDoStep";
#endif
	Component* comp = (Component*) c;
	int QiErr = 0;
	FMIStatus status;
	FMIReal endStepTime;
	FMIBoolean finalTimeEventCalled = FMIFalse;
	FMIReal tdev = fabs(comp->time - currentCommunicationPoint);
#if (FMI_VERSION >= FMI_3)
	/* Just to have consistent values*/
	if (comp->earlyReturnAllowed) {
		if (earlyReturn) *earlyReturn = FMIFalse;
	}
	if (eventEncountered) *eventEncountered = FMIFalse; /* Should not be NULL - but just in case importer messed it up */ 
#endif
	if(comp->mStatus == modelInstantiated || comp->mStatus== modelInitializationMode){
		util_logger(comp, comp->instanceName, FMIError, loggBadCall,
				"%s: Initialization must be finished before calling fmiDoStep", label);
		return util_error(comp);
	}
	if (comp->mStatus == modelTerminated || comp->terminationByModel == FMITrue) {
		util_logger(comp, comp->instanceName, FMIError, loggBadCall, "%s: model is terminated",label);
		return util_error(comp);
	}
#if (FMI_VERSION >= FMI_3)
	if(comp->eventModeUsed){
		if(comp->mStatus !=  modelContinousTimeMode){
			util_logger(comp, comp->instanceName, FMIError, loggBadCall, "%s: fmi3EnterStepMode must be called before dostep since the fmu was instantiated with eventModeUsed",label);
			return util_error(comp);
		}
		if (comp->eventIterRequired){
			util_logger(comp, comp->instanceName, FMIError, loggBadCall, "%s: fmu exited with event and eventIteration has to be done before new call to doStep",label);
			return util_error(comp);
		}
	}
#endif
	/* check if reasonably close to expected time */
	if (!comp->istruct->mInlineIntegration ? tdev > 0 : tdev > 1.5*comp->dstruct->mDymolaFixedStepSize) {
		if (tdev >= SMALL_TIME_DEV(comp->time)) {
			util_logger(comp, comp->instanceName, FMIWarning, loggUndef,
				"%s: currentCommunicationPoint = %g, expected last stop time at %g,\n this indicates a mismatch in the master. Simulation will try to continue.",label, currentCommunicationPoint, comp->time);
		}
	}
#if (FMI_VERSION < FMI_2)
	if (communicationStepSize == 0) {
		return handleEvent(c, FMITrue);
	}

	if (newStep == FMIFalse) {
		util_logger(comp, comp->instanceName, FMIWarning, loggBadCall, "%s: newStep = fmiFalse not supported", label);
		return FMIWarning;
	}
#endif

	/*endStepTime = comp->time + communicationStepSize;*/
	endStepTime = currentCommunicationPoint + communicationStepSize;
	if (comp->StopTimeDefined) {
		/* check if reasonably within stop time */
		tdev = endStepTime - comp->tStop;
		if (tdev > 0) {
			/* allow twice as much since we might have adjusted communicationStepSize already */
			if (tdev > 2 * SMALL_TIME_DEV(comp->tStop)) {
				util_logger(comp, comp->instanceName, FMIWarning, loggUndef,
					"%s: trying to compute past tStop = %f, to %.16f,  at t = %f", "",label, comp->tStop, endStepTime, comp->time);
				return FMIWarning;
			}
			/* adjust end time */
			util_logger(comp, comp->instanceName, FMIOK, loggUndef,
				"t = %f: Reducing communicationStepSize %.16e s to avoid passing tStop.\n", comp->time, tdev);
			endStepTime = comp->tStop;
		}
	}
#ifndef ONLY_INCLUDE_INLINE_INTEGRATION
	if(!comp->istruct->mInlineIntegration){
		/* handle any values changed externally */
#if (FMI_VERSION >= FMI_3)
		if (comp->valWasSet && !comp->eventModeUsed) {
#else 
		if (comp->valWasSet)  {
#endif
			FMIStatus status = handleEvent(c, FMITrue);
			comp->valWasSet = 0;
			if (status != FMIOK) {
				return status;
			}
		}else{
			if(comp->reinitCvode){
				if (integration_reinit(comp, 1) != integrationOK){
				util_logger(comp, comp->instanceName, FMIError, loggUndef, "%s: failed to restart solver after updated input.",label);
				return util_error(comp);
				} 
			}		/* check if re-computing derivatives necessary */
			if (comp->icall < iDemandDerivative) {
				int QiErr = util_refresh_cache(comp, iDemandDerivative, label, NULL);
				if (QiErr != 0) {
					return (QiErr == 1) ? FMIDiscard : util_error(comp);
				}
			}
		}
		/* integration should have been setup by now */
		assert(comp->iData != NULL);

#if !defined(FMU_SOURCE_CODE_EXPORT) || defined(FMU_SOURCE_CODE_EXPORT_SPARSE)
		DYNPropagateDidToThread(comp->did);
		EXCEPTION_TRY
		if (comp->iData->sparseData.nnz && comp->iData->sparseData.delayCVSuperLUMTCall) {
			if (PerformDelayedCvSuperLUMTCall(comp))
				return util_error(comp);
		}
		EXCEPTION_CATCH_ALL
			return FMIFatal;
		EXCEPTION_END;
#endif

		/* Time events can be predicted/handled outside integration steps while state/step events must be
		   detected within (since we cannot predict their time). Step events are OK to handle after
		   each step. */
		while (comp->time < endStepTime || endStepTime == comp->dstruct->mNextTimeEvent && !finalTimeEventCalled ) {
			FMIBoolean callEventUpdate = FMIFalse;
			if (comp->time < comp->dstruct->mNextTimeEvent) {
#if (FMI_VERSION >= FMI_3)
				FMIReal tRetEarly;
#endif
				FMIReal intStopTime = MIN(comp->dstruct->mNextTimeEvent,comp->nextResultSampleTime);
				FMIReal nextTime = MIN(endStepTime, intStopTime);
				/* integrate */
				/* A bit messy*/
				switch(
#if (FMI_VERSION >= FMI_3)
					(comp->earlyReturnAllowed && comp->eventModeUsed)?integration_step_return_at_event(comp, &nextTime, comp->eventModeUsed, comp->earlyReturnAllowed, &tRetEarly, eventEncountered):
#endif
					integration_step(comp, &nextTime)) {
					case integrationOK:
#if (FMI_VERSION >= FMI_3)
						if (lastSuccessfulTime) *lastSuccessfulTime = nextTime;
#endif
						break;
					case integrationTerminate:
#if (FMI_VERSION >= FMI_3)
						if (terminateSimulation) *terminateSimulation = fmi3True;
						status = fmi3OK;
#else
						status = FMIDiscard;
#endif
						util_logger(comp, comp->instanceName, status, loggUndef, "%s: simulation terminated by model",label);
						if (comp->steadyStateReached)
							util_logger(comp, comp->instanceName, status, loggUndef, "%s: simulation terminated after a steady state was reached",label);
						comp->terminationByModel = FMITrue;
						return status;
					case integrationError:
						util_logger(comp, comp->instanceName, FMIError, loggUndef, "%s: integration_step failed",label);
						return util_error(comp);
#if (FMI_VERSION >= FMI_3)
					case integrationEvent:
						/* Early event should be set */
						if (earlyReturn) *earlyReturn = FMITrue;
						if (lastSuccessfulTime) *lastSuccessfulTime = tRetEarly;
						comp->eventIterRequired = 1;
						return FMIOK;
#endif
					case integrationFatal:
						return FMIFatal;
				}
				fmiSetTime_(c, nextTime);
				/* compute approximate inputs */
				integration_extrapolate_inputs(comp, nextTime); 
				/* check step events in case they occurred at the very end time of step */
				if (comp->istruct->mTriggerStepEvent != 0) {
					callEventUpdate = FMITrue;
#ifdef LOG_EVENTS
					util_logger(c, comp->instanceName, FMIOK, loggUndef, "%.12f: step event", comp->time);
#endif
				}
				if(comp->storeResult && nextTime == comp->nextResultSampleTime && !callEventUpdate && nextTime != comp->dstruct->mNextTimeEvent){
					/*if event sampling will be handled at the event instead*/
					CS_RESULT_SAMPLE(FMIFalse);
				}
			} else {
				fmiSetTime_(c, comp->dstruct->mNextTimeEvent);
				if(comp->time == endStepTime){
					finalTimeEventCalled=FMITrue;
				}
				callEventUpdate = FMITrue;
#ifdef LOG_EVENTS
				util_logger(c, comp->instanceName, FMIOK, loggUndef, "%.12f: time event", comp->time);
#endif
			}
			if (callEventUpdate) {
#if (FMI_VERSION >= FMI_3)
				if (comp->eventModeUsed) {
					if (earlyReturn) *earlyReturn = comp->time < endStepTime;
					if (lastSuccessfulTime) *lastSuccessfulTime = comp->time;
					if(eventEncountered) *eventEncountered = FMITrue;
					comp->eventIterRequired = 1;
					return FMIOK;
				}
#endif
				{
				FMIStatus status;
				CS_RESULT_SAMPLE(FMITrue);
				status = handleEvent(c, FMIFalse);
#if (FMI_VERSION >= FMI_3)
				if ((status == fmi3Discard) && terminateSimulation) *terminateSimulation = fmi3True;
#endif
				if (status != FMIOK) {
					return status;
				}
				CS_RESULT_SAMPLE(FMITrue);
				}
			}
		}
		comp->time = endStepTime;
		comp->ipData->useDerivatives = 0;
	}else{
#endif /* ndef ONLY_INCLUDE_INLINE_INTEGRATION */
		double stepTime = 0;
		double h = comp->dstruct->mDymolaFixedStepSize;
		unsigned long long startStepCount = comp->inlineStepCounter;
		while( (stepTime=(++comp->inlineStepCounter)*h) < endStepTime+0.1*h){
			status = fmiSetTime_(c, stepTime);
			if(status!=FMIOK){
#if (FMI_VERSION >= FMI_3)
				if (status == FMIDiscard) {
					if (terminateSimulation) *terminateSimulation = fmi3True;
				}
#endif
				return status;
			}
			status = handleEvent(c, FMIFalse);
			if (status != FMIOK) {
#if (FMI_VERSION >= FMI_3)
				
				if (status == FMIDiscard) {
					if (terminateSimulation) *terminateSimulation = fmi3True;
				}
#endif
				return status;
			}
#if (FMI_VERSION >= FMI_3)
			if (lastSuccessfulTime) *lastSuccessfulTime = stepTime;
#endif
		}
		/*Remove last step counter increase that failed while statement*/
		--comp->inlineStepCounter;
		if(startStepCount != comp->inlineStepCounter){
			CS_RESULT_SAMPLE(FMITrue);
		}

#ifndef ONLY_INCLUDE_INLINE_INTEGRATION
	}
#endif
	util_logger(comp, comp->instanceName, FMIOK, loggFuncCall, "%s", label);
	return FMIOK;
}
#if (FMI_VERSION >= FMI_2) 
DYMOLA_STATIC FMIStatus fmiHybridDoStep_(FMIComponent c, FMIReal currentCommunicationPoint, FMIReal communicationStepSize, 
					 FMIBoolean noSetFMUStatePriorToCurrentPoint, FMIBoolean *encounteredEvent, FMIReal *endTime)

{
#ifndef ONLY_INCLUDE_INLINE_INTEGRATION
	Component* comp = (Component*) c;
	static FMIString label = "fmi2HybridDoStep";
	int QiErr = 0;
	FMIStatus status;
	FMIReal endStepTime;
	FMIReal tdev = fabs(comp->time - currentCommunicationPoint);

	if (comp->mStatus == modelTerminated || comp->terminationByModel == FMITrue) {
		util_logger(comp, comp->instanceName, FMIError, loggBadCall, "%s: model is terminated", label);
		return util_error(comp);
	}
		/* handle any values changed externally */
	if (comp->eventIterRequired || comp->valWasSet || comp->mStatus == modelCsPreEventMode || comp->hycosimInputEvent) {
		/* fmiStatus status = handleEvent(c); */
		comp->mStatus = modelCsPreEventMode;
		comp->eventIterRequired = 1;
		*encounteredEvent=FMITrue;
		*endTime = comp->time;
		return FMIOK;
	}

	if(comp->mStatus != modelContinousTimeMode){
		if(comp->mStatus == modelEventModeExit){
			/*We have to reinit solution after event*/
			if (integration_reinit(comp, 1) != integrationOK) {
				return util_error(comp);
			}
			comp->mStatus = modelContinousTimeMode;
		}else{
			util_logger(comp, comp->instanceName, FMIError, loggBadCall, "%s: more eventIteration needed before calling hybridDoStep",label);
			return util_error(comp);
		}
	}
	/* check if reasonably close to expected time */
	if (tdev > 0) {
		if (tdev >= SMALL_TIME_DEV(comp->time)) {
			util_logger(comp, comp->instanceName, FMIDiscard, loggBadCall,
				"%s: currentCommunicationPoint = %.16f, expected %.16f", label, currentCommunicationPoint, comp->time);
			return FMIDiscard;
		}
		/* compensate by adjusting step size, unless event iteration request */
		if (communicationStepSize != 0) {
			FMIReal dh = currentCommunicationPoint - comp->time;
#ifdef FMU_VERBOSITY
			util_logger(comp, comp->instanceName, FMIOK, loggUndef,
				"t=%f: Adjusting communicationStepSize with %.16e s.\n", comp->time, dh);
#endif
			communicationStepSize += dh;
		}
	}

	if(communicationStepSize == 0) {
		return handleEvent(c, FMITrue);
		/*TODO new event handling at 0 step?*/
	}

	endStepTime = currentCommunicationPoint + communicationStepSize;
	if (comp->StopTimeDefined) {
		/* check if reasonably within stop time */
		tdev = endStepTime - comp->tStop;
		if (tdev > 0) {
			/* allow twice as much since we might have adjusted communicationStepSize already */
			if (tdev > 2 * SMALL_TIME_DEV(comp->tStop)) {
				util_logger(comp, comp->instanceName, FMIError, loggUndef,
					"%s: trying to compute past tStop = %f, to %.16f,  at t = %f", label, comp->tStop, endStepTime, comp->time);
				return util_error(comp);
			}
			/* adjust end time */
			util_logger(comp, comp->instanceName, FMIOK, loggUndef,
				"t = %f: Reducing communicationStepSize %.16e s to avoid passing tStop.\n", comp->time, tdev);
			endStepTime = comp->tStop;
		}
	}
	/* check if re-computing derivatives necessary */
	else if (comp->icall < iDemandDerivative) {
		int QiErr = util_refresh_cache(comp, iDemandDerivative, label, NULL);
		if (QiErr != 0) {
			return (QiErr == 1) ? FMIDiscard : util_error(comp);
		}
	}

	/* integration should have been setup by now */
	assert(comp->iData != NULL);

#if !defined(FMU_SOURCE_CODE_EXPORT) || defined(FMU_SOURCE_CODE_EXPORT_SPARSE)
	DYNPropagateDidToThread(comp->did);
	EXCEPTION_TRY
	if (comp->iData->sparseData.nnz && comp->iData->sparseData.delayCVSuperLUMTCall) {
		if (PerformDelayedCvSuperLUMTCall(comp))
			return util_error(comp);
	}
	EXCEPTION_CATCH_ALL
		return FMIFatal;
	EXCEPTION_END;
#endif

	/* Time events can be predicted/handled outside integration steps while state/step events must be
	   detected within (since we cannot predict their time). Step events are OK to handle after
	   each step. */
	while (comp->time < endStepTime || endStepTime == comp->dstruct->mNextTimeEvent){
		if (comp->time < comp->dstruct->mNextTimeEvent) {
			FMIBoolean dummyEvent;
			FMIReal nextTime = MIN(endStepTime, comp->dstruct->mNextTimeEvent);
			/* integrate */
			switch(integration_step_return_at_event(comp, &nextTime, FMITrue, FMITrue, endTime, &dummyEvent)) {
				case integrationOK:
					break;
				case integrationTerminate:
					status = FMIDiscard;
					util_logger(comp, comp->instanceName, status, loggUndef, "%s: simulation terminated by model",label);
					if (comp->steadyStateReached)
						util_logger(comp, comp->instanceName, status, loggUndef, "%s: simulation terminated after a steady state was reached", label);
					comp->terminationByModel = FMITrue;
					return status;
				case integrationError:
					util_logger(comp, comp->instanceName, FMIError, loggUndef, "%s: integration_step failed", label);
					return util_error(comp);
				case integrationEvent:
					*encounteredEvent=FMITrue;
					comp->mStatus = modelCsPreEventMode;
					comp->firstEventCall = FMITrue;
					return FMIOK;
				case integrationFatal:
					return FMIFatal;
			}
			fmiSetTime_(c, nextTime);
				/* compute approximate inputs */
			integration_extrapolate_inputs(comp, nextTime); 
				/* check step events in case they occurred at the very end time of step */
			if (comp->istruct->mTriggerStepEvent != 0) {
				comp->eventIterRequired =1;
				*encounteredEvent=FMITrue;
#ifdef LOG_EVENTS
				util_logger(c, comp->instanceName, FMIOK, loggUndef, "%.12f: step event", comp->time);
#endif
				*endTime = nextTime;
				comp->mStatus = modelCsPreEventMode;
				comp->firstEventCall = FMITrue;
				return FMIOK;
			}
		} else {
			fmiSetTime_(c, comp->dstruct->mNextTimeEvent);
			*endTime = comp->dstruct->mNextTimeEvent;
			*encounteredEvent=FMITrue;
			comp->mStatus = modelCsPreEventMode;
			comp->firstEventCall = FMITrue;
			return FMIOK;
#ifdef LOG_EVENTS
			util_logger(c, comp->instanceName, FMIOK, loggUndef, "%.12f: time event", comp->time);
#endif
		}
	}

	*encounteredEvent=FMIFalse;
    comp->time = endStepTime;
	comp->mStatus = modelContinousTimeMode;
	comp->firstEventCall = FMITrue;
	CS_RESULT_SAMPLE(FMITrue);

	comp->ipData->useDerivatives = 0;
#endif /* ndef ONLY_INCLUDE_INLINE_INTEGRATION */ 
	return FMIOK;
}

DYMOLA_STATIC FMIStatus fmiGetNextEventTime_(FMIComponent c, FMIReal *eventTime)
{
	Component* comp = (Component*) c;
	*eventTime = comp->dstruct->mNextTimeEvent;
	return FMIOK;
}

#endif /*FMI_2*/

#if (FMI_VERSION < FMI_3)
/* ---------------------------------------------------------------------- */
DYMOLA_STATIC FMIStatus fmiCancelStep_(FMIComponent c)
{
	Component* comp = (Component*) c;
#if (FMI_VERSION >= FMI_2)
	static FMIString label = "fmi2CancelStep";
#else
	static FMIString label = "fmiCancelStep";
#endif
	util_logger(comp, comp->instanceName, FMIDiscard, loggBadCall, "%s: asynchronous execution of fmiDoStep is not supported",label);
	return FMIDiscard; /*	asynchronous execution of fmiDoStep is not supported,
							so this method should not be called at all */
}

/* ---------------------------------------------------------------------- */
DYMOLA_STATIC FMIStatus fmiGetStatus_(FMIComponent c, const FMIStatusKind s, FMIStatus* value)
{
#if (FMI_VERSION >= FMI_2)
	static FMIString label = "fmi2GetStatus";
#else
	static FMIString label = "fmiGetStatus";
#endif
	Component* comp = (Component*) c;
	util_logger(comp, comp->instanceName, FMIDiscard, loggBadCall,
		"%s: not supported since asynchronous execution of fmiDoStep is not supported",label);
	return FMIDiscard;
}

/* ---------------------------------------------------------------------- */
DYMOLA_STATIC FMIStatus fmiGetRealStatus_(FMIComponent c, const FMIStatusKind s, FMIReal* value)
{
#if (FMI_VERSION >= FMI_2)
	static FMIString label = "fmi2GetRealStatus";
#else
	static FMIString label = "fmiGetRealStatus";
#endif
	Component* comp = (Component*) c;
	if (s != FMILastSuccessfulTime) {
		util_logger(comp, comp->instanceName, FMIDiscard, loggBadCall, "%s: fmiStatusKind %d unknown",label, s);
		return FMIDiscard;
	}
	*value = comp->time;
	return FMIOK;
}

/* ---------------------------------------------------------------------- */
DYMOLA_STATIC FMIStatus fmiGetIntegerStatus_(FMIComponent c, const FMIStatusKind s, FMIInteger* value)
{
#if (FMI_VERSION >= FMI_2)
	static FMIString label = "fmi2GetIntegerStatus";
#else
	static FMIString label = "fmiGetIntegerStatus";
#endif
	Component* comp = (Component*) c;
	util_logger(comp, comp->instanceName, FMIDiscard, loggBadCall, "%s: fmiStatusKind %d unknown", label, s);
	return FMIDiscard;
}

/* ---------------------------------------------------------------------- */
DYMOLA_STATIC FMIStatus fmiGetBooleanStatus_(FMIComponent c, const FMIStatusKind s, FMIBoolean* value)
{
#if (FMI_VERSION >= FMI_2)
	static FMIString label = "fmi2GetBooleanStatus";
#else
	static FMIString label = "fmiGetBooleanStatus";
#endif
	Component* comp = (Component*) c;
#if (FMI_VERSION >= FMI_2)
	if (s != FMITerminated)
#endif
	{
		util_logger(comp, comp->instanceName, FMIDiscard, loggBadCall, "%s: fmiStatusKind %d unknown",label, s);
		return FMIDiscard;
	}
#if (FMI_VERSION >= FMI_2)
	*value = comp->terminationByModel;
	return FMIOK;
#endif
}

/* ---------------------------------------------------------------------- */
DYMOLA_STATIC FMIStatus fmiGetStringStatus_(FMIComponent c, const FMIStatusKind s, FMIString* value)
{
#if (FMI_VERSION >= FMI_2)
	static FMIString label = "fmi2GetStringStatus";
#else
	static FMIString label = "fmiGetStringStatus";
#endif
	Component* comp = (Component*) c;
	util_logger(comp, comp->instanceName, FMIDiscard, loggBadCall,
		"%s: not supported since asynchronous execution of fmiDoStep is not supported",label);
	return FMIDiscard;
}
#endif
/* ----------------- local function definitions ----------------- */
static FMIStatus handleEvent(FMIComponent c, FMIBoolean forceReset)
{
	Component* comp = (Component*) c;
	FMIStatus status;
#if (FMI_VERSION >= FMI_2)
	FMIBoolean terminateSimulation;
#else
	FMIEventInfo eventInfo;
#endif
	comp->anyNonEvent = FMIFalse;

	status = util_event_update(comp, FMIFalse,
#if (FMI_VERSION >= FMI_2)
		&terminateSimulation);
#else
		&eventInfo);
#endif
	if (status != FMIOK) {
		return status;
	}

#if (FMI_VERSION >= FMI_2)
	if (terminateSimulation)  {
#else
	if (eventInfo.terminateSimulation) {
#endif
		status = FMIDiscard;
		util_logger(comp, comp->instanceName, status, loggUndef, "fmiDoStep: simulation terminated by model");
		if (comp->steadyStateReached)
			util_logger(comp, comp->instanceName, status, loggUndef, "fmiDoStep: simulation terminated after a steady state was reached");
		comp->terminationByModel = FMITrue;
		return status;
	}

#if (FMI_VERSION < FMI_2)
	assert(eventInfo.stateValueReferencesChanged == FMIFalse);
#endif
#ifndef ONLY_INCLUDE_INLINE_INTEGRATION
	if(!comp->istruct->mInlineIntegration && (!comp->anyNonEvent || forceReset)){
		switch (integration_reinit(comp,1))
		{
		case integrationOK:
			break;
		case integrationFatal:
			return FMIFatal;
		default:
			return util_error(comp);
		} 
	}
#endif
	return FMIOK;
}
#if (FMI_VERSION >= FMI_3)
DYMOLA_STATIC FMIStatus fmi3EnterStepMode_(fmi3Instance instance) {
#ifndef ONLY_INCLUDE_INLINE_INTEGRATION
	Component* comp = (Component*) instance;
	FMIStatus status = FMIError;
	if (comp->mStatus == modelContinousTimeMode) {
		util_logger(comp, comp->instanceName, status, loggUndef, "fmi3EnterStepMode: called twice");
		return status;
	}
	comp->mStatus = modelContinousTimeMode;
	status = FMIOK;
	return status;
#else
	unSuportedFunctionCall(instance, "fmi3EnterStepMode");
	return util_error((Component*)instance);
#endif
}

DYMOLA_STATIC FMIStatus fmi3ActivateModelPartition_(fmi3Instance instance,
                                                  fmi3ValueReference clockReference,
                                                  fmi3Float64 activationTime) {
    unSuportedFunctionCall(instance, "fmi3ActivateModelPartition");
	return util_error((Component*)instance);
}

#endif


#endif /* FMU_SKIP_CO_SIM */
