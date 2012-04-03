/**
 * @file SAT2FO.hpp
 * Defines class SAT2FO.
 */

#ifndef __SAT2FO__
#define __SAT2FO__

#include "Forwards.hpp"

#include "Lib/Numbering.hpp"

#include "Kernel/Inference.hpp"

namespace SAT {

using namespace Lib;
using namespace Kernel;

/**
 * Performs conversion between SAT and first-order literals
 * assuming it is a one-to-one relation
 *
 * Also has other utility functions related to the correspondence
 * between SAT and first-order literals
 */
class SAT2FO {
public:
  SATLiteral toSAT(Literal* l);
  SATClause* toSAT(Clause* cl);
  Literal* toFO(SATLiteral sl) const;

  unsigned createSpareSatVar();

  void collectAssignment(SATSolver& solver, LiteralStack& res) const;
  SATClause* createConflictClause(LiteralStack& unsatCore, Inference::Rule rule=Inference::THEORY);

  unsigned maxSATVar() const { return _posMap.getNumberUpperBound(); }
private:
  typedef Numbering<Literal *, 1 > TwoWayMap;
  TwoWayMap _posMap;
};

}

#endif // __SAT2FO__