/**
 * @file BFNTMainLoop.cpp
 * Implements class BFNTMainLoop.
 */

#include <cerrno>
#include <csignal>

#include "Lib/Portability.hpp"

#if !COMPILER_MSVC

#include <sys/types.h>
#include <sys/wait.h>

#endif


#include "Lib/Environment.hpp"
#include "Lib/Int.hpp"
#include "Lib/Metaiterators.hpp"
#include "Lib/ScopedPtr.hpp"
#include "Lib/System.hpp"
#include "Lib/Timer.hpp"

#include "Lib/Sys/Multiprocessing.hpp"

#include "Kernel/Clause.hpp"
#include "Kernel/Problem.hpp"
#include "Kernel/Unit.hpp"

#include "Statistics.hpp"

#include "BFNTMainLoop.hpp"

#undef LOGGING
#define LOGGING 0

#define BFNT_CHILD_RESULT_SAT 0
#define BFNT_CHILD_RESULT_UNSAT 6
#define BFNT_CHILD_RESULT_UNKNOWN 7

namespace Shell
{
#if COMPILER_MSVC

BFNTMainLoop::BFNTMainLoop(Problem& prb, const Options& opt)
: MainLoop(prb, opt)
{}

void BFNTMainLoop::init()
{
  CALL("BFNTMainLoop::init");

  USER_ERROR("BFNT not supported on Windows");
}

MainLoopResult BFNTMainLoop::runImpl()
{
  CALL("BFNTMainLoop::runImpl");

  USER_ERROR("BFNT not supported on Windows");
}

#else

BFNTMainLoop::BFNTMainLoop(Problem& prb, const Options& opt)
: MainLoop(prb, opt),
  _childOpts(opt),
  _bfnt(prb.getProperty())
{
  CALL("BFNTMainLoop::BFNTMainLoop");

  //this is important, otherwise we'd start creating new and new processes
  // -- the child process would also run BFNT therefore also attemps to run
  //BFNT-running children.
  _childOpts.setBfnt(false);
}


void BFNTMainLoop::init()
{
  CALL("BFNTMainLoop::init");

  //Putting problem units into the BFNT convertor here may result into
  //one clause appearing in multiple Problem objects. In _prb and then in
  //child problems created by the spawed processes. Normally we wouldn't
  //want this to happen, but in the _prb object we do not use the clauses
  //any more after this point, and the child problems are isolated from each
  //other in different processes.
  _bfnt.apply(_prb.units());
}

/**
 * Run the child process that does proving on the flattenned problem
 *
 * Result statuses of the process:
 * BFNT_CHILD_RESULT_SAT
 * BFNT_CHILD_RESULT_UNSAT
 * BFNT_CHILD_RESULT_UNKNOWN
 */
void BFNTMainLoop::runChild(size_t modelSize)
{
  CALL("BFNTMainLoop::runChild");

  ScopedPtr<Problem> childPrb(_bfnt.createProblem(modelSize));

#if LOGGING
  UnitList::Iterator puit(childPrb->units());
  while(puit.hasNext()) {
    Unit* u = puit.next();
    LOG("Flattenned unit: "<<u->toString());
  }
#endif

  ScopedPtr<MainLoop> childMainLoop(MainLoop::createFromOptions(*childPrb, _childOpts));
  MainLoopResult innerRes = childMainLoop->run();
  innerRes.updateStatistics();

  LOG("Child termination reason: "
      << ((innerRes.terminationReason==Statistics::SATISFIABLE) ? "Satisfiable" :
	  (innerRes.terminationReason==Statistics::REFUTATION) ? "Unsatisfiable" : "Unknown") );
#if LOGGING
  if(env.statistics->model!="") {
    LOG("Model: "<<endl<<env.statistics->model);
  }
#endif

  if(env.options->mode()!=Options::MODE_SPIDER) {
    if(innerRes.terminationReason==Statistics::SATISFIABLE || innerRes.terminationReason==Statistics::TIME_LIMIT) {
      env.beginOutput();
      env.statistics->print(env.out());
      env.endOutput();
    }
  }

  switch(innerRes.terminationReason) {
  case Statistics::SATISFIABLE:
    exit(BFNT_CHILD_RESULT_SAT);
  case Statistics::REFUTATION:
    exit(BFNT_CHILD_RESULT_UNSAT);
  default:
    exit(BFNT_CHILD_RESULT_UNKNOWN);
  }
  exit(BFNT_CHILD_RESULT_UNKNOWN);
}


MainLoopResult BFNTMainLoop::spawnChild(size_t modelSize)
{
  CALL("BFNTMainLoop::spawnChild");

  pid_t childPid=Multiprocessing::instance()->fork();

  if(!childPid) {
    runChild(modelSize);
    ASSERTION_VIOLATION;
  }

  System::ignoreSIGINT();

  int status;
  errno=0;
  pid_t res=waitpid(childPid, &status, 0);
  if(res==-1) {
    SYSTEM_FAIL("Error in waiting for forked process.",errno);
  }

  System::heedSIGINT();

  Timer::syncClock();

  if(res!=childPid) {
    INVALID_OPERATION("Invalid waitpid return value: "+Int::toString(res)+"  pid of forked Vampire: "+Int::toString(childPid));
  }

  ASS(!WIFSTOPPED(status));

  if( (WIFSIGNALED(status) && WTERMSIG(status)==SIGINT) ||
      (WIFEXITED(status) && WEXITSTATUS(status)==VAMP_RESULT_STATUS_SIGINT) )  {
    //if the forked Vampire was terminated by SIGINT (Ctrl+C), we also terminate
    //(3 is the return value for this case; see documentation for the
    //@b vampireReturnValue global variable)

    raise(SIGINT);
  }

  if(WIFEXITED(status)) {
    switch(WEXITSTATUS(status)) {
    case BFNT_CHILD_RESULT_SAT:
      return MainLoopResult(Statistics::SATISFIABLE);
    case BFNT_CHILD_RESULT_UNSAT:
      return MainLoopResult(Statistics::REFUTATION);

    case VAMP_RESULT_STATUS_OTHER_SIGNAL:
      INVALID_OPERATION("error in the child process");

    case BFNT_CHILD_RESULT_UNKNOWN:
    default: //under default will fall timeout
      return MainLoopResult(Statistics::UNKNOWN);
    }
  }
  else {
    return MainLoopResult(Statistics::UNKNOWN);
  }
}


MainLoopResult BFNTMainLoop::runImpl()
{
  CALL("BFNTMainLoop::runImpl");

  env.timer->makeChildrenIncluded();

  size_t modelSize = 1;
  for(;;) {
    Timer::syncClock();
    if(env.timeLimitReached()) { return MainLoopResult(Statistics::TIME_LIMIT); }
    LOG("Trying model size "<<modelSize);
    env.statistics->maxBFNTModelSize = modelSize;
    MainLoopResult childResult = spawnChild(modelSize);

    if(childResult.terminationReason == Statistics::SATISFIABLE) {
      return childResult;
    }

    modelSize++;
  }
}
#endif

}