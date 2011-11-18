#include "svncpp\delta.hpp"
#include "svncpp\general.hpp"

namespace svn
{
namespace delta
{

svn_error_t *ApplyDeltaHandler::txdelta_window_handler(svn_txdelta_window_t *window, void *baton)
{
	ApplyDeltaHandler* handler = (ApplyDeltaHandler*)baton;
	if(!handler) return NULL;
	try
	{
		if(!window)
			delete handler;
		else
			handler->handleWindow(window);
	}
	catch(svn::ClientException& e){ return e.detach(); }
	return NULL;
}

}//delta
}//svn