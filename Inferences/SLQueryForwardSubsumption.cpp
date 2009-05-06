/**
 * @file SLQueryForwardSubsumption.cpp
 * Implements class SLQueryForwardSubsumption.
 */

#include "../Lib/VirtualIterator.hpp"
#include "../Lib/BacktrackData.hpp"
#include "../Lib/SkipList.hpp"
#include "../Lib/DArray.hpp"
#include "../Lib/List.hpp"
#include "../Lib/DHMap.hpp"
#include "../Lib/DHMultiset.hpp"
#include "../Lib/Comparison.hpp"

#include "../Kernel/Term.hpp"
#include "../Kernel/Clause.hpp"
#include "../Kernel/MLMatcher.hpp"

#include "../Indexing/Index.hpp"
#include "../Indexing/LiteralIndex.hpp"
#include "../Indexing/IndexManager.hpp"

#include "../Saturation/SaturationAlgorithm.hpp"

#include "../Lib/Environment.hpp"
#include "../Shell/Statistics.hpp"

#include "SLQueryForwardSubsumption.hpp"

using namespace Lib;
using namespace Kernel;
using namespace Indexing;
using namespace Saturation;
using namespace Inferences;

void SLQueryForwardSubsumption::attach(SaturationAlgorithm* salg)
{
  CALL("SLQueryForwardSubsumption::attach");
  ForwardSimplificationEngine::attach(salg);
  _index=static_cast<SimplifyingLiteralIndex*>(
	  _salg->getIndexManager()->request(SIMPLIFYING_SUBST_TREE) );
}

void SLQueryForwardSubsumption::detach()
{
  CALL("SLQueryForwardSubsumption::detach");
  _index=0;
  _salg->getIndexManager()->release(SIMPLIFYING_SUBST_TREE);
  ForwardSimplificationEngine::detach();
}


struct MatchInfo {
  MatchInfo() {}
  MatchInfo(Literal* cLit, Literal* qLit)
  : clauseLiteral(cLit), queryLiteral(qLit) {}
  Literal* clauseLiteral;
  Literal* queryLiteral;

};

typedef List<MatchInfo> MIList;

struct ClauseMatches {
  ClauseMatches() : matches(0), matchCount(0) {}

  MIList* matches;
  unsigned matchCount;
};


struct PtrHash {
  static unsigned hash(void* ptr) {
    return static_cast<unsigned>(reinterpret_cast<size_t>(ptr)>>4);
  }
};
struct PtrHash2 {
  static unsigned hash(void* ptr) {
    return static_cast<unsigned>(reinterpret_cast<size_t>(ptr)>>3);
  }
};

//typedef SkipList<ClauseMatches,ClauseMatches> CMSkipList;
typedef DHMap<Clause*,ClauseMatches, PtrHash, PtrHash2> CMMap;
typedef DHMap<Literal*, LiteralList*, PtrHash > MatchMap;

void SLQueryForwardSubsumption::perform(Clause* cl, bool& keep, ClauseIterator& toAdd,
	ClauseIterator& premises)
{
  CALL("SLQueryForwardSubsumption::perform");
  toAdd=ClauseIterator::getEmpty();
  premises=ClauseIterator::getEmpty();

  unsigned clen=cl->length();
  if(clen==0) {
    keep=true;
    return;
  }

  CMMap* gens=0;

  for(unsigned li=0;li<clen;li++) {
    SLQueryResultIterator rit=_index->getGeneralizations( (*cl)[li], false, false);
    while(rit.hasNext()) {
      SLQueryResult res=rit.next();
      unsigned rlen=res.clause->length();
      if(rlen==1) {
	keep=false;
	premises=pvi( getSingletonIterator(res.clause) );
	goto fin;
      } else if(rlen>clen) {
	continue;
      }

      if(!gens) {
	gens=new CMMap();
      }

      ClauseMatches* cms;
      if(gens->getValuePtr(res.clause, cms)) {
	ASS(cms->matches==0);
      }
      MIList::push(MatchInfo(res.literal, (*cl)[li]), cms->matches);
      cms->matchCount++;
    }
  }

  if(gens)
  {
    static DArray<LiteralList*> matches(32);
    static MatchMap matchMap;

    CMMap::Iterator git(*gens);
    while(git.hasNext()) {
      Clause* mcl;
      ClauseMatches clmatches;
      git.next(mcl, clmatches);

      unsigned mlen=mcl->length();
      if(mlen>clmatches.matchCount) {
	continue;
      }

      matchMap.reset();
      MIList::Iterator miit(clmatches.matches);
      while(miit.hasNext()) {
	MatchInfo minfo=miit.next();
	LiteralList** alts; //pointer to list of possibly matching literals of cl
	matchMap.getValuePtr(minfo.clauseLiteral, alts, 0);
	LiteralList::push(minfo.queryLiteral, *alts);
      };

      matches.ensure(mlen);
      bool mclMatchFailed=false;
      for(unsigned li=0;li<mlen;li++) {
	LiteralList* alts;
	if(matchMap.find( (*mcl)[li], alts) ) {
	  ASS(alts);
	  matches[li]=alts;
	} else {
	  matches[li]=0;
	  mclMatchFailed=true;
	}
      }

      if(!mclMatchFailed) {
	if(!MLMatcher::canBeMatched(mcl,matches)) {
	  mclMatchFailed=true;
	}
      }

      for(unsigned li=0;li<mlen;li++) {
	matches[li]->destroy();
      }

      if(!mclMatchFailed) {
	keep=false;
	premises=pvi( getSingletonIterator(mcl) );
	goto fin;
      }

    }
    keep=true;
  }
fin:
  if(!keep) {
    env.statistics->forwardSubsumed++;
  }
  if(gens) {
    CMMap::Iterator git(*gens);
    while(git.hasNext()) {
      git.next().matches->destroy();
    }
    delete gens;
  }
}

