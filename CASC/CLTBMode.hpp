/**
 * @file CLTBMode.hpp
 * Defines class CLTBMode.
 */

#ifndef __CLTBMode__
#define __CLTBMode__

#include <utility>

#include "Forwards.hpp"

#include "Lib/DHSet.hpp"
#include "Lib/Portability.hpp"
#include "Lib/ScopedPtr.hpp"
#include "Lib/Stack.hpp"

#include "Lib/VString.hpp"

#include "Lib/Sys/SyncPipe.hpp"

#include "Kernel/Problem.hpp"

#include "Shell/Property.hpp"
#include "Shell/SineUtils.hpp"

namespace CASC {

using namespace std;
using namespace Lib;
using namespace Kernel;

#if COMPILER_MSVC

class CLTBMode
{
public:
  static void perform() { USER_ERROR("casc_ltb mode is not supported on Windows"); }
};

#else

class CLTBProblem;

class CLTBMode
{
public:
  static void perform();
private:
  void solveBatch(istream& batchFile);
  int readInput(istream& batchFile);
  static ostream& lineOutput();
  static ostream& coutLineOutput();
  void loadIncludes();

  typedef List<vstring> StringList;
  typedef Stack<vstring> StringStack;
  typedef pair<vstring,vstring> StringPair;
  typedef Stack<StringPair> StringPairStack;

  vstring category;
  /** per-problem time limit, in milliseconds */
  int _problemTimeLimit;
  /** true if question answers should be given */
  bool _questionAnswering;
  /** total time used by batches before this one, in milliseconds */
  int _timeUsedByPreviousBatches;

  /** files to be included */
  StringList* _theoryIncludes;

  /** The first vstring in the pair is problem file, the second
   * one is output file. The problemFiles[0] is the first
   * problem that should be attempted. */
  StringPairStack problemFiles;

  ScopedPtr<Problem> _baseProblem;

  friend class CLTBProblem;
};


class CLTBProblem
{
public:
  CLTBProblem(CLTBMode* parent, vstring problemFile, vstring outFile);

  void searchForProof(int terminationTime) __attribute__((noreturn));
private:
  typedef Set<vstring> StrategySet;
  typedef Stack<vstring> Schedule;
  bool runSchedule(Schedule&,StrategySet& remember,bool fallback,int terminationTime);
  unsigned getSliceTime(vstring sliceCode,vstring& chopped);

  void performStrategy(int terminationTime);
  void waitForChildAndExitWhenProofFound();
  void exitOnNoSuccess() __attribute__((noreturn));

  static ofstream* writerFileStream;
  static void terminatingSignalHandler(int sigNum) __attribute__((noreturn));
  void runWriterChild() __attribute__((noreturn));
  void runSlice(vstring slice, unsigned milliseconds) __attribute__((noreturn));
  void runSlice(Options& strategyOpt) __attribute__((noreturn));

  static vstring problemFinishedString;

#if VDEBUG
  DHSet<pid_t> childIds;
#endif

  CLTBMode* parent;
  vstring problemFile;
  vstring outFile;

  /**
   * Problem that is being solved.
   *
   * This is just a reference to parent's @c baseProblem object into which we
   * add problem-specific axioms in the @c searchForProof() function. We can do this,
   * because in the current process this child object is the only one that
   * will be using the problem object.
   */
  Problem& prb;

  pid_t writerChildPid;
  /** pipe for collecting the output from children */
  Lib::Sys::SyncPipe childOutputPipe;
};

#endif //!COMPILER_MSVC

}

#endif // __CLTBMode__
