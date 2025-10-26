
#include "signal.h"

boost::mutex pending::mutex;
std::map<boost::thread::id, std::list<std::function<void ()>>> pending::callbacks;

ThreadWaker* thread_waker = nullptr;

void pending::process()
{
        boost::mutex::scoped_lock lm(mutex);
	auto const thread_id = boost::this_thread::get_id();
        for (auto i: callbacks[thread_id]) {
                i();
        }
        callbacks[thread_id].clear();
}
