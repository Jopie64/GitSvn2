#pragma once
#include "svn_delta.h"

namespace svn
{
namespace delta
{

class ApplyDeltaHandler
{
public:
	static svn_error_t* txdelta_window_handler(svn_txdelta_window_t *window, void *baton);

	virtual void handleWindow(svn_txdelta_window_t *window)=0;

};

}//delta
}//svn
