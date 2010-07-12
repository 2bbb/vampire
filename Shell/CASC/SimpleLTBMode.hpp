/**
 * @file SimpleLTBMode.hpp
 * Defines class SimpleLTBMode.
 */

#ifndef __SimpleLTBMode__
#define __SimpleLTBMode__

#include <string>
#include <utility>

#include "Forwards.hpp"

#include "Lib/DHSet.hpp"
#include "Lib/Portability.hpp"
#include "Lib/Stack.hpp"

#include "Lib/Sys/SyncPipe.hpp"

#include "Shell/Property.hpp"
#include "Shell/SineUtils.hpp"

namespace Shell {
namespace CASC {

using namespace std;
using namespace Lib;

class SLTBProblem;

class SimpleLTBMode
{
public:
  void perform();
private:
  void readInput();

  typedef List<string> StringList;
  typedef Stack<string> StringStack;
  typedef pair<string,string> StringPair;
  typedef Stack<StringPair> StringPairStack;

  string category;
  int problemTimeLimit;
  int overallTimeLimit;

  /** The first string in the pair is problem file, the second
   * one is output file. The problemFiles[0] is the first
   * problem that should be attempted. */
  StringPairStack problemFiles;

  friend class SLTBProblem;
};


class SLTBProblem
{
public:
  SLTBProblem(SimpleLTBMode* parent, string problemFile, string outFile);

  void perform() __attribute__((noreturn));
private:

  void performStrategy();

  void waitForChildAndExitWhenProofFound();

  void exitOnNoSuccess() __attribute__((noreturn));

  bool runSchedule(const char** sliceCodes);

  static void terminatingSignalHandler(int sigNum) __attribute__((noreturn));
  void runWriterChild() __attribute__((noreturn));

  void runChild(string slice, unsigned ds) __attribute__((noreturn));
  void runChild(Options& opt) __attribute__((noreturn));

  unsigned getSliceTime(string sliceCode);

#if VDEBUG
  DHSet<pid_t> childIds;
#endif

  SimpleLTBMode* parent;
  string problemFile;
  string outFile;

  UnitList* probUnits;
  Property property;

  pid_t writerChildPid;
  //pipe for collecting the output from children
  SyncPipe childOutputPipe;
};

}
}

#endif // __SimpleLTBMode__