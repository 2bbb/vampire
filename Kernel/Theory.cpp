/**
 * @file Theory.cpp
 * Implements class Theory.
 */

#include <math.h>

#include "Debug/Assertion.hpp"
#include "Debug/Tracer.hpp"

#include "Lib/Environment.hpp"
#include "Lib/Int.hpp"

#include "Shell/Skolem.hpp"

#include "Signature.hpp"
#include "SortHelper.hpp"
#include "Sorts.hpp"
#include "Term.hpp"

#include "Theory.hpp"

namespace Kernel
{

using namespace Lib;

///////////////////////
// IntegerConstantType
//

BitVectorConstantType::BitVectorConstantType(DArray<bool> binArray)
{
    CALL("BitVectorConstantType::BitVectorConstantType(vstring, vstring)");
    setBinArray(binArray);
    sortB = env.sorts->addBitVectorSort(binArray.size());
}


IntegerConstantType::IntegerConstantType(const vstring& str)
{
  CALL("IntegerConstantType::IntegerConstantType(vstring)");

  if (!Int::stringToInt(str, _val)) {
    //TODO: raise exception only on overflow, the proper syntax should be guarded by assertion
    throw ArithmeticException();
  }
}

IntegerConstantType IntegerConstantType::operator+(const IntegerConstantType& num) const
{
  CALL("IntegerConstantType::operator+");

  InnerType res;
  if (!Int::safePlus(_val, num._val, res)) {
    throw ArithmeticException();
  }
  return IntegerConstantType(res);
}

IntegerConstantType IntegerConstantType::operator-(const IntegerConstantType& num) const
{
  CALL("IntegerConstantType::operator-/1");

  InnerType res;
  if (!Int::safeMinus(_val, num._val, res)) {
    throw ArithmeticException();
  }
  return IntegerConstantType(res);
}

IntegerConstantType IntegerConstantType::operator-() const
{
  CALL("IntegerConstantType::operator-/0");

  InnerType res;
  if (!Int::safeUnaryMinus(_val, res)) {
    throw ArithmeticException();
  }
  return IntegerConstantType(res);
}

IntegerConstantType IntegerConstantType::operator*(const IntegerConstantType& num) const
{
  CALL("IntegerConstantType::operator*");

  InnerType res;
  if (!Int::safeMultiply(_val, num._val, res)) {
    throw ArithmeticException();
  }
  return IntegerConstantType(res);
}

IntegerConstantType IntegerConstantType::operator/(const IntegerConstantType& num) const
{
  CALL("IntegerConstantType::operator/");

  //TODO: check if division corresponds to the TPTP semantic
  if (num._val==0) {
    throw ArithmeticException();
  }
  if(_val == numeric_limits<InnerType>::min() && num._val == -1){
    throw ArithmeticException();
  }
  return IntegerConstantType(_val/num._val);
}

IntegerConstantType IntegerConstantType::operator%(const IntegerConstantType& num) const
{
  CALL("IntegerConstantType::operator%");

  //TODO: check if modulo corresponds to the TPTP semantic
  if (num._val==0) {
    throw ArithmeticException();
  }
  return IntegerConstantType(_val%num._val);
}

bool IntegerConstantType::operator==(const IntegerConstantType& num) const
{
  CALL("IntegerConstantType::operator==");

  return _val==num._val;
}

bool IntegerConstantType::operator>(const IntegerConstantType& num) const
{
  CALL("IntegerConstantType::operator>");

  return _val>num._val;
}

IntegerConstantType IntegerConstantType::floor(RationalConstantType rat)
{
  CALL("IntegerConstantType::floor");

  IntegerConstantType numer = rat.numerator();
  IntegerConstantType denom = rat.denominator();
  ASS_REP(denom>0, denom.toString());

  // euclidiean for positive
  if (numer>0) {
    return numer/denom;
  }
  ASS(numer<=0);
  IntegerConstantType res = numer/denom;
  if (numer%denom!=0) {
    res = res-1;
  }
  return res;
}
IntegerConstantType IntegerConstantType::ceiling(RationalConstantType rat)
{
  CALL("IntegerConstantType::ceiling");

  IntegerConstantType numer = rat.numerator();
  IntegerConstantType denom = rat.denominator();
  ASS_REP(denom>0, denom.toString());

  // euclidian for negative
  if (numer<0) {
    return numer/denom;
  }
  ASS(numer>=0)

  IntegerConstantType res = numer/denom;
  if (numer%denom!=0) {
    res = res+1;
  }
  return res;
}

Comparison IntegerConstantType::comparePrecedence(IntegerConstantType n1, IntegerConstantType n2)
{
  CALL("IntegerConstantType::comparePrecedence");
  try {
    if (n1 == numeric_limits<InnerType>::min()) {
      if (n2 == numeric_limits<InnerType>::min()) {
        return EQUAL;
      } else {
        return GREATER;
      }
    } else {
      if (n2 == numeric_limits<InnerType>::min()) {
        return LESS;
      } else {
        InnerType an1 = abs(n1.toInner());
        InnerType an2 = abs(n2.toInner());

        ASS_GE(an1,0);
        ASS_GE(an2,0);

        return an1 < an2 ? LESS : (an1 == an2 ? // compare the signed ones, making negative greater than positive
            static_cast<Comparison>(-Int::compare(n1.toInner(), n2.toInner()))
                              : GREATER);
      }
    }
  }
  catch(ArithmeticException) {
    ASSERTION_VIOLATION;
    throw;
  }
}

vstring IntegerConstantType::toString() const
{
  CALL("IntegerConstantType::toString");

  return Int::toString(_val);
}

vstring BitVectorConstantType::toString() const
{
    CALL("BitVectorConstantType::toString");
    BitVectorOperations::printBoolArrayContent(binArray);
    return "bv" + BitVectorOperations::boolArraytoString(binArray);
}

///////////////////////
// RationalConstantType
//

RationalConstantType::RationalConstantType(InnerType num, InnerType den)
{
  CALL("RationalConstantType::RationalConstantType");

  init(num, den);
}

RationalConstantType::RationalConstantType(const vstring& num, const vstring& den)
{
  CALL("RationalConstantType::RationalConstantType");

  init(InnerType(num), InnerType(den));
}

RationalConstantType::RationalConstantType(InnerType num)
{
  CALL("RationalConstantType::RationalConstantType");

  init(num, IntegerConstantType(1));
}

void RationalConstantType::init(InnerType num, InnerType den)
{
  CALL("RationalConstantType::init");

  _num = num;
  _den = den;
  cannonize();

  // Dividing by zero is bad!
  if(_den.toInner()==0) throw ArithmeticException();
}

RationalConstantType RationalConstantType::operator+(const RationalConstantType& o) const
{
  CALL("RationalConstantType::operator+");

  if (_den==o._den) {
    return RationalConstantType(_num + o._num, _den);
  }
  return RationalConstantType(_num*o._den + o._num*_den, _den*o._den);
}

RationalConstantType RationalConstantType::operator-(const RationalConstantType& o) const
{
  CALL("RationalConstantType::operator-/1");

  return (*this) + (-o);
}

RationalConstantType RationalConstantType::operator-() const
{
  CALL("RationalConstantType::operator-/0");

  return RationalConstantType(-_num, _den);
}

RationalConstantType RationalConstantType::operator*(const RationalConstantType& o) const
{
  CALL("RationalConstantType::operator*");

  return RationalConstantType(_num*o._num, _den*o._den);
}

RationalConstantType RationalConstantType::operator/(const RationalConstantType& o) const
{
  CALL("RationalConstantType::operator/");

  return RationalConstantType(_num*o._den, _den*o._num);
}

bool RationalConstantType::isInt() const
{
  CALL("RationalConstantType::isInt");

  return _den==1;
}

bool RationalConstantType::operator==(const RationalConstantType& o) const
{
  CALL("IntegerConstantType::operator==");

  return _num==o._num && _den==o._den;
}

bool RationalConstantType::operator>(const RationalConstantType& o) const
{
  CALL("IntegerConstantType::operator>");

  return (_num*o._den)>(o._num*_den);
}


vstring RationalConstantType::toString() const
{
  CALL("RationalConstantType::toString");

  vstring numStr = _num.toString();
  vstring denStr = _den.toString();

//  return "("+numStr+"/"+denStr+")";
  return numStr+"/"+denStr;
}

/**
 * Ensure the GCD of numerator and denominator is 1, and the only
 * number that may be negative is numerator
 */
void RationalConstantType::cannonize()
{
  CALL("RationalConstantType::cannonize");

  InnerType gcd = Int::gcd(_num.toInner(), _den.toInner());
  if (gcd!=1) {
    _num = _num/gcd;
    _den = _den/gcd;
  }
  if (_den<0) {
    _num = -_num;
    _den = -_den;
  }
  // Normalize zeros
  // If it is of the form 0/c then rewrite it to 0/1
  // Unless it is of the form 0/0
  if(_num==0 && _den!=0){ _den=1; }
}

Comparison RationalConstantType::comparePrecedence(RationalConstantType n1, RationalConstantType n2)
{
  CALL("RationalConstantType::comparePrecedence");
  try {

    if (n1==n2) { return EQUAL; }

    bool haveRepr1 = true;
    bool haveRepr2 = true;

    IntegerConstantType repr1, repr2;

    try {
      repr1 = n1.numerator()+n1.denominator();
    } catch(ArithmeticException) {
      haveRepr1 = false;
    }

    try {
      repr2 = n2.numerator()+n2.denominator();
    } catch(ArithmeticException) {
      haveRepr2 = false;
    }

    if (haveRepr1 && haveRepr2) {
      Comparison res = IntegerConstantType::comparePrecedence(repr1, repr2);
      if (res==EQUAL) {
	res = IntegerConstantType::comparePrecedence(n1.numerator(), n2.numerator());
      }
      ASS_NEQ(res, EQUAL);
      return res;
    }
    if (haveRepr1 && !haveRepr2) {
      return LESS;
    }
    if (!haveRepr1 && haveRepr2) {
      return GREATER;
    }

    ASS(!haveRepr1);
    ASS(!haveRepr2);

    Comparison res = IntegerConstantType::comparePrecedence(n1.denominator(), n2.denominator());
    if (res==EQUAL) {
      res = IntegerConstantType::comparePrecedence(n1.numerator(), n2.numerator());
    }
    ASS_NEQ(res, EQUAL);
    return res;
  }
  catch(ArithmeticException) {
    ASSERTION_VIOLATION;
    throw;
  }
}


///////////////////////
// RealConstantType
//

Comparison RealConstantType::comparePrecedence(RealConstantType n1, RealConstantType n2)
{
  CALL("RealConstantType::comparePrecedence");

  return RationalConstantType::comparePrecedence(n1, n2);
}

bool RealConstantType::parseDouble(const vstring& num, RationalConstantType& res)
{
  CALL("RealConstantType::parseDouble");

  try {
    vstring newNum;
    IntegerConstantType denominator = 1;
    bool haveDecimal = false;
    bool neg = false;
    size_t nlen = num.size();
    for(size_t i=0; i<nlen; i++) {
      if (num[i]=='.') {
	if (haveDecimal) {
	  return false;
	}
	haveDecimal = true;
      }
      else if (i==0 && num[i]=='-') {
	neg = true;
      }
      else if (num[i]>='0' && num[i]<='9') {
	if (newNum=="0") {
	  newNum = num[i];
	}
	else {
	  newNum += num[i];
	}
	if (haveDecimal) {
	  denominator = denominator * 10;
	}
      }
      else {
	return false;
      }
    }
    if (neg) {
      newNum = '-'+newNum;
    }
    IntegerConstantType numerator(newNum);
    res = RationalConstantType(numerator, denominator);
  } catch(ArithmeticException) {
    return false;
  }
  return true;
}

RealConstantType::RealConstantType(const vstring& number)
{
  CALL("RealConstantType::RealConstantType");

  RationalConstantType value;
  if (parseDouble(number, value)) {
    init(value.numerator(), value.denominator());
    return;
  }

  double numDbl;
  if (!Int::stringToDouble(number, numDbl)) {
    //TODO: raise exception only on overflow, the proper syntax should be guarded by assertion
    throw ArithmeticException();
  }
  InnerType denominator = 1;
  while(::floor(numDbl)!=numDbl) {
    denominator = denominator*10;
    numDbl *= 10;
  }

  InnerType::InnerType numerator = static_cast<InnerType::InnerType>(numDbl);
  if (numerator!=numDbl) {
    //the numerator part of double doesn't fit inside the inner integer type
    throw ArithmeticException();
  }
  init(numerator, denominator);
}

vstring RealConstantType::toNiceString() const
{
  CALL("RealConstantType::toNiceString");

  if (denominator().toInner()==1) {
    return numerator().toString()+".0";
  }
  float frep = (float) numerator().toInner() /(float) denominator().toInner();
  return Int::toString(frep);
  //return toString();
}

/////////////////
// Theory
//

Theory Theory::theory_obj;  // to facilitate destructor call at deinitization
Theory::Tuples Theory::tuples_obj;

Theory* theory = &Theory::theory_obj;
Theory::Tuples* theory_tuples = &Theory::tuples_obj;

/**
 * Accessor for the singleton instance of the Theory class.
 */
Theory* Theory::instance()
{
  return theory;
}

Theory::Tuples* Theory::tuples()
{
  return theory_tuples;
}

/**
 * Constructor of the Theory object
 *
 * The constructor is private, since Theory is a singleton class.
 */
Theory::Theory()
{

}

/**
 * Return arity of the symbol that is interpreted by Interpretation
 * @b i.
 */
unsigned Theory::getArity(Interpretation i)
{
  CALL("Signature::InterpretedSymbol::getArity");
  ASS(theory->isValidInterpretation(i));

  if (theory->isStructuredSortInterpretation(i)){
    switch (theory->convertToStructured(i)) {
      case StructuredSortInterpretation::BVNEG:
      case StructuredSortInterpretation::BVNOT:    
            return 1;
      case StructuredSortInterpretation::ARRAY_SELECT:
      case StructuredSortInterpretation::ARRAY_BOOL_SELECT:
      case StructuredSortInterpretation::BVSLT:
      case StructuredSortInterpretation::BVAND:
      case StructuredSortInterpretation::BVLSHR:
      case StructuredSortInterpretation::CONCAT:
      case StructuredSortInterpretation::BVADD:
      case StructuredSortInterpretation::BVNAND:
      case StructuredSortInterpretation::BVOR:
      case StructuredSortInterpretation::BVXOR:
      case StructuredSortInterpretation::BVNOR:
      case StructuredSortInterpretation::BVXNOR:
      case StructuredSortInterpretation::BVASHR:
      case StructuredSortInterpretation::BVCOMP:
      case StructuredSortInterpretation::BVSUB:
      case StructuredSortInterpretation::BVSDIV: 
      case StructuredSortInterpretation::BVSREM:
      case StructuredSortInterpretation::BVSMOD:
      case StructuredSortInterpretation::BVULE:
      case StructuredSortInterpretation::BVUGT:
      case StructuredSortInterpretation::BVUGE:
      case StructuredSortInterpretation::BVSLE:
      case StructuredSortInterpretation::BVSGT:
      case StructuredSortInterpretation::BVSGE:
      case StructuredSortInterpretation::BVULT:
      case StructuredSortInterpretation::BV_ZERO_EXTEND:
      case StructuredSortInterpretation::BV_SIGN_EXTEND:
      case StructuredSortInterpretation::BVSHL:
      case StructuredSortInterpretation::REPEAT:
      case StructuredSortInterpretation::BV_ROTATE_LEFT:
      case StructuredSortInterpretation::BV_ROTATE_RIGHT: 
      case StructuredSortInterpretation::BVMUL:
      case StructuredSortInterpretation::BVUDIV: 
      case StructuredSortInterpretation::BVUREM:
        return 2;
      case StructuredSortInterpretation::ARRAY_STORE:
      case StructuredSortInterpretation::EXTRACT:
        return 3;
      default:
          USER_ERROR("Add to list here");
    }
  }

  switch(i) {
  case INT_IS_INT:
  case INT_IS_RAT:
  case INT_IS_REAL:
  case RAT_IS_INT:
  case RAT_IS_RAT:
  case RAT_IS_REAL:
  case REAL_IS_INT:
  case REAL_IS_RAT:
  case REAL_IS_REAL:

  case INT_TO_INT:
  case INT_TO_RAT:
  case INT_TO_REAL:
  case RAT_TO_INT:
  case RAT_TO_RAT:
  case RAT_TO_REAL:
  case REAL_TO_INT:
  case REAL_TO_RAT:
  case REAL_TO_REAL:

  case INT_SUCCESSOR:
  case INT_UNARY_MINUS:
  case RAT_UNARY_MINUS:
  case REAL_UNARY_MINUS:

  case INT_FLOOR:
  case INT_CEILING:
  case INT_TRUNCATE:
  case INT_ROUND:
  case INT_ABS:

  case RAT_FLOOR:
  case RAT_CEILING:
  case RAT_TRUNCATE:
  case RAT_ROUND:

  case REAL_FLOOR:
  case REAL_CEILING:
  case REAL_TRUNCATE:
  case REAL_ROUND:
  //case BVNOT:
  //case BVNEG:

    return 1;

  case EQUAL:

  case INT_GREATER:
  case INT_GREATER_EQUAL:
  case INT_LESS:
  case INT_LESS_EQUAL:
  case INT_DIVIDES:

  case RAT_GREATER:
  case RAT_GREATER_EQUAL:
  case RAT_LESS:
  case RAT_LESS_EQUAL:

  case REAL_GREATER:
  case REAL_GREATER_EQUAL:
  case REAL_LESS:
  case REAL_LESS_EQUAL:

  case INT_PLUS:
  case INT_MINUS:
  case INT_MULTIPLY:
  case INT_QUOTIENT_E:
  case INT_QUOTIENT_T:
  case INT_QUOTIENT_F:
  case INT_REMAINDER_E:
  case INT_REMAINDER_T:
  case INT_REMAINDER_F:

  case RAT_PLUS:
  case RAT_MINUS:
  case RAT_MULTIPLY:
  case RAT_QUOTIENT:
  case RAT_QUOTIENT_E:
  case RAT_QUOTIENT_T:
  case RAT_QUOTIENT_F:
  case RAT_REMAINDER_E:
  case RAT_REMAINDER_T:
  case RAT_REMAINDER_F:

  case REAL_PLUS:
  case REAL_MINUS:
  case REAL_MULTIPLY:
  case REAL_QUOTIENT:
  case REAL_QUOTIENT_E:
  case REAL_QUOTIENT_T:
  case REAL_QUOTIENT_F:
  case REAL_REMAINDER_E:
  case REAL_REMAINDER_T:
  case REAL_REMAINDER_F:
      return 2;


  default:
    ASSERTION_VIOLATION_REP(i);
  }
}

/**
 * Return true iff the symbol that is interpreted by Interpretation
 * is a function (false is returned for predicates)
 */
bool Theory::isFunction(Interpretation i)
{
  cout<<" problem Interpretation "<< i;
  CALL("Signature::InterpretedSymbol::isFunction");
  ASS(theory->isValidInterpretation(i));

  if(theory->isStructuredSortInterpretation(i)){
    switch(theory->convertToStructured(i)){
      case StructuredSortInterpretation::ARRAY_BOOL_SELECT:
      case StructuredSortInterpretation::BVSLT:
      case StructuredSortInterpretation::BVULE:
      case StructuredSortInterpretation::BVUGT:
      case StructuredSortInterpretation::BVUGE:
      case StructuredSortInterpretation::BVSLE:
      case StructuredSortInterpretation::BVSGT:
      case StructuredSortInterpretation::BVSGE:
      case StructuredSortInterpretation::BVULT:    
          
        return false;
      default:
        return true;
    }
  }

  switch(i) {
  case INT_TO_INT:
  case INT_TO_RAT:
  case INT_TO_REAL:
  case RAT_TO_INT:
  case RAT_TO_RAT:
  case RAT_TO_REAL:
  case REAL_TO_INT:
  case REAL_TO_RAT:
  case REAL_TO_REAL:

  case INT_SUCCESSOR:
  case INT_UNARY_MINUS:
  case RAT_UNARY_MINUS:
  case REAL_UNARY_MINUS:

  case INT_PLUS:
  case INT_MINUS:
  case INT_MULTIPLY:
  case INT_QUOTIENT_E:
  case INT_QUOTIENT_T:
  case INT_QUOTIENT_F:
  case INT_REMAINDER_E:
  case INT_REMAINDER_T:
  case INT_REMAINDER_F:
  case INT_FLOOR:
  case INT_CEILING:
  case INT_TRUNCATE:
  case INT_ROUND:
  case INT_ABS:

  case RAT_PLUS:
  case RAT_MINUS:
  case RAT_MULTIPLY:
  case RAT_QUOTIENT:
  case RAT_QUOTIENT_E:
  case RAT_QUOTIENT_T:
  case RAT_QUOTIENT_F:
  case RAT_REMAINDER_E:
  case RAT_REMAINDER_T:
  case RAT_REMAINDER_F:
  case RAT_FLOOR:
  case RAT_CEILING:
  case RAT_TRUNCATE:
  case RAT_ROUND:

  case REAL_PLUS:
  case REAL_MINUS:
  case REAL_MULTIPLY:
  case REAL_QUOTIENT:
  case REAL_QUOTIENT_E:
  case REAL_QUOTIENT_T:
  case REAL_QUOTIENT_F:
  case REAL_REMAINDER_E:
  case REAL_REMAINDER_T:
  case REAL_REMAINDER_F:
  case REAL_FLOOR:
  case REAL_CEILING:
  case REAL_TRUNCATE:
  case REAL_ROUND:


  case EQUAL:

  case INT_GREATER:
  case INT_GREATER_EQUAL:
  case INT_LESS:
  case INT_LESS_EQUAL:
  case INT_DIVIDES:

  case RAT_GREATER:
  case RAT_GREATER_EQUAL:
  case RAT_LESS:
  case RAT_LESS_EQUAL:

  case REAL_GREATER:
  case REAL_GREATER_EQUAL:
  case REAL_LESS:
  case REAL_LESS_EQUAL:

  case INT_IS_INT:
  case INT_IS_RAT:
  case INT_IS_REAL:
  case RAT_IS_INT:
  case RAT_IS_RAT:
  case RAT_IS_REAL:
  case REAL_IS_INT:
  case REAL_IS_RAT:
  case REAL_IS_REAL:

    return false;

  default:
    ASSERTION_VIOLATION;
  }
}

/**
 * Return true iff the symbol that is interpreted by Interpretation
 * is inequality predicate
 */
bool Theory::isInequality(Interpretation i)
{
  CALL("Signature::InterpretedSymbol::isInequality");
  ASS(theory->isValidInterpretation(i));

  switch(i) {
  case INT_GREATER:
  case INT_GREATER_EQUAL:
  case INT_LESS:
  case INT_LESS_EQUAL:
  case RAT_GREATER:
  case RAT_GREATER_EQUAL:
  case RAT_LESS:
  case RAT_LESS_EQUAL:
  case REAL_GREATER:
  case REAL_GREATER_EQUAL:
  case REAL_LESS:
  case REAL_LESS_EQUAL:
    return true;
  default:
    return false;
  }
  ASSERTION_VIOLATION;
}

/**
 * Return true if interpreted operation @c i has all arguments and
 * (in case of a function) the result type of the same sort.
 * For such operation the @c getOperationSort() function can be
 * called.
 */
bool Theory::hasSingleSort(Interpretation i)
{
  CALL("Theory::hasSingleSort");

  switch(i) {
  case EQUAL:  // This not SingleSort because we don't know the sorts of its args
  case INT_TO_RAT:
  case INT_TO_REAL:
  case RAT_TO_INT:
  case RAT_TO_REAL:
  case REAL_TO_INT:
  case REAL_TO_RAT:
    return false;
  default:
    return true;
  }
}

/**
 * This function can be called for operations for which  the
 * function @c hasSingleSort returns true
 */
unsigned Theory::getOperationSort(Interpretation i)
{
  CALL("Theory::getOperationSort");


  ASS(hasSingleSort(i));
  ASS(theory->isValidInterpretation(i));

  if (theory->isStructuredSortInterpretation(i)) {
    return theory->getStructuredOperationSort(i);
  }

  switch(i) {
  case INT_GREATER:
  case INT_GREATER_EQUAL:
  case INT_LESS:
  case INT_LESS_EQUAL:
  case INT_DIVIDES:
  case INT_SUCCESSOR:
  case INT_UNARY_MINUS:
  case INT_PLUS:
  case INT_MINUS:
  case INT_MULTIPLY:
  case INT_QUOTIENT_E:
  case INT_QUOTIENT_T:
  case INT_QUOTIENT_F:
  case INT_REMAINDER_E:
  case INT_REMAINDER_T:
  case INT_REMAINDER_F:
  case INT_FLOOR:
  case INT_CEILING:
  case INT_TRUNCATE:
  case INT_ROUND:
  case INT_ABS:

  case INT_TO_INT:
  case INT_IS_INT:
  case INT_IS_RAT:
  case INT_IS_REAL:
    return Sorts::SRT_INTEGER;

  case RAT_UNARY_MINUS:
  case RAT_PLUS:
  case RAT_MINUS:
  case RAT_MULTIPLY:
  case RAT_QUOTIENT:
  case RAT_QUOTIENT_E:
  case RAT_QUOTIENT_T:
  case RAT_QUOTIENT_F:
  case RAT_REMAINDER_E:
  case RAT_REMAINDER_T:
  case RAT_REMAINDER_F:
  case RAT_FLOOR:
  case RAT_CEILING:
  case RAT_TRUNCATE:
  case RAT_ROUND:
  case RAT_GREATER:
  case RAT_GREATER_EQUAL:
  case RAT_LESS:
  case RAT_LESS_EQUAL:

  case RAT_TO_RAT:
  case RAT_IS_INT:
  case RAT_IS_RAT:
  case RAT_IS_REAL:
    return Sorts::SRT_RATIONAL;

  case REAL_UNARY_MINUS:
  case REAL_PLUS:
  case REAL_MINUS:
  case REAL_MULTIPLY:
  case REAL_QUOTIENT:
  case REAL_QUOTIENT_E:
  case REAL_QUOTIENT_T:
  case REAL_QUOTIENT_F:
  case REAL_REMAINDER_E:
  case REAL_REMAINDER_T:
  case REAL_REMAINDER_F:
  case REAL_FLOOR:
  case REAL_CEILING:
  case REAL_TRUNCATE:
  case REAL_ROUND:
  case REAL_GREATER:
  case REAL_GREATER_EQUAL:
  case REAL_LESS:
  case REAL_LESS_EQUAL:

  case REAL_TO_REAL:
  case REAL_IS_INT:
  case REAL_IS_RAT:
  case REAL_IS_REAL:
    return Sorts::SRT_REAL;

      
  default:
    ASSERTION_VIOLATION;
  }
}



bool Theory::isConversionOperation(Interpretation i)
{
  CALL("Theory::isConversionOperation");

  //we do not include operations as INT_TO_INT here because they actually
  //don't convert anything (they're identities)
  switch(i) {
  case INT_TO_RAT:
  case INT_TO_REAL:
  case RAT_TO_INT:
  case RAT_TO_REAL:
  case REAL_TO_INT:
  case REAL_TO_RAT:
    return true;
  default:
    return false;
  }
}
bool Theory::isLinearOperation(Interpretation i)
{
  CALL("Theory::isComparisonOperation");

  switch(i) {
  case INT_UNARY_MINUS:
  case INT_PLUS:
  case INT_MINUS:
  case RAT_UNARY_MINUS:
  case RAT_PLUS:
  case RAT_MINUS:
  case REAL_UNARY_MINUS:
  case REAL_PLUS:
  case REAL_MINUS:
    return true;
  default:
    return false;
  }
}
bool Theory::isNonLinearOperation(Interpretation i)
{
  CALL("Theory::isComparisonOperation");

  switch(i) {
  case INT_MULTIPLY:
  case INT_QUOTIENT_E:
  case INT_QUOTIENT_T:
  case INT_QUOTIENT_F:
  case INT_REMAINDER_E:
  case INT_REMAINDER_T:
  case INT_REMAINDER_F:
  case RAT_MULTIPLY:
  case RAT_QUOTIENT:
  case RAT_QUOTIENT_E:
  case RAT_QUOTIENT_T:
  case RAT_QUOTIENT_F:
  case RAT_REMAINDER_E:
  case RAT_REMAINDER_T:
  case RAT_REMAINDER_F:
  case REAL_MULTIPLY:
  case REAL_QUOTIENT:
  case REAL_QUOTIENT_E:
  case REAL_QUOTIENT_T:
  case REAL_QUOTIENT_F:
  case REAL_REMAINDER_E:
  case REAL_REMAINDER_T:
  case REAL_REMAINDER_F:
    return true;
  default:
    return false;
  }
}

unsigned Theory::getSymbolForStructuredSort(unsigned sort, StructuredSortInterpretation interp)
{
   
    return getSymbolForStructuredSort(sort, interp, -1,-1);
}

unsigned Theory::getSymbolForStructuredSort(unsigned sort, StructuredSortInterpretation interp, int arg1, int arg2)
{
    cout<<"\n Theory::getSymbolForStructuredSort\n";
    return env.signature->getInterpretingSymbol(getInterpretation(sort,interp,arg1,arg2));
}

/**
 * Return true if interpreted function @c i is an array operation.
 * @author Laura Kovacs
 * @since 31/08/2012, Vienna
 */
bool Theory::isArrayOperation(Interpretation i)
{
  CALL("Theory::isArrayFunction");
  if(!theory->isStructuredSortInterpretation(i)) return false;
  return env.sorts->hasStructuredSort(theory->getSort(i),Sorts::StructuredSort::ARRAY);
}

bool Theory::isBitVectorOperation(Interpretation i)
{
  CALL("Theory::isBitVectorOperation");
  if(!theory->isStructuredSortInterpretation(i)) return false;
  return env.sorts->hasStructuredSort(theory->getSort(i),Sorts::StructuredSort::BITVECTOR);
}

/**
* This function can be called for array operations
* it returns the range domain (the sort of the output) of select and store
* @author Laura Kovacs
* @since 31/08/2012, Vienna
*/
unsigned Theory::getArrayOperationSort(Interpretation i)
{
    CALL("Theory::getArrayOperationSort");
    ASS(isArrayOperation(i));

    unsigned sort = theory->getSort(i);

    switch(theory->convertToStructured(i))
    {
      case StructuredSortInterpretation::ARRAY_SELECT:
      case StructuredSortInterpretation::ARRAY_BOOL_SELECT:
        return env.sorts->getArraySort(sort)->getInnerSort();
      case StructuredSortInterpretation::ARRAY_STORE:
        return sort;
      default:
        ASSERTION_VIOLATION;
    }
}



/**
* This function returns the domain of array indexes (SRT_INT)
* @author Laura Kovacs
* @since 31/08/2012, Vienna
* @since 7/10/2015 update to support polymorphism in the index sort
*/
unsigned Theory::getArrayDomainSort(Interpretation i)
{
    CALL("Theory::getArrayDomainSort");
    ASS(isArrayOperation(i));

    unsigned sort = theory->getSort(i);

    return  env.sorts->getArraySort(sort)->getIndexSort();
}

/*unsigned Theory::getBitVectorArg1Sort(Interpretation i )
{
    CALL("Theory::getBitVectorArg1Sort");
    ASS(isBitVectorOperation(i));

    unsigned sort = theory->getSort(i);
    cout<<"\n in getBitVectorArg1Sort and sort is :"<< sort<<"\n";
    return  env.sorts->getBitVectorSort(sort)->getSizeArg1();
}

unsigned Theory::getBitVectorArg2Sort(Interpretation i )
{
    CALL("Theory::getBitVectorArg1Sort");
    ASS(isBitVectorOperation(i));

    unsigned sort = theory->getSort(i);
    cout<<"\n in getBitVectorArg2Sort and sort is :"<< sort<<"\n";
    return  env.sorts->getBitVectorSort(sort)->getSizeArg2();
}*/

/**
 * Get the number of the skolem function symbol used in the clause form of the
 * array extensionality axiom (of particular sort).
 *
 * select(X,sk(X,Y)) != select(Y,sk(X,Y)) | X = Y
 *
 * If the symbol does not exist yet, it is added to the signature. We use 0 to
 * represent that the symbol not yet exists, assuming that at call time of this
 * method, at least the array function are already in the signature.
 *
 * We want to have this function available e.g. in simplification rules.
 */
unsigned Theory::getArrayExtSkolemFunction(unsigned sort) {

  if(_arraySkolemFunctions.find(sort)){
    return _arraySkolemFunctions.get(sort);
  }

  bool isBool = (env.sorts->getArraySort(sort)->getInnerSort() == Sorts::SRT_BOOL);

  Interpretation store = getInterpretation(sort, StructuredSortInterpretation::ARRAY_STORE,-1,-1);
  Interpretation select = getInterpretation(sort, isBool ? StructuredSortInterpretation::ARRAY_BOOL_SELECT
                                                         : StructuredSortInterpretation::ARRAY_SELECT,-1,-1);

  unsigned arraySort = getArrayOperationSort(store);
  unsigned indexSort = theory->getArrayDomainSort(select);
  unsigned params[] = {arraySort, arraySort};
  unsigned skolemFunction = Shell::Skolem::addSkolemFunction(2, params, indexSort, "arrayDiff");

  _arraySkolemFunctions.insert(sort,skolemFunction);

  return skolemFunction;
}

unsigned Theory::Tuples::getFunctor(unsigned arity, unsigned* sorts) {
  CALL("Theory::Tuples::getFunctor(unsigned arity, unsigned* sorts)");
  return getFunctor(env.sorts->addTupleSort(arity, sorts));
}

unsigned Theory::Tuples::getFunctor(unsigned tupleSort) {
  CALL("Theory::Tuples::getFunctor(unsigned tupleSort)");

  ASS_REP(env.sorts->hasStructuredSort(tupleSort, Sorts::StructuredSort::TUPLE),
          env.sorts->sortName(tupleSort));

  Sorts::TupleSort* tuple = env.sorts->getTupleSort(tupleSort);
  unsigned  arity = tuple->arity();
  unsigned* sorts = tuple->sorts();

  theory->defineTupleTermAlgebra(arity, sorts);
  ASS(env.signature->isTermAlgebraSort(tupleSort));
  Shell::TermAlgebra* ta = env.signature->getTermAlgebraOfSort(tupleSort);

  return ta->constructor(0)->functor();
}

bool Theory::Tuples::isFunctor(unsigned functor) {
  CALL("Theory::Tuples::isFunctor(unsigned)");
  unsigned tupleSort = env.signature->getFunction(functor)->fnType()->result();
  return env.sorts->hasStructuredSort(tupleSort, Sorts::StructuredSort::TUPLE);
}

unsigned Theory::Tuples::getProjectionFunctor(unsigned proj, unsigned tupleSort) {
  CALL("Theory::Tuples::getProjectionFunctor");

  ASS_REP(env.sorts->hasStructuredSort(tupleSort, Sorts::StructuredSort::TUPLE),
          env.sorts->sortName(tupleSort));

  Sorts::TupleSort* tuple = env.sorts->getTupleSort(tupleSort);
  unsigned  arity = tuple->arity();
  unsigned* sorts = tuple->sorts();

  theory->defineTupleTermAlgebra(arity, sorts);
  ASS(env.signature->isTermAlgebraSort(tupleSort));
  Shell::TermAlgebra* ta = env.signature->getTermAlgebraOfSort(tupleSort);

  Shell::TermAlgebraConstructor* c = ta->constructor(0);

  ASS_NEQ(proj, c->arity());

  return c->destructorFunctor(proj);
}

// TODO: replace with a constant time algorithm
bool Theory::Tuples::findProjection(unsigned projFunctor, bool isPredicate, unsigned &proj) {
  CALL("Theory::Tuples::findProjection");

  BaseType* projType = isPredicate ? static_cast<BaseType*>(env.signature->getPredicate(projFunctor)->predType())
                                   : static_cast<BaseType*>(env.signature->getFunction(projFunctor)->fnType());

  if (projType->arity() != 1) {
    return false;
  }

  unsigned tupleSort = projType->arg(0);

  if (!env.sorts->hasStructuredSort(tupleSort, Sorts::StructuredSort::TUPLE)) {
    return false;
  }

  if (!env.signature->isTermAlgebraSort(tupleSort)) {
    return false;
  }

  Shell::TermAlgebraConstructor* c = env.signature->getTermAlgebraOfSort(tupleSort)->constructor(0);
  for (unsigned i = 0; i < c->arity(); i++) {
    if (projFunctor == c->destructorFunctor(i)) {
      proj = i;
      return true;
    }
  }

  return false;
}


/**
 * This function creates a type for converion function @c i.
 *
 * @c i must be a type conversion operation.
 */
FunctionType* Theory::getConversionOperationType(Interpretation i)
{
  CALL("Theory::getConversionOperationType");

  unsigned from, to;
  switch(i) {
  case INT_TO_RAT:
    from = Sorts::SRT_INTEGER;
    to = Sorts::SRT_RATIONAL;
    break;
  case INT_TO_REAL:
    from = Sorts::SRT_INTEGER;
    to = Sorts::SRT_REAL;
    break;
  case RAT_TO_INT:
    from = Sorts::SRT_RATIONAL;
    to = Sorts::SRT_INTEGER;
    break;
  case RAT_TO_REAL:
    from = Sorts::SRT_RATIONAL;
    to = Sorts::SRT_REAL;
    break;
  case REAL_TO_INT:
    from = Sorts::SRT_REAL;
    to = Sorts::SRT_INTEGER;
    break;
  case REAL_TO_RAT:
    from = Sorts::SRT_REAL;
    to = Sorts::SRT_RATIONAL;
    break;
  default:
    ASSERTION_VIOLATION;
  }
  return new FunctionType({from}, to);
}

Sorts::StructuredSort Theory::getInterpretedSort(StructuredSortInterpretation ssi) {
  switch (ssi) {
    case StructuredSortInterpretation::ARRAY_SELECT:
    case StructuredSortInterpretation::ARRAY_BOOL_SELECT:
    case StructuredSortInterpretation::ARRAY_STORE:
      return Sorts::StructuredSort::ARRAY;
    case StructuredSortInterpretation::LIST_HEAD:
    case StructuredSortInterpretation::LIST_TAIL:
    case StructuredSortInterpretation::LIST_CONS:
    case StructuredSortInterpretation::LIST_IS_EMPTY:
      return Sorts::StructuredSort::LIST;
    case StructuredSortInterpretation::BVSLT:
    case StructuredSortInterpretation::BVAND:
    case StructuredSortInterpretation::BVLSHR:
    case StructuredSortInterpretation::BVNEG:
    case StructuredSortInterpretation::CONCAT:
    case StructuredSortInterpretation::EXTRACT:
    case StructuredSortInterpretation::BVADD:
    case StructuredSortInterpretation::BVNOT:
    case StructuredSortInterpretation::BVOR:
    case StructuredSortInterpretation::BVMUL:
    case StructuredSortInterpretation::BVUDIV:
    case StructuredSortInterpretation::BVUREM:
    case StructuredSortInterpretation::BVSHL:
    case StructuredSortInterpretation::BVULT:
    case StructuredSortInterpretation::BVNAND:
    case StructuredSortInterpretation::BVNOR:
    case StructuredSortInterpretation::BVXOR:
    case StructuredSortInterpretation::BVXNOR:
    case StructuredSortInterpretation::BVCOMP:
    case StructuredSortInterpretation::BVSUB:
    case StructuredSortInterpretation::BVSDIV:
    case StructuredSortInterpretation::BVSREM:
    case StructuredSortInterpretation::BVSMOD:
    case StructuredSortInterpretation::BVASHR:
    case StructuredSortInterpretation::BV_ZERO_EXTEND:
    case StructuredSortInterpretation::BV_SIGN_EXTEND:
    case StructuredSortInterpretation::BV_ROTATE_LEFT:
    case StructuredSortInterpretation::BV_ROTATE_RIGHT:
    case StructuredSortInterpretation::BVULE:
    case StructuredSortInterpretation::BVUGT:
    case StructuredSortInterpretation::BVUGE:
    case StructuredSortInterpretation::BVSLE:
    case StructuredSortInterpretation::BVSGT:
    case StructuredSortInterpretation::BVSGE:
    case StructuredSortInterpretation::REPEAT:    
        return Sorts::StructuredSort::BITVECTOR;
    default:
      ASSERTION_VIOLATION;
  }
}

vstring Theory::getInterpretationName(Interpretation interp) {
  CALL("Theory::getInterpretationName");

  if (Theory::instance()->isStructuredSortInterpretation(interp)) {
    switch (Theory::instance()->convertToStructured(interp)) {
      case StructuredSortInterpretation::ARRAY_SELECT:
      case StructuredSortInterpretation::ARRAY_BOOL_SELECT:
        return "$select";
      case StructuredSortInterpretation::ARRAY_STORE:
        return "$store";
      case StructuredSortInterpretation::BVSLT:
        return "$bvslt";
      case StructuredSortInterpretation::BVAND:
        return "$bvand";
      case StructuredSortInterpretation::BVLSHR:  
          return "$bvlshr";
      case StructuredSortInterpretation::BVNEG:  
          return "$bvneg";   
      case StructuredSortInterpretation::CONCAT:  
          return "$concat";
      case StructuredSortInterpretation::EXTRACT:  
          return "$extract";
      case StructuredSortInterpretation::BVADD:  
          return "$bvadd";
          
      case StructuredSortInterpretation::BVNOT:  
          return "$bvnot";
      case StructuredSortInterpretation::BVOR:  
          return "$bvor";
       
      case StructuredSortInterpretation::BVMUL:  
          return "$bvmul";  
      case StructuredSortInterpretation::BVUDIV:  
          return "$bvudiv";
          
      case StructuredSortInterpretation::BVUREM:  
          return "$bvurem";
          
      case StructuredSortInterpretation::BVSHL:  
          return "$bvshl";
      case StructuredSortInterpretation::BVULT:  
          return "$bvult";  
          
      case StructuredSortInterpretation::BVNAND:  
          return "$bvnand";
      
      case StructuredSortInterpretation::BVNOR:  
          return "$bvnor";
      
      case StructuredSortInterpretation::BVXOR:  
          return "$bvxor";
      case StructuredSortInterpretation::BVXNOR:  
          return "$bvxnor";
      case StructuredSortInterpretation::BVCOMP:  
          return "$bvcomp";    
      case StructuredSortInterpretation::BVSUB:  
          return "$bvsub";    
      case StructuredSortInterpretation::BVSDIV:  
          return "$bvsdiv";
       case StructuredSortInterpretation::BVSREM:  
          return "$bvsrem";   
       case StructuredSortInterpretation::BVSMOD:  
          return "$bvsmod";
       case StructuredSortInterpretation::BVASHR:  
          return "$bvashr";
        case StructuredSortInterpretation::BV_ZERO_EXTEND:  
          return "$bv_zero_extend";
        case StructuredSortInterpretation::BV_SIGN_EXTEND:  
          return "$bv_sign_extend";
        case StructuredSortInterpretation::BV_ROTATE_LEFT:  
          return "$bv_rotate_left";
        case StructuredSortInterpretation::BV_ROTATE_RIGHT:  
          return "$bv_rotate_right";
        case StructuredSortInterpretation::BVULE:  
            return "$bvule";
        case StructuredSortInterpretation::BVUGT:  
            return "$bvugt";
        case StructuredSortInterpretation::BVUGE:  
          return "$bvuge";
        case StructuredSortInterpretation::BVSLE:  
          return "$bvsle";
        case StructuredSortInterpretation::BVSGT:  
          return "$bvsgt";
        case StructuredSortInterpretation::BVSGE:  
          return "$bvsge";
        case StructuredSortInterpretation::REPEAT:
            return "$repeat";
            
      default:
        ASSERTION_VIOLATION_REP(interp);
    }
  } else {
    switch (interp) {
      case Theory::INT_SUCCESSOR:
        //this one is not according the TPTP arithmetic (it doesn't have successor)
        return "$successor";
      case Theory::INT_DIVIDES:
        return "$divides";
      case Theory::INT_UNARY_MINUS:
      case Theory::RAT_UNARY_MINUS:
      case Theory::REAL_UNARY_MINUS:
        return "$uminus";
      case Theory::INT_PLUS:
      case Theory::RAT_PLUS:
      case Theory::REAL_PLUS:
        return "$sum";
      case Theory::INT_MINUS:
      case Theory::RAT_MINUS:
      case Theory::REAL_MINUS:
        return "$difference";
      case Theory::INT_MULTIPLY:
      case Theory::RAT_MULTIPLY:
      case Theory::REAL_MULTIPLY:
        return "$product";
      case Theory::INT_GREATER:
      case Theory::RAT_GREATER:
      case Theory::REAL_GREATER:
        return "$greater";
      case Theory::INT_GREATER_EQUAL:
      case Theory::RAT_GREATER_EQUAL:
      case Theory::REAL_GREATER_EQUAL:
        return "$greatereq";
      case Theory::INT_LESS:
      case Theory::RAT_LESS:
      case Theory::REAL_LESS:
        return "$less";
      case Theory::INT_LESS_EQUAL:
      case Theory::RAT_LESS_EQUAL:
      case Theory::REAL_LESS_EQUAL:
        return "$lesseq";
      case Theory::INT_IS_INT:
      case Theory::RAT_IS_INT:
      case Theory::REAL_IS_INT:
        return "$is_int";
      case Theory::INT_IS_RAT:
      case Theory::RAT_IS_RAT:
      case Theory::REAL_IS_RAT:
        return "$is_rat";
      case Theory::INT_IS_REAL:
      case Theory::RAT_IS_REAL:
      case Theory::REAL_IS_REAL:
        return "$is_real";
      case Theory::INT_TO_INT:
      case Theory::RAT_TO_INT:
      case Theory::REAL_TO_INT:
        return "$to_int";
      case Theory::INT_TO_RAT:
      case Theory::RAT_TO_RAT:
      case Theory::REAL_TO_RAT:
        return "$to_rat";
      case Theory::INT_TO_REAL:
      case Theory::RAT_TO_REAL:
      case Theory::REAL_TO_REAL:
        return "$to_real";
      case Theory::INT_ABS:
        return "$abs";
      case Theory::INT_QUOTIENT_E:
      case Theory::RAT_QUOTIENT_E:
      case Theory::REAL_QUOTIENT_E:
        return "$quotient_e";
      case Theory::INT_QUOTIENT_T:
      case Theory::RAT_QUOTIENT_T:
      case Theory::REAL_QUOTIENT_T:
        return "$quotient_t";
      case Theory::INT_QUOTIENT_F:
      case Theory::RAT_QUOTIENT_F:
      case Theory::REAL_QUOTIENT_F:
        return "$quotient_f";
      case Theory::INT_REMAINDER_T:
      case Theory::RAT_REMAINDER_T:
      case Theory::REAL_REMAINDER_T:
        return "$remainder_t";
      case Theory::INT_REMAINDER_F:
      case Theory::RAT_REMAINDER_F:
      case Theory::REAL_REMAINDER_F:
        return "$remainder_f";
      case Theory::INT_REMAINDER_E:
      case Theory::RAT_REMAINDER_E:
      case Theory::REAL_REMAINDER_E:
        return "$remainder_e";
      case Theory::RAT_QUOTIENT:
      case Theory::REAL_QUOTIENT:
        return "$quotient";
      case Theory::INT_TRUNCATE:
      case Theory::RAT_TRUNCATE:
      case Theory::REAL_TRUNCATE:
        return "truncate";
      case Theory::INT_FLOOR:
      case Theory::RAT_FLOOR:
      case Theory::REAL_FLOOR:
        return "floor";
      case Theory::INT_CEILING:
      case Theory::RAT_CEILING:
      case Theory::REAL_CEILING:
        return "ceiling";
    
      default:
        ASSERTION_VIOLATION_REP(interp);
    }
  }
}

BaseType* Theory::getStructuredSortOperationType(Interpretation i) {
  CALL("Theory::getStructuredSortOperationType");

  ASS(theory->isStructuredSortInterpretation(i));

  unsigned theorySort = theory->getSort(i);
  StructuredSortInterpretation ssi = theory->convertToStructured(i);
  cout<<"\n theorySort is: \n"<<theorySort<<"\n";
  //cout<<"\n StrucutedSortInte is \n"<<ssi;
  
  switch (theory->getInterpretedSort(ssi)) {
    case Sorts::StructuredSort::ARRAY: {
      unsigned indexSort = getArrayDomainSort(i);
      unsigned valueSort = getArrayOperationSort(i);
      unsigned innerSort = env.sorts->getArraySort(theorySort)->getInnerSort();

      switch (ssi) {
        case StructuredSortInterpretation::ARRAY_SELECT:
          return new FunctionType({ theorySort, indexSort }, valueSort);

        case StructuredSortInterpretation::ARRAY_BOOL_SELECT:
          return new PredicateType({ theorySort, indexSort });

        case StructuredSortInterpretation::ARRAY_STORE:
          return new FunctionType({ theorySort, indexSort, innerSort }, valueSort);
        
        default:
          ASSERTION_VIOLATION;
      }
    }
    
      case Sorts::StructuredSort::BITVECTOR: {
          unsigned size = env.sorts->getBitVectorSort(theorySort)->getSize();
          unsigned sortt = env.sorts->addBitVectorSort(size);
          cout<<"\n theory sort is "<< theorySort<< " and sortt is "<<sortt<<"\n";
          switch (ssi){
            case StructuredSortInterpretation::BVSLT:
            case StructuredSortInterpretation::BVULT:
            case StructuredSortInterpretation::BVULE:
            case StructuredSortInterpretation::BVUGT:
            case StructuredSortInterpretation::BVUGE:
            case StructuredSortInterpretation::BVSLE:
            case StructuredSortInterpretation::BVSGT:
            case StructuredSortInterpretation::BVSGE:    
                return new PredicateType({theorySort,sortt});
            case StructuredSortInterpretation::BVAND:
            case StructuredSortInterpretation::BVLSHR:
            case StructuredSortInterpretation::BVADD:
            case StructuredSortInterpretation::BVOR:
            case StructuredSortInterpretation::BVMUL:
            case StructuredSortInterpretation::BVUDIV:
            case StructuredSortInterpretation::BVUREM:
            case StructuredSortInterpretation::BVSHL:
            case StructuredSortInterpretation::BVNAND:
            case StructuredSortInterpretation::BVNOR:
            case StructuredSortInterpretation::BVXOR:
            case StructuredSortInterpretation::BVXNOR:
            case StructuredSortInterpretation::BVSUB:
            case StructuredSortInterpretation::BVSDIV:
            case StructuredSortInterpretation::BVSREM:
            case StructuredSortInterpretation::BVSMOD:
            case StructuredSortInterpretation::BVASHR:   
                return new FunctionType({ sortt, sortt}, sortt);
            case StructuredSortInterpretation::BVNEG:
            case StructuredSortInterpretation::BVNOT:
                return new FunctionType({sortt}, sortt);
            case StructuredSortInterpretation::CONCAT:  
            {
                unsigned argSize1 = env.signature->getArg1();
                cout<<" and in corresponding concat : argSize1 : "<<argSize1;
                unsigned argSize2 = env.signature->getArg2();
                cout<<"\n and in corresponding concat : argSize2 : "<<argSize2<<"\n";
                return new FunctionType({env.sorts->addBitVectorSort(argSize1), env.sorts->addBitVectorSort(argSize2)}, sortt);
            }
            // here also
            case StructuredSortInterpretation::BV_ROTATE_LEFT:  
            case StructuredSortInterpretation::BV_ROTATE_RIGHT:
            {
                return new FunctionType({Sorts::SRT_INTEGER, sortt}, sortt);
            }
            case StructuredSortInterpretation::BV_ZERO_EXTEND:
            case StructuredSortInterpretation::BV_SIGN_EXTEND:
            case StructuredSortInterpretation::REPEAT:    
            {
                unsigned argSize1 = env.signature->getArg1();
                return new FunctionType({Sorts::SRT_INTEGER, env.sorts->addBitVectorSort(argSize1)}, sortt);
            }
            case StructuredSortInterpretation::EXTRACT:
            {
                unsigned bitVecArgSize = env.signature->getArg1();
                cout<<"checkig value \n"<<bitVecArgSize<<"\n";
                //Sorts::BitVectorSort* bv = env.sorts->getBitVectorSort(bitVecArgSize);//getBitVectorSort(bitVecArgSize);
                //cout<<" name of the sort "<< bv->name();
                cout<<" finally : "<<env.sorts->addBitVectorSort(bitVecArgSize);
                return new FunctionType({env.sorts->addBitVectorSort(bitVecArgSize), Sorts::SRT_INTEGER, Sorts::SRT_INTEGER}, sortt);
            }
            
            case StructuredSortInterpretation::BVCOMP:
            {
                //unsigned bitVecArgSize = env.signature->getArg1();
                unsigned one = 1;
                //Sorts::BitVectorSort* bv = env.sorts->getBitVectorSort(bitVecArgSize);//getBitVectorSort(bitVecArgSize);
                //cout<<" name of the sort "<< bv->name();
                cout<<endl<<"testing bvcomp sortt is "<< sortt;
                unsigned temp =  env.sorts->addBitVectorSort(one);
                cout<<endl<<" and the result is "<<  temp<<" result should be before this ";
                return new FunctionType({sortt, sortt}, temp);
            }
          
          
          default: ASSERTION_VIOLATION;
          }
      
      }
    
    default:
      ASSERTION_VIOLATION;
  }
}

/**
 * Return type of the function representing interpreted function/predicate @c i.
 */
BaseType* Theory::getOperationType(Interpretation i)
{
  CALL("Theory::getOperationType");
  ASS_NEQ(i, EQUAL);

  if (isConversionOperation(i)) {
    return getConversionOperationType(i);
  }

  if (theory->isStructuredSortInterpretation(i)) {
    return getStructuredSortOperationType(i);
  }

  unsigned sort;
  ASS(hasSingleSort(i));
  sort = getOperationSort(i);

  unsigned arity = getArity(i);

  static DArray<unsigned> domainSorts;
  domainSorts.init(arity, sort);

  if (isFunction(i)) {
    return new FunctionType(arity, domainSorts.array(), sort);
  } else {
    return new PredicateType(arity, domainSorts.array());
  }
}

void Theory::defineTupleTermAlgebra(unsigned arity, unsigned* sorts) {
  CALL("Signature::defineTupleTermAlgebra");

  unsigned tupleSort = env.sorts->addTupleSort(arity, sorts);

  if (env.signature->isTermAlgebraSort(tupleSort)) {
    return;
  }

  unsigned functor = env.signature->addFreshFunction(arity, "tuple");
  FunctionType* tupleType = new FunctionType(arity, sorts, tupleSort);
  env.signature->getFunction(functor)->setType(tupleType);
  env.signature->getFunction(functor)->markTermAlgebraCons();

  Array<unsigned> destructors(arity);
  for (unsigned i = 0; i < arity; i++) {
    unsigned projSort = sorts[i];
    unsigned destructor;
    if (projSort == Sorts::SRT_BOOL) {
      destructor = env.signature->addFreshPredicate(1, "proj");
      env.signature->getPredicate(destructor)->setType(new PredicateType({ tupleSort }));
    } else {
      destructor = env.signature->addFreshFunction(1, "proj");
      env.signature->getFunction(destructor)->setType(new FunctionType({ tupleSort }, projSort));
    }
    destructors[i] = destructor;
  }

  Shell::TermAlgebraConstructor* constructor = new Shell::TermAlgebraConstructor(functor, destructors);

  Shell::TermAlgebraConstructor* constructors[] = { constructor };
  env.signature->addTermAlgebra(new Shell::TermAlgebra(tupleSort, 1, constructors, false));
}

bool Theory::isInterpretedConstant(unsigned func)
{
  CALL("Theory::isInterpretedConstant");

  if (func>=Term::SPECIAL_FUNCTOR_LOWER_BOUND) {
    return false;
  }

  return env.signature->getFunction(func)->interpreted() && env.signature->functionArity(func)==0;
}

/**
 * Return true iff @b t is an interpreted constant
 */
bool Theory::isInterpretedConstant(Term* t)
{
  CALL("Theory::isInterpretedConstant(Term*)");

  if (t->isSpecial()) { return false; }

  return t->arity()==0 && env.signature->getFunction(t->functor())->interpreted();
}

/**
 * Return true iff @b t is an interpreted constant
 */
bool Theory::isInterpretedConstant(TermList t)
{
  CALL("Theory::isInterpretedConstant(TermList)");

  return t.isTerm() && isInterpretedConstant(t.term());
}

/**
 * Return true iff @b t is a constant with a numerical interpretation
 */
bool Theory::isInterpretedNumber(TermList t)
{
  CALL("Theory::isInterpretedNumber(TermList)");

  return isInterpretedConstant(t) && env.signature->getFunction(t.term()->functor())->numericConstant();
}

/**
 * Return true iff @b pred is an interpreted predicate
 */
bool Theory::isInterpretedPredicate(unsigned pred)
{
  CALL("Theory::isInterpretedPredicate(unsigned)");

  return env.signature->getPredicate(pred)->interpreted();
}

/**
 * Return true iff @b lit has an interpreted predicate
 */
bool Theory::isInterpretedPredicate(Literal* lit)
{
  CALL("Theory::isInterpretedPredicate");

  if(lit->isEquality()){
    return SortHelper::getEqualityArgumentSort(lit)!=Sorts::SRT_DEFAULT;
  }

  return isInterpretedPredicate(lit->functor());
}

/**
 * Return true iff @b lit has an interpreted predicate interpreted
 * as @b itp
 */
bool Theory::isInterpretedPredicate(Literal* lit, Interpretation itp)
{
  CALL("Theory::isInterpretedPredicate/2");

  return env.signature->getPredicate(lit->functor())->interpreted() &&
      interpretPredicate(lit)==itp;
}

bool Theory::isInterpretedFunction(unsigned func)
{
  CALL("Theory::isInterpretedFunction(unsigned)");

  if (func>=Term::SPECIAL_FUNCTOR_LOWER_BOUND) {
    return false;
  }

  return env.signature->getFunction(func)->interpreted() && env.signature->functionArity(func)!=0;
}


/**
 * Return true iff @b t is an interpreted function
 */
bool Theory::isInterpretedFunction(Term* t)
{
  CALL("Theory::isInterpretedFunction(Term*)");

  return isInterpretedFunction(t->functor());
}

/**
 * Return true iff @b t is an interpreted function
 */
bool Theory::isInterpretedFunction(TermList t)
{
  CALL("Theory::isInterpretedFunction(TermList)");

  return t.isTerm() && isInterpretedFunction(t.term());
}

/**
 * Return true iff @b t is an interpreted function interpreted
 * as @b itp
 */
bool Theory::isInterpretedFunction(Term* t, Interpretation itp)
{
  CALL("Theory::isInterpretedFunction(Term*,Interpretation)");

  return isInterpretedFunction(t->functor()) &&
      interpretFunction(t)==itp;
}

/**
 * Return true iff @b t is an interpreted function interpreted
 * as @b itp
 */
bool Theory::isInterpretedFunction(TermList t, Interpretation itp)
{
  CALL("Theory::isInterpretedFunction(TermList,Interpretation)");

  return t.isTerm() && isInterpretedFunction(t.term(), itp);
}


Interpretation Theory::interpretFunction(unsigned func)
{
  CALL("Theory::interpretFunction");
  ASS(isInterpretedFunction(func));

  Signature::InterpretedSymbol* sym =
      static_cast<Signature::InterpretedSymbol*>(env.signature->getFunction(func));

  return sym->getInterpretation();
}

/**
 * Assuming @b t is an interpreted function, return its interpretation
 */
Interpretation Theory::interpretFunction(Term* t)
{
  CALL("Theory::interpretFunction");
  ASS(isInterpretedFunction(t));

  return interpretFunction(t->functor());
}

/**
 * Assuming @b t is an interpreted function, return its interpretation
 */
Interpretation Theory::interpretFunction(TermList t)
{
  CALL("Theory::interpretFunction");
  ASS(t.isTerm());

  return interpretFunction(t.term());
}

Interpretation Theory::interpretPredicate(unsigned pred)
{
  CALL("Theory::interpretPredicate");
  ASS(isInterpretedPredicate(pred));

  Signature::InterpretedSymbol* sym =
      static_cast<Signature::InterpretedSymbol*>(env.signature->getPredicate(pred));

  return sym->getInterpretation();
}

/**
 * Assuming @b lit has an interpreted predicate, return its interpretation
 */
Interpretation Theory::interpretPredicate(Literal* lit)
{
  CALL("Theory::interpretFunction");
  ASS(isInterpretedPredicate(lit));

  return interpretPredicate(lit->functor());
}

/**
 * Try to interpret the term as an integer constant. If it is an
 * integer constant, return true and save the constant in @c res, otherwise
 * return false.
 * @since 04/05/2013 Manchester
 * @author Andrei Voronkov
 */
bool Theory::tryInterpretConstant(const Term* t, IntegerConstantType& res)
{
  CALL("Theory::tryInterpretConstant(Term*,IntegerConstantType)");

  if (t->arity() != 0 || t->isSpecial()) {
    return false;
  }
  unsigned func = t->functor();
  Signature::Symbol* sym = env.signature->getFunction(func);
  if (!sym->integerConstant()) {
    return false;
  }
  res = sym->integerValue();
  return true;
} // Theory::tryInterpretConstant

/**
 * Try to interpret the term as an rational constant. If it is an
 * rational constant, return true and save the constant in @c res, otherwise
 * return false.
 * @since 04/05/2013 Manchester
 * @author Andrei Voronkov
 */
bool Theory::tryInterpretConstant(const Term* t, RationalConstantType& res)
{
  CALL("Theory::tryInterpretConstant(Term*,RationalConstantType)");

  if (t->arity() != 0 || t->isSpecial()) {
    return false;
  }
  unsigned func = t->functor();
  Signature::Symbol* sym = env.signature->getFunction(func);
  if (!sym->rationalConstant()) {
    return false;
  }
  res = sym->rationalValue();
  return true;
} // Theory::tryInterpretConstant

/**
 * Try to interpret the term as a real constant. If it is an
 * real constant, return true and save the constant in @c res, otherwise
 * return false.
 * @since 04/05/2013 Manchester
 * @author Andrei Voronkov
 */
bool Theory::tryInterpretConstant(const Term* t, RealConstantType& res)
{
  CALL("Theory::tryInterpretConstant(Term*,RealConstantType)");

  if (t->arity() != 0 || t->isSpecial()) {
    return false;
  }
  unsigned func = t->functor();
  Signature::Symbol* sym = env.signature->getFunction(func);
  if (!sym->realConstant()) {
    return false;
  }
  res = sym->realValue();
  return true;
} // // Theory::tryInterpretConstant

Term* Theory::representConstant(const IntegerConstantType& num)
{
  CALL("Theory::representConstant(const IntegerConstantType&)");

  unsigned func = env.signature->addIntegerConstant(num);
  return Term::create(func, 0, 0);
}

Term* Theory::representConstant(const RationalConstantType& num)
{
  CALL("Theory::representConstant(const RationalConstantType&)");

  unsigned func = env.signature->addRationalConstant(num);
  return Term::create(func, 0, 0);
}

Term* Theory::representConstant(const RealConstantType& num)
{
  CALL("Theory::representConstant(const RealConstantType&)");

  unsigned func = env.signature->addRealConstant(num);
  return Term::create(func, 0, 0);
}

Term* Theory::representIntegerConstant(vstring str)
{
  CALL("Theory::representIntegerConstant");

  try {
    return Theory::instance()->representConstant(IntegerConstantType(str));
  }
  catch(ArithmeticException&) {
    NOT_IMPLEMENTED;
//    bool added;
//    unsigned fnNum = env.signature->addFunction(str, 0, added);
//    if (added) {
//      env.signature->getFunction(fnNum)->setType(new FunctionType(Sorts::SRT_INTEGER));
//      env.signature->addToDistinctGroup(fnNum, Signature::INTEGER_DISTINCT_GROUP);
//    }
//    else {
//      ASS(env.signature->getFunction(fnNum))
//    }
  }
}

Term* Theory::representRealConstant(vstring str)
{
  CALL("Theory::representRealConstant");
  try {
    return Theory::instance()->representConstant(RealConstantType(str));
  } catch(ArithmeticException&) {
    NOT_IMPLEMENTED;
  }
}

/**
 * Return term containing unary function interpreted as @b itp with
 * @b arg as its first argument
 */
Term* Theory::fun1(Interpretation itp, TermList arg)
{
  CALL("Theory::fun1");
  ASS(isFunction(itp));
  ASS_EQ(getArity(itp), 1);

  unsigned fn=theory->getFnNum(itp);
  return Term::create(fn, 1, &arg);
}

/**
 * Return term containing binary function interpreted as @b itp with
 * arguments @b arg1 and @b arg2
 */
Term* Theory::fun2(Interpretation itp, TermList arg1, TermList arg2)
{
  CALL("Theory::fun2");
  ASS(isFunction(itp));
  ASS_EQ(getArity(itp), 2);

  TermList args[]= {arg1, arg2};

  unsigned fn=theory->getFnNum(itp);
  return Term::create(fn, 2, args);
}


/**
* Return term containing trenary function interpreted as @b itp with
* arguments @b arg1 ,  @b arg2, @b arg3
*/
Term* Theory::fun3(Interpretation itp, TermList arg1, TermList arg2, TermList arg3)
    {
        CALL("Theory::fun3");
        ASS(isFunction(itp));
        ASS_EQ(getArity(itp), 3);

        TermList args[]= {arg1, arg2, arg3};

        unsigned fn=theory->getFnNum(itp);
        return Term::create(fn, 3, args);
    }




/**
 * Return literal containing binary predicate interpreted as @b itp with
 * arguments @b arg1 and @b arg2
 *
 * Equality cannot be created using this function, Term::createEquality has to be used.
 */
Literal* Theory::pred2(Interpretation itp, bool polarity, TermList arg1, TermList arg2)
{
  CALL("Theory::fun2");
  ASS(!isFunction(itp));
  ASS_EQ(getArity(itp), 2);
  ASS_NEQ(itp,EQUAL);

  TermList args[]= {arg1, arg2};

  unsigned pred=theory->getPredNum(itp);
  return Literal::create(pred, 2, polarity, false, args);
}

/**
 * Return number of function that is intepreted as @b itp
 */
unsigned Theory::getFnNum(Interpretation itp)
{
  CALL("Theory::getFnNum");
  ASS(isFunction(itp));

  return env.signature->getInterpretingSymbol(itp);
}

/**
 * Return number of predicate that is intepreted as @b itp
 */
unsigned Theory::getPredNum(Interpretation itp)
{
  CALL("Theory::getPredNum");
  ASS(!isFunction(itp));

  return env.signature->getInterpretingSymbol(itp);
}

/**
 * Register that a predicate pred with a given polarity has the given
 * template. See tryGetInterpretedLaTeXName for explanation of templates
 */
void Theory::registerLaTeXPredName(unsigned pred, bool polarity, vstring temp)
{
  CALL("Theory::registerPredLaTeXName");
  if(polarity){
    _predLaTeXnamesPos.insert(pred,temp);
  }else{
    _predLaTeXnamesNeg.insert(pred,temp);
  }
}
/**
 * Register that a function has the given template
 * See tryGetInterpretedLaTeXName for explanation of templates
 */
void Theory::registerLaTeXFuncName(unsigned func, vstring temp)
{
  CALL("Theory::registerFuncLaTeXName");
  _funcLaTeXnames.insert(func,temp);
}

/**
 * We try and get a LaTeX special name for an interpeted function/predicate.
 * Note: the functions may not necessarily be interpreted in the sense that we treat
 *       them as interpreted in Vampire. They are just called that here as we have an
 *       interpretation for them. So we can have LaTeX symbols for any predicate or
 *       function if they are recorded e.g. skolem functions are recorded in Signature.
 *
 * See Shell/LaTeX for usage.
 *
 * Polarity only makes sense if pred=true
 *
 * First we look in the recorded templates and if one is not found we fallback to the
 * default templates for known interprted functions. Note that in most cases the known
 * interpreted functions will use these defaults.
 *
 * A template is a string with "ai" representing parameter i. These will be
 * replaced by the actual parameters elsewhere. For example, the template for
 * not greater or equal to is "a0 \not \geq a1"
 */
vstring Theory::tryGetInterpretedLaTeXName(unsigned func, bool pred,bool polarity)
{
  CALL("Theory::tryGetInterpretedLaTeXName");

   //cout << "Get LaTeX for " << func << endl;

  // Used if no recorded template is found
  Interpretation i;

  if(pred){
    if(polarity){
      if(_predLaTeXnamesPos.find(func)){ return _predLaTeXnamesPos.get(func); }
      else if(_predLaTeXnamesNeg.find(func)){
        // If a negative record is found but no positive we negate it
        return "\neg ("+_predLaTeXnamesNeg.get(func)+")";
      }
    }
    else{
      if(_predLaTeXnamesNeg.find(func)){ return _predLaTeXnamesNeg.get(func); }
      else if(_predLaTeXnamesPos.find(func)){
        // If a positive record is found but no negative we negate it
        return "\neg ("+_predLaTeXnamesPos.get(func)+")";
      }
    }
    // We get here if no record is found for a predicate
    if(!isInterpretedPredicate(func)) return "";
    i = interpretPredicate(func);
  }
  else{
    if(_funcLaTeXnames.find(func)){ return _funcLaTeXnames.get(func); }
    // We get here if no record is found for a function
    if(!isInterpretedFunction(func)) return "";
    i = interpretFunction(func);
  }

  // There are some default templates
  // For predicates these include the notion of polarity
  vstring pol = polarity ? "" : " \\not ";

  //TODO do we want special symbols for quotient, remainder, floor, ceiling, truncate, round?

  switch(i){
  case INT_SUCCESSOR: return "a0++";
  case INT_UNARY_MINUS:
  case RAT_UNARY_MINUS:
  case REAL_UNARY_MINUS: return "-a0";

  case EQUAL:return "a0 "+pol+"= a1";

  case INT_GREATER: return "a0 "+pol+"> a1";
  case INT_GREATER_EQUAL: return "a0 "+pol+"\\geq a1";
  case INT_LESS: return "a0 "+pol+"< a1";
  case INT_LESS_EQUAL: return "a0 "+pol+"\\leq a1";
  case INT_DIVIDES: return "a0 "+pol+"\\| a1"; // check?

  case RAT_GREATER: return "a0 "+pol+"> a1";
  case RAT_GREATER_EQUAL: return "a0 "+pol+"\\geq a1";
  case RAT_LESS: return "a0 "+pol+"< a1";
  case RAT_LESS_EQUAL: return "a0 "+pol+"\\leq a1";

  case REAL_GREATER: return "a0 "+pol+"> a1";
  case REAL_GREATER_EQUAL: return "a0 "+pol+"\\geq a1";
  case REAL_LESS: return "a0 "+pol+"< a1";
  case REAL_LESS_EQUAL: return "a0 "+pol+"\\leq a1";

  case INT_PLUS: return "a0 + a1";
  case INT_MINUS: return "a0 - a1";
  case INT_MULTIPLY: return "a0 \\cdot a1";

  case RAT_PLUS: return "a0 + a1";
  case RAT_MINUS: return "a0 - a1";
  case RAT_MULTIPLY: return "a0 \\cdot a1";
  case RAT_QUOTIENT: return "a0 / a1";

  case REAL_PLUS: return "a0 + a1";
  case REAL_MINUS: return "a0 - a1";
  case REAL_MULTIPLY: return "a0 \\cdot a1";
  case REAL_QUOTIENT: return "a0 / a1";

  default: return "";
  }

  return "";

}

}
