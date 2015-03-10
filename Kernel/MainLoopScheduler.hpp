/**
 * @file MainLoopScheduler.hpp
 *
 * @since 7 May 2014
 * @author dmitry
 */

#ifndef __MainLoopScheduler__
#define __MainLoopScheduler__

#include <cstddef>
#if VDEBUG
#include <iostream>
#endif//VDEBUG
#include <queue>

#include "Kernel/MainLoopFwd.hpp"
#include "Kernel/MainLoopContext.hpp"
#include "Kernel/ProblemFwd.hpp"

#include "Lib/STLAllocator.hpp"

#include "Shell/OptionsList.hpp"

//namespace Shell {

//class OptionsList;

//}

namespace Kernel {

//class MainLoopContext;
//class MainLoopResult;
//class Problem;

//template< typename _Container = std::vector< MainLoopContext* > >
class MainLoopScheduler {
public:
        CLASS_NAME(MainLoopScheduler);
        USE_ALLOCATOR(MainLoopScheduler);

	MainLoopScheduler(Problem& prb, std::size_t capacity);
	MainLoopScheduler(Problem& prb, Shell::OptionsList& opts, std::size_t capacity);
	MainLoopScheduler(Problem& prb, Shell::OptionsList& opts);

	~MainLoopScheduler();

	MainLoopResult run();
	//static MainLoopScheduler* createFromOptions(Problem& prb, OptionsList* opts);

//	static ConcurrentMainLoop* getCurrentMainLoop() {
//		return MainLoopContext::currentContext -> getMainLoop();
//	}

//	static MainLoopContext* context() {
//		return MainLoopContext::currentContext;
//	}
// it won't compile in release mode if some of these are left in!
#if VDEBUG
	static std::ostream& log(){
		std::cout << MainLoopContext::currentContext->_id << ": ";
		return std::cout;
	}
#endif //VDEBUG

	inline
	void addStrategy(Shell::Options& opt){
#if VDEBUG
                cout << "Adding a new strategy, its priority is " << opt.getMultiProofAttemptPriority() << endl;
#endif //VDEBUG
		optionsQueue.push(&opt);
	}

	inline
	void addStrategies(Shell::OptionsList& opts){
		Shell::OptionsList::Iterator i(opts);
	    while(i.hasNext()){
			addStrategy(i.next());
	    }
	}

	inline
	const std::size_t numberOfAliveContexts() const {
		return _contextCounter;
	}

	inline
	const std::size_t numberOfContexts() const {
		return _capacity;
	}

    static const MainLoopScheduler* scheduler;

protected:

private:

	Problem& _prb;
	std::size_t _capacity;
	std::size_t _contextCounter;
	MainLoopContext** _mlcl;

	class CompareOptions{
		public:

		//Reversed priority order: 0 - TOP priority
		inline
	    	bool operator()(Shell::Options* lhs, Shell::Options* rhs) const {
	    		return (lhs -> getMultiProofAttemptPriority() > rhs ->getMultiProofAttemptPriority());
	    	}
	};

	std::priority_queue<Shell::Options*, std::vector<Shell::Options*, Lib::STLAllocator<Shell::Options*>>, CompareOptions> optionsQueue;

	static MainLoopContext* createContext(Problem& prb, Shell::Options& opt);

	inline
	void deleteContext(const std::size_t k){
		CALL("MainLoopScheduler::deleteContext");
		ASS(_mlcl[k]);
		delete _mlcl[k];
		_mlcl[k] = 0;
		_contextCounter--;
		ASS_GE(_contextCounter,0);
		ASS_LE(_contextCounter,_capacity);
	}
	void clearAll();

	inline
	void addContext(const std::size_t k){
		CALL("MainLoopScheduler::addContext");
		ASS_L(k,_capacity);
		ASS(!optionsQueue.empty());
		_mlcl[k] = createContext(_prb, /*const_cast<Shell::Options&>*/(*optionsQueue.top()));
		ASS(_mlcl[k]);
		optionsQueue.pop();
		_contextCounter++;
		ASS_LE(_contextCounter,_capacity);
	}

	inline
	bool exausted() const{
		return (_contextCounter == 0) && optionsQueue.empty();
	}

	inline
	void contextStep(const std::size_t k){
		CALL("MainLoopScheduler::contextStep");
		ASS_L(k,_capacity);
		_mlcl[k] -> doStep(_maxTimeSlice);
//		timeSliceMagic(k); //TODO: [dmitry] Perhaps, we would return to this scheduling scheme
//#if VDEBUG
//		std::cout << "Context step finished." << std::endl;
//#endif
		const unsigned int avgTime = _mlcl[k] -> averageTimeSlice();
//#if VDEBUG
//		std::cout << "#Context step finished." << std::endl;
//#endif
		if(_minTimeSlice > avgTime) _minTimeSlice = avgTime;
//#if VDEBUG
//		std::cout << "##Context step finished." << std::endl;
//#endif
	}

/*	inline
	void timeSliceMagic(const std::size_t k){
		CALL("MainLoopScheduler::timeSliceMagic");

		//TODO: [dmitry] More nicer slicing scheme needed: some strategies do one derivation step too long
		const unsigned int timeSlice = _mlcl[k] -> averageTimeSlice();
		if(_maxTimeSlice <= timeSlice) {
			_maxTimeSlice = timeSlice;
			_nmts = 0;
		}else{
			_nmts++;
			if(_nmts >= _capacity){
				_maxTimeSlice /= 2;
				_nmts = 0;
			}
		}

	}*/

	unsigned int _maxTimeSlice, _minTimeSlice, /*_nmts,*/ _cycleCount;
	static const unsigned int _cycleThreshold;
};

} /* namespace Kernel */

#endif /* __ConcurrentMainLoop__ */