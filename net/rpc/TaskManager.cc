#include <net/rpc/TaskManager.h>

namespace thefox {
namespace detail {

void defaultOobCallback(const TcpConnectionPtr &sender, 
							const std::string &type, 
							const gpb::Message *message,
							const Timestamp &receiveTime)
{}

}
}

using namespace thefox;

TaskManager::TaskManager(const CallCallback &callCb, const ReplyCallback &replyCb)
	: _dispatcher(detail::defaultOobCallback)
	, _callCallback(callCb)
	, _replyCallback(replyCb)
	, _started(true)
	
{
	_msgLoopThread.reset(new Thread(std::bind(&TaskManager::loop, this), "mqmanager.loop"));
	_msgLoopThread->start();
}

TaskManager::~TaskManager()
{
	_started = false;
	_msgLoopThread->stop();
}

void TaskManager::pushBox(const TcpConnectionPtr &sender, const Timestamp &receiveTime, const BoxPtr &msg)
{
	MutexLockGuard lock(_mutex);
	TaskPtr box(new Task(sender,receiveTime, msg));
	_tasks.push(box);
	_event.set();
}

TaskPtr TaskManager::popBox()
{
	MutexLockGuard lock(_mutex);
	TaskPtr msg(_tasks.front());
	_tasks.pop();
	return msg;
}

void TaskManager::setDefaultOobCallback(const OnewayCallback &cb)
{
	_dispatcher.setDefaultOobCallback(cb);
}

void TaskManager::registerOobCallback(const std::string &type, const OnewayCallback &cb)
{
	_dispatcher.registerOobCallback(type, cb);
}

void TaskManager::loop()
{
	while (_started) {
		TaskPtr msg;
		while (_event.wait()) {
			{
			MutexLockGuard lock(_mutex);
			if (_tasks.empty()) {
				_event.reset();
				continue;
			}
			}

			if ((msg = popBox()) != NULL) {
				if (msg->hasCall())
					_callCallback(msg->sender(), msg->call(), msg->time());
				if (msg->hasReply())
					_replyCallback(msg->sender(), msg->reply(), msg->time());
				if (msg->hasOnewayMessage())
					_dispatcher.onMessage(msg->sender(), msg->oneway(), msg->time());
			}
				
		}
	}
}
