/**
 * @file CTFwSubsAndRes.cpp
 * Implements class CTFwSubsAndRes.
 */


#include "Kernel/Inference.hpp"
#include "Kernel/ColorHelper.hpp"

#include "Indexing/Index.hpp"
#include "Indexing/IndexManager.hpp"

#include "Saturation/SaturationAlgorithm.hpp"

#include "Lib/Environment.hpp"
#include "Shell/Statistics.hpp"


#include "CTFwSubsAndRes.hpp"

namespace Inferences
{
using namespace Lib;
using namespace Kernel;
using namespace Indexing;
using namespace Saturation;

void CTFwSubsAndRes::attach(SaturationAlgorithm* salg)
{
  CALL("CTFwSubsAndRes::attach");

  ForwardSimplificationEngine::attach(salg);
  _index=static_cast<ClauseSubsumptionIndex*>(
	  _salg->getIndexManager()->request(FW_SUBSUMPTION_CODE_TREE) );
}

void CTFwSubsAndRes::detach()
{
  CALL("CTFwSubsAndRes::detach");

  _index=0;
  _salg->getIndexManager()->release(FW_SUBSUMPTION_CODE_TREE);
  ForwardSimplificationEngine::detach();
}

Clause* CTFwSubsAndRes::buildSResClause(Clause* cl, unsigned resolvedIndex, Clause* premise)
{
  CALL("CTFwSubsAndRes::buildSResClause");
  
  unsigned clen = cl->length();
  ASS_L(resolvedIndex,clen);

  unsigned newLength = clen-1;

  Inference* inf = new Inference2(Inference::SUBSUMPTION_RESOLUTION, cl, premise);
  Unit::InputType inpType = (Unit::InputType)
  	max(cl->inputType(), premise->inputType());

  Clause* res = new(newLength) Clause(newLength, inpType, inf);

  unsigned next = 0;
  for(unsigned i=0;i<clen;i++) {
    if(i!=resolvedIndex) {
	(*res)[next++] = (*cl)[i];
    }
  }
  ASS_EQ(next,newLength);

  res->setAge(cl->age());

  return res;
}

void CTFwSubsAndRes::perform(Clause* cl, ForwardSimplificationPerformer* simplPerformer)
{
  CALL("CTFwSubsAndRes::perform");
  
  if(cl->length()==0) {
    return;
  }

  TimeCounter tc_fs(TC_FORWARD_SUBSUMPTION);

  Clause::requestAux();

  ClauseSResResultIterator sresIt=_index->getSubsumingOrSResolvingClauses(cl, 
	  _subsumptionResolution);
  while(sresIt.hasNext()) {
    ClauseSResQueryResult res=sresIt.next();
    Clause* premise=res.clause;
    if(premise->hasAux()) {
      //we already yielded this clause as a potential subsumer
      continue;
    }
    premise->setAux(0);
    if(!ColorHelper::compatible(cl->color(), premise->color())) {
      continue;
    }
    
    if(res.resolved) {
      Clause* replacement=buildSResClause(cl, res.resolvedQueryLiteralIndex, premise);
      simplPerformer->perform(premise, replacement);
      env.statistics->forwardSubsumptionResolution++;
    }
    else {
      simplPerformer->perform(premise, 0);
      env.statistics->forwardSubsumed++;
    }
    
    if(!simplPerformer->clauseKept()) {
      goto fin;
    }
  }

fin:
  Clause::releaseAux();
}


}
