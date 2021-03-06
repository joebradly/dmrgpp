#include <string>
const std::string license=
"Copyright (c) 2009-2012 , UT-Battelle, LLC\n"
"All rights reserved\n"
"\n"
"[DMRG++, Version 2.0.0]\n"
"\n"
"*********************************************************\n"
"THE SOFTWARE IS SUPPLIED BY THE COPYRIGHT HOLDERS AND\n"
"CONTRIBUTORS \"AS IS\" AND ANY EXPRESS OR IMPLIED\n"
"WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED\n"
"WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A\n"
"PARTICULAR PURPOSE ARE DISCLAIMED.\n"
"\n"
"Please see full open source license included in file LICENSE.\n"
"*********************************************************\n"
"\n";

typedef double RealType;

#include <unistd.h>
#include "Observer.h"
#include "ObservableLibrary.h"
#include "IoSimple.h"
#include "ModelFactory.h"
#include "OperatorsBase.h"
#ifndef USE_MPI
#include "ConcurrencySerial.h"
typedef PsimagLite::ConcurrencySerial<RealType> ConcurrencyType;
#else
#include "ConcurrencyMpi.h"
typedef PsimagLite::ConcurrencyMpi<RealType> ConcurrencyType;
#endif
#include "Geometry.h" 
#include "CrsMatrix.h"
#include "ModelHelperLocal.h"
#include "ModelHelperSu2.h"
#include "InternalProductOnTheFly.h"
#include "VectorWithOffset.h"
#include "VectorWithOffsets.h"
#include "GroundStateTargetting.h"
#include "DmrgSolver.h" // only used for types
#include "TimeStepTargetting.h"
#include "DynamicTargetting.h"
#include "AdaptiveDynamicTargetting.h"
#include "CorrectionTargetting.h"
#include "MettsTargetting.h" // experimental
#include "BasisWithOperators.h"
#include "LeftRightSuper.h"
#include "InputNg.h"
#include "Provenance.h"

using namespace Dmrg;

typedef std::complex<RealType> ComplexType;

typedef  PsimagLite::CrsMatrix<ComplexType> MySparseMatrixComplex;
typedef  PsimagLite::CrsMatrix<RealType> MySparseMatrixReal;
typedef  PsimagLite::Geometry<RealType,ProgramGlobals> GeometryType;
typedef PsimagLite::IoSimple::In IoInputType;
typedef PsimagLite::InputNg<InputCheck> InputNgType;
typedef ParametersDmrgSolver<RealType,InputNgType::Readable> DmrgSolverParametersType;

template<typename ModelType>
size_t dofsFromModelName(const ModelType& model)
{
	const std::string& modelName = model.params().model;
	size_t site = 0; // FIXME : account for Hilbert spaces changing with site
	size_t dofs = size_t(log(model.hilbertSize(site))/log(2.0));
	std::cerr<<"DOFS= "<<dofs<<" <------------------------------------\n";
	if (modelName.find("FeAsBasedSc")!=std::string::npos) return dofs;
	if (modelName.find("FeAsBasedScExtended")!=std::string::npos) return dofs;
	if (modelName.find("HubbardOneBand")!=std::string::npos) return dofs;
	return 0;
}

template<typename VectorWithOffsetType,typename ModelType,typename SparseMatrixType,typename OperatorType,typename TargettingType>
bool observeOneFullSweep(
	IoInputType& io,
	const GeometryType& geometry,
	const ModelType& model,
	const std::string& obsOptions,
	bool hasTimeEvolution,
	ConcurrencyType& concurrency)
{
	bool verbose = false;
	typedef typename SparseMatrixType::value_type FieldType;
	typedef Observer<FieldType,VectorWithOffsetType,ModelType,IoInputType> 
		ObserverType;
	typedef ObservableLibrary<ObserverType,TargettingType> ObservableLibraryType;
	size_t n  = geometry.numberOfSites();
	//std::string sSweeps = "sweeps=";
	//std::string::size_type begin = obsOptions.find(sSweeps);
	//if (begin != std::string::npos) {
	//	std::string sTmp = obsOptions.substr(begin+sSweeps.length(),std::string::npos);
		//std::cout<<"sTmp="<<sTmp<<"\n";
	//	n = atoi(sTmp.c_str());
	//}
	ObservableLibraryType observerLib(io,n,hasTimeEvolution,model,concurrency,verbose);
	
	bool ot = false;
	if (obsOptions.find("ot")!=std::string::npos || obsOptions.find("time")!=std::string::npos) ot = true;
	if (hasTimeEvolution && ot) {
		observerLib.measureTime("superDensity");
		observerLib.measureTime("nupNdown");
		observerLib.measureTime("nup+ndown");
		if (obsOptions.find("sz")!=std::string::npos) observerLib.measureTime("sz");
	}

	if (hasTimeEvolution) observerLib.setBrackets("time","time");

	const std::string& modelName = model.params().model;
	size_t rows = n; // could be n/2 if there's enough symmetry

	size_t numberOfDofs = dofsFromModelName(model);
	if (!hasTimeEvolution && obsOptions.find("onepoint")!=std::string::npos) {
		observerLib.measureTheOnePoints(numberOfDofs);
	}

	if (modelName.find("Heisenberg")==std::string::npos) {
		if (obsOptions.find("cc")!=std::string::npos) {
			observerLib.measure("cc",rows,n);
		}

		if (obsOptions.find("nn")!=std::string::npos) {
			observerLib.measure("nn",rows,n);
		}
	}
	if (obsOptions.find("szsz")!=std::string::npos) {
		observerLib.measure("szsz",rows,n);
	}

	if (modelName.find("FeAsBasedSc")!=std::string::npos ||
	    modelName.find("FeAsBasedScExtended")!=std::string::npos) {
		bool dd4 = (obsOptions.find("dd4")!=std::string::npos);

		if (obsOptions.find("dd")!=std::string::npos && !dd4 &&
			geometry.label(0).find("ladder")!=std::string::npos) {
			observerLib.measure("dd",rows,n);
		}

		// FOUR-POINT DELTA-DELTA^DAGGER:
		if (dd4 && geometry.label(0).find("ladder")!=std::string::npos) {
			observerLib.measure("dd4",rows,n);
		} // if dd4
	}

	if (obsOptions.find("s+s-")!=std::string::npos) {
		observerLib.measure("s+s-",rows,n);
	}
//	if (obsOptions.find("s-s+")!=std::string::npos) {
//		observerLib.measure("s-s+",rows,n);
//	}
	if (obsOptions.find("ss")!=std::string::npos) {
		observerLib.measure("ss",rows,n);
	}

	return observerLib.endOfData();
}

template<template<typename,typename> class ModelHelperTemplate,
         template<typename> class VectorWithOffsetTemplate,
         template<template<typename,typename,typename> class,
		  template<typename,typename> class,
                  template<typename,typename> class,
                  typename,typename,typename,
         template<typename> class> class TargettingTemplate,
         typename MySparseMatrix>
void mainLoop(GeometryType& geometry,
              const std::string& targetting,
              ConcurrencyType& concurrency,
	      InputNgType::Readable& io,
	      const DmrgSolverParametersType& params,
              const std::string& obsOptions)
{
	typedef Operator<RealType,MySparseMatrix> OperatorType;
	typedef Basis<RealType,MySparseMatrix> BasisType;
	typedef OperatorsBase<OperatorType,BasisType> OperatorsType;
	typedef typename OperatorType::SparseMatrixType SparseMatrixType;
	typedef BasisWithOperators<OperatorsType,ConcurrencyType> BasisWithOperatorsType; 
	typedef LeftRightSuper<BasisWithOperatorsType,BasisType> LeftRightSuperType;
	typedef ModelHelperTemplate<LeftRightSuperType,ConcurrencyType> ModelHelperType;
	typedef ModelFactory<ModelHelperType,MySparseMatrix,GeometryType,PTHREADS_NAME,DmrgSolverParametersType> ModelType;
	typedef TargettingTemplate<PsimagLite::LanczosSolver,
	                           InternalProductOnTheFly,
	                           WaveFunctionTransfFactory,
	                           ModelType,
	                           ConcurrencyType,
	                           IoInputType,
	                           VectorWithOffsetTemplate> TargettingType;

	typedef DmrgSolver<InternalProductOnTheFly,TargettingType> SolverType;
	
	typedef typename TargettingType::VectorWithOffsetType VectorWithOffsetType;
	
	ModelType model(params,io,geometry,concurrency);

	 //! Read TimeEvolution if applicable:
	typedef typename TargettingType::TargettingParamsType TargettingParamsType;
	TargettingParamsType tsp(io,model);
	
	bool moreData = true;
	const std::string& datafile = params.filename;
	IoInputType dataIo(datafile);
	bool hasTimeEvolution = (targetting == "TimeStepTargetting") ? true : false;
	while (moreData) {
		try {
			moreData = !observeOneFullSweep<VectorWithOffsetType,ModelType,
			            SparseMatrixType,OperatorType,TargettingType>
			(dataIo,geometry,model,obsOptions,hasTimeEvolution,concurrency);
		} catch (std::exception& e) {
			std::cerr<<"CAUGHT: "<<e.what();
			std::cerr<<"There's no more data\n";
			break;
		}

		//if (!hasTimeEvolution) break;
	}
}

void usage(const char* name)
{
	std::cerr<<"USAGE is "<<name<<" -f filename [-o options]\n";
}

int main(int argc,char *argv[])
{
	using namespace Dmrg;

	std::string filename="";
	std::string options = "";
	int opt = 0;
	while ((opt = getopt(argc, argv,"f:o:")) != -1) {
		switch (opt) {
		case 'f':
			filename = optarg;
			break;
		case 'o':
			options = optarg;
			break;
		default:
			usage(argv[0]);
			return 1;
		}
	}

	//sanity checks here
	if (filename=="") {
		usage(argv[0]);
		return 1;
	}

	ConcurrencyType concurrency(argc,argv);
	
	// print license
	if (concurrency.root()) {
		std::cerr<<license;
		Provenance provenance;
		std::cout<<provenance;
	}

	//Setup the Geometry
	InputCheck inputCheck;
	InputNgType::Writeable ioWriteable(filename,inputCheck);
	InputNgType::Readable io(ioWriteable);
	GeometryType geometry(io);

	//! Read the parameters for this run
	//ParametersModelType mp(io);
	DmrgSolverParametersType dmrgSolverParams(io);


	bool su2=false;
	if (dmrgSolverParams.options.find("useSu2Symmetry")!=std::string::npos) su2=true;
	std::string targetting="GroundStateTargetting";
	const char *targets[]={"TimeStepTargetting","DynamicTargetting","AdaptiveDynamicTargetting",
                     "CorrectionVectorTargetting","CorrectionTargetting","MettsTargetting"};
	size_t totalTargets = 6;
	for (size_t i = 0;i<totalTargets;++i)
		if (dmrgSolverParams.options.find(targets[i])!=std::string::npos) targetting=targets[i];
	if (targetting!="GroundStateTargetting" && su2) throw std::runtime_error("SU(2)"
 		" supports only GroundStateTargetting for now (sorry!)\n");
	
	if (su2) {
		if (dmrgSolverParams.targetQuantumNumbers[2]>0) { 
			mainLoop<ModelHelperSu2,VectorWithOffsets,GroundStateTargetting,MySparseMatrixReal>
			(geometry,targetting,concurrency,io,dmrgSolverParams,options);
		} else {
			mainLoop<ModelHelperSu2,VectorWithOffset,GroundStateTargetting,MySparseMatrixReal>
			(geometry,targetting,concurrency,io,dmrgSolverParams,options);
		}
		return 0;
	}
	if (targetting=="TimeStepTargetting") { 
		mainLoop<ModelHelperLocal,VectorWithOffsets,TimeStepTargetting,MySparseMatrixComplex>
		(geometry,targetting,concurrency,io,dmrgSolverParams,options);
		return 0;
	}
	if (targetting=="DynamicTargetting") {
		mainLoop<ModelHelperLocal,VectorWithOffsets,DynamicTargetting,MySparseMatrixReal>
		(geometry,targetting,concurrency,io,dmrgSolverParams,options);
		return 0;
	}
	if (targetting=="AdaptiveDynamicTargetting") {
		mainLoop<ModelHelperLocal,VectorWithOffsets,AdaptiveDynamicTargetting,MySparseMatrixReal>
		(geometry,targetting,concurrency,io,dmrgSolverParams,options);
		return 0;
	}
	if (targetting=="CorrectionTargetting") {
		mainLoop<ModelHelperLocal,VectorWithOffsets,CorrectionTargetting,MySparseMatrixReal>
		(geometry,targetting,concurrency,io,dmrgSolverParams,options);
		return 0;
	}
	if (targetting=="MettsTargetting") { // experimental, do not use
		mainLoop<ModelHelperLocal,VectorWithOffset,MettsTargetting,MySparseMatrixReal>
		(geometry,targetting,concurrency,io,dmrgSolverParams,options);
		return 0;
	}
	mainLoop<ModelHelperLocal,VectorWithOffset,GroundStateTargetting,MySparseMatrixReal>
	(geometry,targetting,concurrency,io,dmrgSolverParams,options);
} // main

