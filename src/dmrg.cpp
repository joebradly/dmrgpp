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

typedef double MatrixElementType;

#include "CrsMatrix.h"
#include "LanczosSolver.h"
#include "ChebyshevSolver.h"
#include "BlockMatrix.h"
#include "DmrgSolver.h"
#include "IoSimple.h"
#include "Operator.h"
#ifndef USE_MPI
#include "ConcurrencySerial.h"
typedef PsimagLite::ConcurrencySerial<MatrixElementType> ConcurrencyType;
#else
#include "ConcurrencyMpi.h"
typedef PsimagLite::ConcurrencyMpi<MatrixElementType> ConcurrencyType;
#endif
#include "ModelFactory.h"
#include "OperatorsBase.h"
#include "Geometry.h"
#include "ModelHelperLocal.h"
#include "ModelHelperSu2.h"
#include "InternalProductOnTheFly.h"
#include "InternalProductStored.h"
#include "InternalProductKron.h"
//#include "InternalProductCached.h"
#include "GroundStateTargetting.h"
#include "TimeStepTargetting.h"
#include "DynamicTargetting.h"
#include "AdaptiveDynamicTargetting.h"
#include "CorrectionTargetting.h"
#include "CorrectionVectorTargetting.h"
#include "MettsTargetting.h" // experimental
#include "VectorWithOffset.h"
#include "VectorWithOffsets.h"
#include "BasisWithOperators.h"
#include "LeftRightSuper.h"
#include "Provenance.h"

typedef std::complex<MatrixElementType> ComplexType;
typedef  PsimagLite::CrsMatrix<ComplexType> MySparseMatrixComplex;
typedef  PsimagLite::CrsMatrix<MatrixElementType> MySparseMatrixReal;

using namespace Dmrg;

typedef PsimagLite::Geometry<MatrixElementType,ProgramGlobals> GeometryType;
typedef PsimagLite::InputNg<InputCheck> InputNgType;
typedef ParametersDmrgSolver<MatrixElementType,InputNgType::Readable> ParametersDmrgSolverType;

template<typename ModelFactoryType,
	 template<typename,typename> class InternalProductTemplate,
         typename TargettingType,
         typename MySparseMatrix>
void mainLoop3(GeometryType& geometry,
              ParametersDmrgSolverType& dmrgSolverParams,
              ConcurrencyType& concurrency,
	      InputNgType::Readable& io)
{
	//! Setup the Model
	ModelFactoryType model(dmrgSolverParams,io,geometry,concurrency);

	//! Read TimeEvolution if applicable:
	typedef typename TargettingType::TargettingParamsType TargettingParamsType;
	TargettingParamsType tsp(io,model);

	//! Setup the dmrg solver:
	typedef DmrgSolver<InternalProductTemplate,TargettingType> SolverType;
	SolverType dmrgSolver(dmrgSolverParams,model,concurrency,tsp);

	//! Calculate observables:
	dmrgSolver.main(geometry);
}

template<template<typename,typename> class ModelHelperTemplate,
	 template<typename,typename> class InternalProductTemplate,
         template<typename> class VectorWithOffsetTemplate,
         template<template<typename,typename,typename> class,
                  template<typename,typename> class,
                  template<typename,typename> class,
                  typename,typename,typename,
                  template<typename> class> class TargettingTemplate,
         typename MySparseMatrix>
void mainLoop2(GeometryType& geometry,
              ParametersDmrgSolverType& dmrgSolverParams,
              ConcurrencyType& concurrency,
	      InputNgType::Readable& io)
{
	typedef Operator<MatrixElementType,MySparseMatrix> OperatorType;
	typedef Basis<MatrixElementType,MySparseMatrix> BasisType;
	typedef OperatorsBase<OperatorType,BasisType> OperatorsType;
	typedef BasisWithOperators<OperatorsType,ConcurrencyType> BasisWithOperatorsType;
	typedef LeftRightSuper<BasisWithOperatorsType,BasisType> LeftRightSuperType;
	typedef ModelHelperTemplate<LeftRightSuperType,ConcurrencyType> ModelHelperType;
	typedef ModelFactory<ModelHelperType,MySparseMatrix,GeometryType,
			     PTHREADS_NAME,ParametersDmrgSolverType> ModelFactoryType;

	if (dmrgSolverParams.options.find("ChebyshevSolver")!=std::string::npos) {
		typedef TargettingTemplate<PsimagLite::ChebyshevSolver,
					   InternalProductTemplate,
					   WaveFunctionTransfFactory,
					   ModelFactoryType,
					   ConcurrencyType,
					   PsimagLite::IoSimple,
					   VectorWithOffsetTemplate
					   > TargettingType;
		mainLoop3<ModelFactoryType, InternalProductTemplate,TargettingType,MySparseMatrix>
		(geometry,dmrgSolverParams,concurrency,io);
	} else {
		typedef TargettingTemplate<PsimagLite::LanczosSolver,
					   InternalProductTemplate,
					   WaveFunctionTransfFactory,
					   ModelFactoryType,
					   ConcurrencyType,
					   PsimagLite::IoSimple,
					   VectorWithOffsetTemplate
					   > TargettingType;
		mainLoop3<ModelFactoryType,InternalProductTemplate,TargettingType,MySparseMatrix>
		(geometry,dmrgSolverParams,concurrency,io);
	}
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
              ParametersDmrgSolverType& dmrgSolverParams,
              ConcurrencyType& concurrency,
	      InputNgType::Readable& io)
{
	if (dmrgSolverParams.options.find("InternalProductStored")!=std::string::npos) {
		mainLoop2<ModelHelperTemplate,
		         InternalProductStored,
		         VectorWithOffsetTemplate,
		         TargettingTemplate,
		         MySparseMatrix>(geometry,dmrgSolverParams,concurrency,io);
	} else if (dmrgSolverParams.options.find("InternalProductKron")!=std::string::npos) {
		mainLoop2<ModelHelperTemplate,
			 InternalProductKron,
			 VectorWithOffsetTemplate,
			 TargettingTemplate,
			 MySparseMatrix>(geometry,dmrgSolverParams,concurrency,io);
	} else {
 		mainLoop2<ModelHelperTemplate,
		         InternalProductOnTheFly,
		         VectorWithOffsetTemplate,
		         TargettingTemplate,
		         MySparseMatrix>(geometry,dmrgSolverParams,concurrency,io);
	}
}

int main(int argc,char *argv[])
{
	InputCheck inputCheck;
	std::string filename="";
	int opt = 0;
	std::string strUsage(argv[0]);
	strUsage += " -f filename";
	std::string insitu("");
	while ((opt = getopt(argc, argv,"f:o:")) != -1) {
		switch (opt) {
		case 'f':
			filename = optarg;
			break;
		case 'o':
			if (insitu=="") {
				insitu = optarg;
			} else {
				insitu += ",";
				insitu += optarg;
			}
			break;
		default:
			inputCheck.usageMain(strUsage);
			return 1;
		}
	}

	// sanity checks here
	if (filename=="") {
		inputCheck.usageMain(strUsage);
		return 1;
	}

	//! setup distributed parallelization
	ConcurrencyType concurrency(argc,argv);

	// print license
	if (concurrency.root()) {
		std::cerr<<license;
		Provenance provenance;
		std::cout<<provenance;
	}

	InputNgType::Writeable ioWriteable(filename,inputCheck);
	InputNgType::Readable io(ioWriteable);

	GeometryType geometry(io);

	ParametersDmrgSolver<MatrixElementType,InputNgType::Readable> dmrgSolverParams(io);

	if (insitu!="") dmrgSolverParams.insitu = insitu;

#ifndef USE_PTHREADS
	inputCheck.checkForThreads(dmrgSolverParams.nthreads);
#endif

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
			mainLoop<ModelHelperSu2,VectorWithOffsets,GroundStateTargetting,
				MySparseMatrixReal>(geometry,dmrgSolverParams,concurrency,io);
		} else {
			mainLoop<ModelHelperSu2,VectorWithOffset,GroundStateTargetting,
				MySparseMatrixReal>(geometry,dmrgSolverParams,concurrency,io);
		}
		return 0;
	}
	if (targetting=="TimeStepTargetting") { 
		mainLoop<ModelHelperLocal,VectorWithOffsets,TimeStepTargetting,
			MySparseMatrixComplex>(geometry,dmrgSolverParams,concurrency,io);
			return 0;
	}
	if (targetting=="DynamicTargetting") {
		mainLoop<ModelHelperLocal,VectorWithOffsets,DynamicTargetting,
			MySparseMatrixReal>(geometry,dmrgSolverParams,concurrency,io);
			return 0;
	}
	if (targetting=="AdaptiveDynamicTargetting") {
		mainLoop<ModelHelperLocal,VectorWithOffsets,AdaptiveDynamicTargetting,
			MySparseMatrixReal>(geometry,dmrgSolverParams,concurrency,io);
			return 0;
	}
	if (targetting=="CorrectionVectorTargetting") {
		mainLoop<ModelHelperLocal,VectorWithOffsets,CorrectionVectorTargetting,
			MySparseMatrixReal>(geometry,dmrgSolverParams,concurrency,io);
			return 0;
	}
	if (targetting=="CorrectionTargetting") {
		mainLoop<ModelHelperLocal,VectorWithOffsets,CorrectionTargetting,
			MySparseMatrixReal>(geometry,dmrgSolverParams,concurrency,io);
			return 0;
	}
	if (targetting=="MettsTargetting") {
		mainLoop<ModelHelperLocal,VectorWithOffsets,MettsTargetting,
			MySparseMatrixReal>(geometry,dmrgSolverParams,concurrency,io);
			return 0;
	}
	mainLoop<ModelHelperLocal,VectorWithOffset,GroundStateTargetting,
		MySparseMatrixReal>(geometry,dmrgSolverParams,concurrency,io);
}

