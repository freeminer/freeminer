#include "arnis_adapter.h"
#include "emerge.h"
namespace arnis
{
XZ::operator XZPoint()
{
	return XZPoint{X, Y};
}
}