/**
 * @file Property.cpp (syntactic properties of problems)
 *
 * @since 06/06/2001 Manchester
 * @author Andrei Voronkov
 * @since 17/07/2003 Manchester, changed to new representation
 */

#include "Debug/Tracer.hpp"

#include "Lib/Int.hpp"
#include "Lib/Environment.hpp"

#include "Kernel/Clause.hpp"
#include "Kernel/FormulaUnit.hpp"
#include "Kernel/SortHelper.hpp"
#include "Kernel/SubformulaIterator.hpp"
#include "Kernel/Term.hpp"
#include "Kernel/Signature.hpp"

#include "Statistics.hpp"
#include "FunctionDefinition.hpp"
#include "Property.hpp"

using namespace Lib;
using namespace Kernel;
using namespace Shell;

/**
 * Initialize Property. Must be applied to the preprocessed problem.
 *
 * @since 29/06/2002, Manchester
 */
Property::Property()
  : _goalClauses(0),
    _axiomClauses(0),
    _positiveEqualityAtoms(0),
    _equalityAtoms(0),
    _atoms(0),
    _goalFormulas(0),
    _axiomFormulas(0),
    _subformulas(0),
    _terms(0),
    _unitGoals(0),
    _unitAxioms(0),
    _hornGoals(0),
    _hornAxioms(0),
    _equationalClauses(0),
    _pureEquationalClauses(0),
    _groundUnitAxioms(0),
    _positiveAxioms(0),
    _groundPositiveAxioms(0),
    _groundGoals(0),
    _maxFunArity(0),
    _maxPredArity(0),
    _totalNumberOfVariables(0),
    _maxVariablesInClause(0),
    _props(0),
    _hasInterpreted(false),
    _hasNonDefaultSorts(false),
    _hasSpecialTermsOrLets(false),
    _hasFormulaItes(false)
{
  _interpretationPresence.init(Theory::MAX_INTERPRETED_ELEMENT+1, false);
  env.property = this;
} // Property::Property

/**
 * Create a new property, scan the units with it and return the property.
 * @since 22/07/2011 Manchester
 */
Property* Property::scan(UnitList* units)
{
  CALL("Property::scan");

  Property* prop = new Property;
  prop->add(units);
  return prop;
} // Property::scan

/**
 * Destroy the property. If this property is used as env.property, set env.property to null.
 * @since 22/07/2011 Manchester
 */
Property::~Property()
{
  CALL("Property::~Property");
  if (this == env.property) {
    env.property = 0;
  }
}

/**
 * Add units and modify an existing property.
 * @since 29/06/2002 Manchester
 */
void Property::add(UnitList* units)
{
  CALL("Property::add(UnitList*)");

  UnitList::Iterator us(units);
  while (us.hasNext()) {
    scan(us.next());
  }

  // information about sorts is read from the environment, not from the problem
  if (env.sorts->hasSort()) {
    addProp(PR_SORTS);
  }
    
  // information about interpreted constant is read from the signature
  if (env.signature->strings()) {
    addProp(PR_HAS_STRINGS);
  }
  if (env.signature->integers()) {
    addProp(PR_HAS_INTEGERS);
  }
  if (env.signature->rationals()) {
    addProp(PR_HAS_RATS);
  }
  if (env.signature->reals()) {
    addProp(PR_HAS_REALS);
  }


  // determine the category after adding
  if (formulas() > 0) { // FOF, either FEQ or FNE
    if (_equalityAtoms == 0) {
      _category = FNE;
    }
    else {
      _category = FEQ;
    }
  }
  // no formulas in the input, one of NEQ, HEQ, PEQ, HNE, NNE, EPR, UEQ
  else if (_maxFunArity == 0) { // all function symbols are constants
    if (_pureEquationalClauses == clauses()) { // only equations, UEQ or PEQ
      if (clauses() == unitClauses()) { // all clauses are unit
	_category = UEQ;
      }
      else {
	_category = PEQ;
      }
    }
    else {
      _category = EPR;
    }
  }
  // one of NEQ, HEQ, PEQ, HNE, NNE, UEQ
  else if (_equationalClauses == 0 ) { // HNE, NNE
    if (clauses() == hornClauses()) { // all clauses are horn
      _category = HNE;
    }
    else {
      _category = NNE;
    }
  }
  // one of NEQ, HEQ, PEQ, UEQ
  else if (_pureEquationalClauses == clauses()) { // only equations, UEQ or PEQ
    if (clauses() == unitClauses()) { // all clauses are unit
      _category = UEQ;
    }
    else {
      _category = PEQ;
    }
  }
  // one of NEQ, HEQ
  else if (clauses() == hornClauses()) { // all clauses are horn
    _category = HEQ;
  }
  else {
    _category = NEQ;
  }
} // Property::add(const UnitList* units)

/**
 * Scan property from a unit.
 *
 * @since 29/06/2002 Manchester
 * @since 17/07/2003 Manchester, changed to non-pointer types,
 *        formula scanning added
 * @since 26/05/2007 Manchester, changed to use new datastructures
 */
void Property::scan(Unit* unit)
{
  CALL("Property::scan(const Unit*)");

  if (unit->isClause()) {
    scan(static_cast<Clause*>(unit));
  }
  else {
    scan(static_cast<FormulaUnit*>(unit));
  }
  if (! hasProp(PR_HAS_FUNCTION_DEFINITIONS)) {
    FunctionDefinition::Def* def =
      FunctionDefinition::isFunctionDefinition(*unit);
    if (def) {
      addProp(PR_HAS_FUNCTION_DEFINITIONS);
      FunctionDefinition::deleteDef(def);
    }
  }
} // Property::scan(const Unit* unit)

/**
 * Scan a clause.
 *
 * @param clause the clause
 * @since 29/06/2002 Manchester
 * @since 17/07/2003 Manchester, changed to non-pointer types
 * @since 27/08/2003 Vienna, changed to count variables
 * @since 26/05/2007 Manchester, changed to use new datastructures
 */
void Property::scan(Clause* clause)
{
  CALL("Property::scan(const Clause*)");

  int positiveLiterals = 0;
  int negativeLiterals = 0;
  int equationalLiterals = 0;
  int positiveEquationalLiterals = 0;
  int groundLiterals = 0;
  _variablesInThisClause = 0;

  for (int i = clause->length()-1;i >= 0;i--) {
    Literal* literal = (*clause)[i];
    if (literal->isPositive()) {
      positiveLiterals ++;
    }
    else {
      negativeLiterals ++;
    }

    if (literal->isEquality()) {
      equationalLiterals++;
      if (literal->isPositive()) {
	positiveEquationalLiterals++;
      }
    }

    scan(literal);

    if (literal->shared() && literal->ground()) {
      groundLiterals++;
    }
  }
  int literals = positiveLiterals + negativeLiterals;
  _atoms += literals;

  if ( equationalLiterals > 0 ) {
    _equationalClauses ++;
    _equalityAtoms += equationalLiterals;
    _positiveEqualityAtoms += positiveEquationalLiterals;
  }
  if ( literals == equationalLiterals ) {
    _pureEquationalClauses ++;
  }

  if (clause->inputType() == Unit::AXIOM) {
    _axiomClauses ++;
    if ( literals == 1) {
      _unitAxioms ++;
      if ( groundLiterals == 1) {
	_groundUnitAxioms ++;
      }
    }
    if (positiveLiterals <= 1) {
      _hornAxioms ++;
    }
    if (negativeLiterals == 0) {
      _positiveAxioms ++;
      if (literals == groundLiterals) {
	_groundPositiveAxioms ++;
      }
    }
  }
  else {
    _goalClauses ++;
    if ( literals == 1) {
      _unitGoals ++;
    }
    if (positiveLiterals <= 1) {
      _hornGoals ++;
    }
    if (literals == groundLiterals) {
      _groundGoals ++;
    }
  }

  _totalNumberOfVariables += _variablesInThisClause;
  if (_variablesInThisClause > _maxVariablesInClause) {
    _maxVariablesInClause = _variablesInThisClause;
  }
  if (! hasProp(PR_HAS_X_EQUALS_Y) && hasXEqualsY(clause)) {
    addProp(PR_HAS_X_EQUALS_Y);
  }
} // Property::scan (const Clause* clause, bool isAxiom)


/**
 * Scan a formula unit.
 * @since 27/05/2007 flight Manchester-Frankfurt
 * @since 15/01/2014 Manchester, changed to use new hasXEqualsY
 * @author Andrei Voronkov
 */
void Property::scan(FormulaUnit* unit)
{
  CALL("Property::scan(const FormulaUnit*)");

  if (unit->inputType() == Unit::AXIOM) {
    _axiomFormulas ++;
  }
  else {
    _goalFormulas++;
  }
  Formula* f = unit->formula();
  scan(f);
  if (! hasProp(PR_HAS_X_EQUALS_Y)) {
    if (hasXEqualsY(f)) {
      addProp(PR_HAS_X_EQUALS_Y);
    }
  }
} // Property::scan


/**
 * Scan a formula.
 *
 * @since 17/07/2003 Manchester
 * @since 11/12/2004 Manchester, true and false added
 */
void Property::scan(Formula* formula)
{
  CALL("void Property::scan(const Formula&)");

  SubformulaIterator fs(formula);
  while (fs.hasNext()) {
    _subformulas++;
    Formula* f = fs.next();
    switch(f->connective()) {
    case ITE:
      _hasFormulaItes = true;
      break;
    case FORMULA_LET:
    case TERM_LET:
      _hasSpecialTermsOrLets = true;
      break;
    case LITERAL:
    {
      _atoms++;
      Literal* lit = f->literal();
      if (lit->isEquality()) {
	_equalityAtoms++;
	if (lit->isPositive()) {
	  _positiveEqualityAtoms++;
	}
      }
      if (!lit->shared()) {
	_hasSpecialTermsOrLets = true;
      }
      scan(lit);
      break;
    }
    default:
      break;
    }
  }
} // Property::scan(const Formula&)

/**
 * If the sort is recognised by the properties, add information about it to the properties.
 * @since 04/05/2013 Manchester, array sorts removed
 * @author Andrei Voronkov
 */
void Property::scanSort(unsigned sort)
{
  CALL("Property::scanSort");

  if (sort==Sorts::SRT_DEFAULT) {
    return;
  }
  _hasNonDefaultSorts = true;
  env.statistics->hasTypes=true;
  switch(sort) {
  case Sorts::SRT_INTEGER:
    addProp(PR_HAS_INTEGERS);
    break;
  case Sorts::SRT_RATIONAL:
    addProp(PR_HAS_RATS);
    break;
  case Sorts::SRT_REAL:
    addProp(PR_HAS_REALS);
    break;          
  }
}

/**
 * Scan a literal.
 *
 * @param lit the literal
 * @since 29/06/2002 Manchester
 * @since 17/07/2003 Manchester, changed to non-pointer types
 * @since 27/05/2007 flight Manchester-Frankfurt, uses new datastructures
 */
void Property::scan(Literal* lit)
{
  CALL("Property::scan(const Literal*...)");

  if (lit->isEquality()) {
    scanSort(SortHelper::getEqualityArgumentSort(lit));
  }
  else {
    int arity = lit->arity();
    if (arity > _maxPredArity) {
      _maxPredArity = arity;
    }
    PredicateType* type = env.signature->getPredicate(lit->functor())->predType();
    for (int i=0; i<arity; i++) {
      scanSort(type->arg(i));
    }
  }

  scanForInterpreted(lit);
  scan(lit->args());

  if (!hasProp(PR_HAS_INEQUALITY_RESOLVABLE_WITH_DELETION) && lit->isEquality() && lit->shared()
     && lit->isNegative() && !lit->ground() &&
     ( ( lit->nthArgument(0)->isVar() &&
	 !lit->nthArgument(1)->containsSubterm(*lit->nthArgument(0)) ) ||
       ( lit->nthArgument(1)->isVar() &&
	 !lit->nthArgument(0)->containsSubterm(*lit->nthArgument(1)) ))) {
    addProp(PR_HAS_INEQUALITY_RESOLVABLE_WITH_DELETION);
  }
} // Property::scan(Literal* lit)


/**
 * Scan a term arguments.
 *
 * @param ts the list of terms
 * @since 29/06/2002 Manchester
 * @since 17/07/2003 Manchester, changed to non-pointer types,
 *        also NUMERIC case added
 * @since 27/08/2003 Vienna, changed to count variables
 * @since 27/05/2007 flight Manchester-Frankfurt, changed to new datastructures
 */
void Property::scan(TermList* ts)
{
  CALL("Property::scan(TermList*))");

  static Stack<TermList*> stack(64);
  stack.reset();

  for (;;) {
    if (ts->isEmpty()) {
      if (stack.isEmpty()) {
	return;
      }
      ts = stack.pop();
    }
    // ts is non-empty
    _terms ++;
    if (ts->isVar()) {
      _variablesInThisClause++;
    }
    else { // ts is a reference to a complex term
      Term* t = ts->term();
      if (t->isSpecial()) {
	scanSpecialTerm(t);
      }
      else {
	scanForInterpreted(t);

	int arity = t->arity();
	FunctionType* type = env.signature->getFunction(t->functor())->fnType();
	for (int i=0; i<arity; i++) {
	  scanSort(type->arg(i));
	}

	if (arity > _maxFunArity) {
	  _maxFunArity = arity;
	}
	if (arity) {
	  stack.push(t->args());
	}
      }
    }
    ts = ts->next();
  }
} // Property::scan(const Term& term, bool& isGround)

void Property::scanSpecialTerm(Term* t)
{
  CALL("Property::scanSpecialTerm");

  Term::SpecialTermData* sd = t->getSpecialData();
  switch(t->functor()) {
  case Term::SF_TERM_ITE:
  {
    ASS_EQ(t->arity(),2);
    scan(sd->getCondition());
    scan(t->args());
    break;
  }
  case Term::SF_LET_FORMULA_IN_TERM:
  {
    ASS_EQ(t->arity(),1);
    scan(sd->getLhsLiteral());
    scan(sd->getRhsFormula());
    scan(t->args());
    break;
  }
  case Term::SF_LET_TERM_IN_TERM:
  {
    ASS_EQ(t->arity(),1);
    //this is a trick creating an artificial term list with terms we want to traverse
    TermList aux[3];
    aux[0].makeEmpty();
    aux[1] = sd->getRhsTerm();
    aux[2] = sd->getLhsTerm();
    scan(aux+2);
    scan(t->args());
    break;
  }
  default:
    ASSERTION_VIOLATION;
  }
}

void Property::scanForInterpreted(Term* t)
{
  CALL("Property::scanInterpretation");

  Interpretation itp;
  if (t->isLiteral()) {
    Literal* lit = static_cast<Literal*>(t);
    if (!theory->isInterpretedPredicate(lit)) { return; }
    itp = theory->interpretPredicate(lit);
  }
  else {
    if (!theory->isInterpretedFunction(t)) { return; }
    itp = theory->interpretFunction(t);
  }
  _interpretationPresence[itp] = true;
  _hasInterpreted = true;
  unsigned sort = Theory::getOperationSort(itp);
  if(Theory::isInequality(itp)){
    switch(sort){
      case Sorts::SRT_INTEGER : addProp(PR_INTEGER_COMPARISON);
        break;
      case Sorts::SRT_RATIONAL : addProp(PR_RAT_COMPARISON);
        break;
      case Sorts::SRT_REAL : addProp(PR_REAL_COMPARISON);
        break;
    }
  }
  if(Theory::isLinearOperation(itp)){
    switch(sort){
      case Sorts::SRT_INTEGER : addProp(PR_INTEGER_LINEAR);
        break;
      case Sorts::SRT_RATIONAL : addProp(PR_RAT_LINEAR);
        break;
      case Sorts::SRT_REAL : addProp(PR_REAL_LINEAR);
        break;
    }
  }
  if(Theory::isNonLinearOperation(itp)){
    switch(sort){
      case Sorts::SRT_INTEGER : addProp(PR_INTEGER_NONLINEAR);
        break;
      case Sorts::SRT_RATIONAL : addProp(PR_RAT_NONLINEAR);
        break;
      case Sorts::SRT_REAL : addProp(PR_REAL_NONLINEAR);
        break;
    }
  }
}

/**
 * Return the string representation of the CASC category.
 */
vstring Property::categoryString() const
{
  CALL("vstring Property::categoryString() const");
  return categoryToString(_category);
}
vstring Property::categoryToString(Category cat)
{
  switch (cat)
    {
    case NEQ:
      return "NEQ";
    case HEQ:
      return "HEQ";
    case PEQ:
      return "PEQ";
    case HNE:
      return "HNE";
    case NNE:
      return "NNE";
    case FEQ:
      return "FEQ";
    case FNE:
      return "FNE";
    case EPR:
      return "EPR";
    case UEQ:
      return "UEQ";
#if VDEBUG
    default:
      ASSERTION_VIOLATION;
      return "";
#endif
    }
} // categoryString


/**
 * Output the property to a string readable by a human. NOT ALL FIELDS
 * ARE CURRENTLY OUTPUT.
 * @since 27/08/2003 Vienna
 */
vstring Property::toString() const
{
  vstring result("TPTP class: ");
  result += categoryString() + "\n";

  if (clauses() > 0) {
    result += "Clauses: ";
    result += Int::toString(clauses());
    result += " (";
    result += Int::toString(_unitAxioms+_unitGoals);
    result += " unit, ";
    result += Int::toString(_goalClauses);
    result += " goal, ";
    result += Int::toString(_equationalClauses);
    result += " equational)\n";

    result += "Variables: ";
    result += Int::toString(_totalNumberOfVariables);
    result += " (";
    result += Int::toString(_maxVariablesInClause);
    result += " maximum in a single clause)\n";
  }

  if (formulas() > 0) {
    result += "Formulas: ";
    result += Int::toString(formulas());
    result += " (";
    result += Int::toString(_goalFormulas);
    result += " goal)\n";
    result += "Subformulas: ";
    result += Int::toString(_subformulas);
    result += "\n";
  }

  result += "Atoms: ";
  result += Int::toString(_atoms);
  result += " (";
  result += Int::toString(_equalityAtoms);
  result += " equality)\n";

  return result;
} // Property::toString


/**
 * True if the clause contains a positive literal X=Y.
 * @since 04/06/2004 Manchester
 * @since 27/05/2007 Frankfurt airport, changed to new datastructures
 * @since 15/01/2014 Manchester, reimplemented
 * @author Andrei Voronkov
 */
bool Property::hasXEqualsY(const Clause* c)
{
  CALL("Property::hasXEqualsY (const Clause*)");

  for (int i = c->length()-1; i >= 0; i--) {
    const Literal* lit = (*c)[i];
    if (lit->isNegative()) {
      continue;
    }
    if (!lit->isEquality()) {
      continue;
    }
    const TermList* ts1 = lit->args();
    if (!ts1->isVar()) {
      continue;
    }
    const TermList* ts2 = ts1->next();
    if (ts2->isVar() &&
	ts1->var() != ts2->var()) {
      return true;
    }
  }
  return  false;
} // Property::hasXEqualsY(const Clause*)

/**
 * True if the subformula formula would have a literal X=Y
 * after clausification.
 *
 *
 * @warning Works correctly only with rectified formulas (closed or open)
 * @param f the formula
 * @since 11/12/2004 Manchester, true and false added
 * @since 27/05/2007 flight Frankfurt-Lisbon, changed to new datastructures
 * @since 15/01/2014 Manchester, bug fix and improvement
 * @author Andrei Voronkov
 * @warning this function can be improved, but at a higher cost, it also does not treat let constructs
 *          and if-then-else terms
 */
bool Property::hasXEqualsY(const Formula* f)
{
  CALL("Property::hasXEqualsY (const Formula*)");

  MultiCounter posVars; // universally quantified variables in positive subformulas
  MultiCounter negVars; // universally quantified variables in negative subformulas

  Stack<const Formula*> forms;
  Stack<int> pols; // polarities
  forms.push(f);
  pols.push(1);
  while (!forms.isEmpty()) {
    f = forms.pop();
    int pol = pols.pop();

    switch (f->connective()) {
    case LITERAL:
      {
	const Literal* lit = f->literal();
	if (lit->isNegative()) {
	  break;
	}
	if (!lit->isEquality()) {
	  break;
	}
	const TermList* ts1 = lit->args();
	if (!ts1->isVar()) {
	  break;
	}
	const TermList* ts2 = ts1->next();
	if (!ts2->isVar()) {
	  break;
	}
	Var v1 = ts1->var();
	Var v2 = ts2->var();
	if (v1 == v2) {
	  break;
	}
	if (!lit->isPositive()) {
	  pol = -pol;
	}
	if (pol >= 0 && posVars.get(v1) && posVars.get(v2)) {
	  return true;
	}
	if (pol <= 0 && negVars.get(v1) && negVars.get(v2)) {
	  return true;
	}
      }
      break;

    case AND:
    case OR:
      {
	FormulaList::Iterator fs(f->args());
	while (fs.hasNext()) {
	  forms.push(fs.next());
	  pols.push(pol);
	}
      }
      break;

    case IMP:
      forms.push(f->left());
      pols.push(-pol);
      forms.push(f->right());
      pols.push(pol);
      break;

    case IFF:
    case XOR:
      forms.push(f->left());
      pols.push(0);
      forms.push(f->right());
      pols.push(0);
      break;

    case NOT:
      forms.push(f->uarg());
      pols.push(-pol);
      break;

    case FORALL:
      // remember universally quantified variables
      if (pol >= 0) {
	Formula::VarList::Iterator vs(f->vars());
	while (vs.hasNext()) {
	  posVars.inc(vs.next());
	}
      }
      forms.push(f->qarg());
      pols.push(pol);
      break;

  case EXISTS:
      // remember universally quantified variables
      if (pol <= 0) {
	Formula::VarList::Iterator vs(f->vars());
	while (vs.hasNext()) {
	  posVars.inc(vs.next());
	}
      }
      forms.push(f->qarg());
      pols.push(pol);
      break;

    case ITE:
      forms.push(f->condArg());
      pols.push(0);
      forms.push(f->thenArg());
      pols.push(pol);
      forms.push(f->elseArg());
      pols.push(pol);
      break;

    case TERM_LET:
    case FORMULA_LET:
      //these two may introduce the X=Y literal but it would be too complicated to check for it
      break;
      
    case TRUE:
    case FALSE:
      break;

#if VDEBUG
    default:
      ASSERTION_VIOLATION;
#endif
    }
  }
  return false;
} // Property::hasXEqualsY(const Formula* f)

/**
 * Transforms the property to an SQL command asserting this
 * property to the Spider database. An example of a command is
 * "UPDATE problem SET property=0,category='NNE' WHERE name='TOP019-1';".
 *
 * @since 04/05/2005 Manchester
 */
vstring Property::toSpider(const vstring& problemName) const
{
  return (vstring)"UPDATE problem SET property="
    + Int::toString((int)_props)
    + ",category='"
    + categoryString()
    + "' WHERE name='"
    + problemName +
    + "';";
} // Property::toSpider

