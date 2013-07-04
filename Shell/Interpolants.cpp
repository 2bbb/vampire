/**
 * @file Interpolants.cpp
 * Implements class Interpolants.
 */

#include "Lib/DHMap.hpp"
#include "Lib/Stack.hpp"

#include "Kernel/Clause.hpp"
#include "Kernel/ColorHelper.hpp"
#include "Kernel/Formula.hpp"
#include "Kernel/Inference.hpp"
#include "Kernel/InferenceStore.hpp"
#include "Kernel/SubstHelper.hpp"
#include "Kernel/Term.hpp"
#include "Kernel/Unit.hpp"

#include "Flattening.hpp"
#include "SimplifyFalseTrue.hpp"

#include "Interpolants.hpp"



/** surprising colors occur when a clause which is a consequence of transparent clauses, is colored */
#define ALLOW_SURPRISING_COLORS 1

namespace Shell
{

using namespace Lib;
using namespace Kernel;

typedef pair<Formula*, Formula*> UIPair; //pair of unit U and the U-interpolant
typedef List<UIPair> UIPairList;

VirtualIterator<UnitSpec> Interpolants::getParents(UnitSpec u)
{
  CALL("Interpolants::getParents");

  if(!_slicedOff) {
    return InferenceStore::instance()->getParents(u);
  }

  static Stack<UnitSpec> toDo;
  static Stack<UnitSpec> parents;

  toDo.reset();
  parents.reset();

  for(;;) {
    VirtualIterator<UnitSpec> pit = InferenceStore::instance()->getParents(u);
    while(pit.hasNext()) {
      UnitSpec par = pit.next();
      if(_slicedOff->find(par)) {
	toDo.push(par);
      }
      else {
	parents.push(par);
      }
    }
    if(toDo.isEmpty()) {
      break;
    }
    u = toDo.pop();
  }

  return getPersistentIterator(Stack<UnitSpec>::BottomFirstIterator(parents));
}

struct ItemState
{
  ItemState() {}

  ItemState(UnitSpec us) : parCnt(0), inheritedColor(COLOR_TRANSPARENT), interpolant(0),
      leftInts(0), rightInts(0), processed(false), _us(us)
  {
    CALL("ItemState::ItemState");
    _usColor = us.unit()->getColor();
  }

  void destroy()
  {
    CALL("ItemState::destroy");

    leftInts->destroy();
    rightInts->destroy();
  }

  UnitSpec us() const { return _us; }
  Color usColor() const { return _usColor; }
  /** Parents that remain to be traversed
   *
   * Parents in the sense of inferencing, but children in the sense of DFS traversal */
  VirtualIterator<UnitSpec> pars;
  /** number of parents */
  int parCnt;
  /** Color of premise formulas, or the declared color for input formulas */
  Color inheritedColor;
  /** If non-zero, interpolant of the current formula. */
  Formula* interpolant;
  /** Left interpolants of parent formulas */
  List<UIPair>* leftInts;
  /** Right interpolants of parent formulas */
  List<UIPair>* rightInts;
  /** This state was processed, and if it should have its invarient generated,
   * it was generated */
  bool processed;
private:
  /** The current formula */
  UnitSpec _us;
  Color _usColor;
};

Comparison compareUIP(const UIPair& a, const UIPair& b)
{
  CALL("compareUIP");

  Comparison res = Int::compare(a.first, b.first);
  if(res==EQUAL) {
    res = Int::compare(a.second, b.second);
  }
  return res;
}

/**
 * Assuming arguments are lists ordered by the @c compareUIP() function,
 * add non-destructively new elements from @c src into @c tgt.
 */
void mergeCopy(UIPairList*& tgt, UIPairList* src)
{
  CALL("mergeCopy");
  if(!tgt) {
    tgt = src->copy();
    return;
  }

  UIPairList** curr = &tgt;
  UIPairList::Iterator sit(src);
  while(sit.hasNext()) {
    UIPair spl = sit.next();
  retry_curr:
    if(*curr) {
      Comparison cmp = compareUIP((*curr)->head(), spl);
      if(cmp!=GREATER) {
	curr = (*curr)->tailPtr();
	if(cmp==EQUAL) {
	  continue;
	}
	else {
	  goto retry_curr;
	}
      }
    }
    *curr = new UIPairList(spl, *curr);
    curr = (*curr)->tailPtr();
  }
}

void generateInterpolant(ItemState& st);

Formula* Interpolants::getInterpolant(Unit* unit)
{
  CALL("Interpolants::getInterpolant");

  typedef DHMap<UnitSpec,ItemState> ProcMap;
  ProcMap processed;

  Stack<ItemState> sts;

  UnitSpec curr=UnitSpec(unit);

  Formula* resultInterpolant = 0;

  int ctr=0;

  for(;;) {
    ItemState st;

    if(processed.find(curr)) {
      st = processed.get(curr);
      ASS(st.us()==curr);
      ASS(st.processed);
      ASS(!st.pars.hasNext());
    }
    else {
      st = ItemState(curr);
      st.pars = getParents(curr);
    }

    if(curr.unit()->inheritedColor()!=COLOR_INVALID) {
      //set premise-color information for input clauses
      st.inheritedColor=ColorHelper::combine(curr.unit()->inheritedColor(), st.usColor());
      ASS_NEQ(st.inheritedColor, COLOR_INVALID);
    }
#if ALLOW_SURPRISING_COLORS
    else {
      //we set inherited color to the color of the unit.
      //in the case of clause being conclusion of
      //transparent parents, the assigned color should be
      //transparent as well, but there are some corner cases
      //coming from proof transformations which can yield us
      //a colored clause in such case (when the colored premise
      //was removed by the transformation).
      st.inheritedColor=st.usColor();
    }
#else
    else if(!st.processed && !st.pars.hasNext()) {
      //we have unit without any parents. This case is reserved for
      //units introduced by some naming. In this case we need to set
      //the inherited color to the color of the unit.
      st.inheritedColor=st.usColor();
    }
#endif

    if(sts.isNonEmpty()) {
      //update premise color information in the level above
      ItemState& pst=sts.top(); //parent state
      pst.parCnt++;
      if(pst.inheritedColor==COLOR_TRANSPARENT) {
        pst.inheritedColor=st.usColor();
      }
#if VDEBUG
      else {
        Color clr=st.usColor();
        ASS_REP2(pst.inheritedColor==clr || clr==COLOR_TRANSPARENT, pst.us().toString(), curr.toString());
      }
      ASS_EQ(curr.unit()->getColor(),st.usColor());
#endif
    }

    sts.push(st);

    for(;;) {
      if(sts.top().pars.hasNext()) {
        curr=sts.top().pars.next();
        break;
      }
      //we're done with all children, now we can process what we've gathered
      st=sts.pop();
      ctr++;
      Color color = st.usColor();
      if(!st.processed) {
	if(st.inheritedColor!=color || sts.isEmpty()) {
	  //we either have a transparent clause justified by A or B, or the refutation
//	  ASS_EQ(color,COLOR_TRANSPARENT);
      //cout<<"generate interpolant of transparent clause: "<<st.us().toString()<<"\n";
	  ASS_REP2(color==COLOR_TRANSPARENT, st.us().toString(), st.inheritedColor);
	  generateInterpolant(st);
	  LOG("itp_sub","intermediate interpolant for "<<st.us().toString()<<endl<<
	      "  "<<(*st.interpolant));
	}
	st.processed = true;
	processed.insert(st.us(), st);
      }
      
      if(sts.isNonEmpty()) {
	//pass interpolants to the level above
        if(color!=COLOR_LEFT) {
          mergeCopy(sts.top().leftInts, st.leftInts);
        } 
        if(color!=COLOR_RIGHT) {
          mergeCopy(sts.top().rightInts, st.rightInts);
        }
      } 
      else {
	//empty sts (so refutation) with clause st justified by A or B (st is false). 
	//interpolant was already generated in st.interpolant
	//we have now the interpolant for refutation
        resultInterpolant = st.interpolant;
        goto fin;
      }
    }
  }

fin:

  //clean-up
  ProcMap::Iterator destrIt(processed);
  while(destrIt.hasNext()) {
    destrIt.next().destroy();
  }

  //simplify the interpolant and exit
  return Flattening::flatten(SimplifyFalseTrue::simplify(resultInterpolant));
//  return resultInterpolant;
}

void generateInterpolant(ItemState& st)
{
  CALL("generateInterpolant");

  Unit* u=st.us().unit();
  Color color=st.usColor();
  ASS_EQ(color, COLOR_TRANSPARENT);

  Formula* interpolant;
  Formula* unitFormula=u->getFormula(st.us().prop());

  //cout	<<"\n unitFormula: "<<unitFormula->toString()<<"\n";

  if(st.parCnt) {
    //interpolants from refutation proof with at least one inference (there are premises, i.e. parents)
    FormulaList* conj=0;
    List<UIPair>* src= (st.inheritedColor==COLOR_LEFT) //source of relevant parent interpolants
        ? st.rightInts
        : st.leftInts;
    //construct the common part of the interpolant
    List<UIPair>::Iterator sit(src);
    while(sit.hasNext()) {
      UIPair uip=sit.next();
      FormulaList* disj=0;
      FormulaList::push(uip.first, disj);
      FormulaList::push(uip.second, disj);
      FormulaList::push(JunctionFormula::generalJunction(OR, disj), conj);
    }

    if(st.inheritedColor==COLOR_LEFT) {
      //u is justified by A
      FormulaList* innerConj=0;
      List<UIPair>::Iterator sit2(src);
      while(sit2.hasNext()) {
        UIPair uip=sit2.next();
        FormulaList::push(uip.first, innerConj);
      }
      FormulaList::push(new NegatedFormula(JunctionFormula::generalJunction(AND, innerConj)), conj);
    }
    else {
      //u is justified by B or a refutation
    }
    interpolant=JunctionFormula::generalJunction(AND, conj);
  }
  else {
    //trivial interpolants (when there are no premises)
    if(st.inheritedColor==COLOR_RIGHT) {
      interpolant=new NegatedFormula(unitFormula); //this is for TRUE interpolant if the unitFormula is False
    }
    else {
      //a formula coming from left or a refutation
      interpolant=unitFormula; //this is for FALSE interpolant if the unitFormula is False
    }
  }
  st.interpolant=interpolant;
//  cout<<"Unit "<<u->toString()
//	<<"\nto Formula "<<unitFormula->toString()
//	<<"\ninterpolant "<<interpolant->toString()<<endl;
  UIPair uip=make_pair(unitFormula, interpolant);
  if(st.inheritedColor==COLOR_LEFT) {
    st.leftInts->destroy();
    st.leftInts=0;
    List<UIPair>::push(uip,st.leftInts);
  }
  else if(st.inheritedColor==COLOR_RIGHT) {
    st.rightInts->destroy();
    st.rightInts=0;
    List<UIPair>::push(uip,st.rightInts);
  }
}

}
