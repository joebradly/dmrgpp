/*
Copyright (c) 2012, UT-Battelle, LLC
All rights reserved

[DMRG++, Version 2.0.0]
[by G.A., Oak Ridge National Laboratory]

UT Battelle Open Source Software License 11242008

OPEN SOURCE LICENSE

Subject to the conditions of this License, each
contributor to this software hereby grants, free of
charge, to any person obtaining a copy of this software
and associated documentation files (the "Software"), a
perpetual, worldwide, non-exclusive, no-charge,
royalty-free, irrevocable copyright license to use, copy,
modify, merge, publish, distribute, and/or sublicense
copies of the Software.

1. Redistributions of Software must retain the above
copyright and license notices, this list of conditions,
and the following disclaimer.  Changes or modifications
to, or derivative works of, the Software should be noted
with comments and the contributor and organization's
name.

2. Neither the names of UT-Battelle, LLC or the
Department of Energy nor the names of the Software
contributors may be used to endorse or promote products
derived from this software without specific prior written
permission of UT-Battelle.

3. The software and the end-user documentation included
with the redistribution, with or without modification,
must include the following acknowledgment:

"This product includes software produced by UT-Battelle,
LLC under Contract No. DE-AC05-00OR22725  with the
Department of Energy."

*********************************************************
DISCLAIMER

THE SOFTWARE IS SUPPLIED BY THE COPYRIGHT HOLDERS AND
CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
COPYRIGHT OWNER, CONTRIBUTORS, UNITED STATES GOVERNMENT,
OR THE UNITED STATES DEPARTMENT OF ENERGY BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
DAMAGE.

NEITHER THE UNITED STATES GOVERNMENT, NOR THE UNITED
STATES DEPARTMENT OF ENERGY, NOR THE COPYRIGHT OWNER, NOR
ANY OF THEIR EMPLOYEES, REPRESENTS THAT THE USE OF ANY
INFORMATION, DATA, APPARATUS, PRODUCT, OR PROCESS
DISCLOSED WOULD NOT INFRINGE PRIVATELY OWNED RIGHTS.

*********************************************************


*/
// END LICENSE BLOCK
/** \ingroup DMRG */
/*@{*/

/*! \file InitKron.h
 *
 *
 */

#ifndef INIT_KRON_HEADER_H
#define INIT_KRON_HEADER_H

#include "GenIjPatch.h"
#include "ArrayOfMatStruct.h"

namespace Dmrg {

template<typename ModelType,typename ModelHelperType_>
class InitKron {

public:

	typedef ModelHelperType_ ModelHelperType;
	typedef typename ModelHelperType::RealType RealType;
	typedef typename ModelHelperType::SparseMatrixType SparseMatrixType;
	typedef typename SparseMatrixType::value_type ComplexOrRealType;
	typedef typename ModelHelperType::LeftRightSuperType LeftRightSuperType;
	typedef GenIjPatch<LeftRightSuperType> GenIjPatchType;
	typedef typename GenIjPatchType::GenGroupType GenGroupType;
	typedef ArrayOfMatStruct<SparseMatrixType,GenGroupType> ArrayOfMatStructType;
	typedef typename ModelHelperType::LinkType LinkType;
	typedef typename ModelType::LinkProductStructType LinkProductStructType;

	// export ParallelTemplate
	template<typename T2>
	class ParallelConnectionsInner {
	public:
		typedef typename ModelType::template ParallelConnectionsInner<T2> InnerType;
		typedef typename InnerType::Type Type;
	};

	InitKron(const ModelType& model,const ModelHelperType& modelHelper)
	: model_(model),
	  modelHelper_(modelHelper),
	  gengroupLeft_(modelHelper_.leftRightSuper().left()),
	  gengroupRight_(modelHelper_.leftRightSuper().right()),
	  ijpatches_(modelHelper_.leftRightSuper(),modelHelper_.quantumNumber()),
	  aL_(modelHelper_.leftRightSuper().left().hamiltonian(),gengroupLeft_),
	  aRt_(0)
	{
		SparseMatrixType arTranspose;
		transposeConjugate(arTranspose,modelHelper_.leftRightSuper().right().hamiltonian());

//		std::cerr<<"gengroupLeft_.size="<<gengroupLeft_.size()<<"\n";
//		std::cerr<<"gengroupRight_.size="<<gengroupRight_.size()<<"\n";

		aRt_ = new ArrayOfMatStructType(arTranspose,gengroupRight_);

		convertXcYcArrays();
//		printFullMatrix(modelHelper_.leftRightSuper().left().hamiltonian(),"LEFT HAM");
//		printFullMatrix(modelHelper_.leftRightSuper().right().hamiltonian(),"RIGHT HAM");
	}

	~InitKron()
	{
		if (aRt_) delete aRt_;

		for (size_t ic=0;ic<xc_.size();ic++) delete xc_[ic];
		for (size_t ic=0;ic<yc_.size();ic++) delete yc_[ic];

	}

	size_t numberOfThreads() const
	{
		return model_.params().nthreads;
	}

	const ConcurrencyType& concurrency() const { return model_.concurrency(); }

	const ArrayOfMatStructType& xc(size_t ic) const
	{
		return *xc_[ic];
	}

	const ArrayOfMatStructType& yc(size_t ic) const
	{
		return *yc_[ic];
	}

	size_t patch(typename GenIjPatchType::LeftOrRightEnumType i,size_t j) const
	{
		return ijpatches_(i,j);
	}

	size_t patch() const {return ijpatches_.size(); }

	const LeftRightSuperType& lrs() const
	{
		return modelHelper_.leftRightSuper();
	}

	size_t offset() const
	{
		size_t m = modelHelper_.m();
		return modelHelper_.leftRightSuper().super().partition(m);
	}

	size_t size() const { return modelHelper_.size(); }

	size_t connections() const { return xc_.size(); }

	const ArrayOfMatStructType& aRt () const
	{
		return *aRt_;
	}

	const ArrayOfMatStructType& aL() const
	{
		return aL_;
	}

	const GenGroupType& istartLeft() const
	{
		return gengroupLeft_;
	}

	const GenGroupType& istartRight() const
	{
		return gengroupRight_;
	}

	const ComplexOrRealType& value(size_t i) const
	{
		assert(values_.size()>i);
		return values_[i];
	}

private:

	void convertXcYcArrays()
	{
		LinkProductStructType* lps = 0;
		size_t total = model_.getLinkProductStruct(&lps,modelHelper_);

		for (size_t ix=0;ix<total;ix++) {
			SparseMatrixType const* A = 0;
			SparseMatrixType const* B = 0;

			LinkType link2 = model_.getConnection(&A,&B,ix,*lps,modelHelper_);
//			assert(link2.fermionOrBoson==ProgramGlobals::BOSON);
			if (link2.type==ProgramGlobals::ENVIRON_SYSTEM)  {
				LinkType link3 = link2;
				//link3.value *= fermionSign;
				link3.type = ProgramGlobals::SYSTEM_ENVIRON;
				addOneConnection(*B,*A,link3);
				continue;
			}
			addOneConnection(*A,*B,link2);
		}
//		std::cerr<<"Toooooootttttttttaaaaaaaaaaaaaallllllll="<<total<<"\n";

		if (lps) delete lps;
	}

	void addOneConnection(const SparseMatrixType& A,const SparseMatrixType& B,const LinkType& link2)
	{
		values_.push_back(link2.value);
//			assert(std::norm(tmp-0.5)<1e-6);
		ArrayOfMatStructType* x1 = new ArrayOfMatStructType(A,gengroupLeft_);
		xc_.push_back(x1);

		SparseMatrixType tmpMatrix;
		transposeConjugate(tmpMatrix,B);
		ArrayOfMatStructType* y1 = new ArrayOfMatStructType(tmpMatrix,gengroupRight_);
		yc_.push_back(y1);
	}


	InitKron(const InitKron& other);

	InitKron& operator=(const InitKron& other);

	const ModelType& model_;
	const ModelHelperType& modelHelper_;
//	QvalStructType qvalStruct_;
	GenGroupType gengroupLeft_,gengroupRight_;
	GenIjPatchType  ijpatches_;
	ArrayOfMatStructType aL_;
	ArrayOfMatStructType* aRt_; // <-- we own it also, it's newed and deleted here
	std::vector<ArrayOfMatStructType*> xc_;
	std::vector<ArrayOfMatStructType*> yc_;
	std::vector<ComplexOrRealType> values_;

}; //class InitKron
} // namespace PsimagLite

/*@}*/

#endif // INIT_KRON_HEADER_H
