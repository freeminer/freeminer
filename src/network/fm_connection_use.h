#pragma once

#include "config.h"


#if USE_MULTI
#include "fm_connection_multi.h"
namespace con_use
{
using namespace con_multi;
}
#elif USE_SCTP
#include "fm_connection_sctp.h"
namespace con_use
{
using namespace con_sctp;
}
#elif USE_ENET
#include "fm_connection_enet.h"
namespace con_use
{
using namespace con_enet;
}
#else
#include "connection.h"
namespace con_use
{
using namespace con;
}
#endif
