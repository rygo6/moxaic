#include "node.h"

size_t               MXC_NODE_COUNT = 0;
MxcNodeContext       MXC_NODE[MXC_NODE_CAPACITY] = {};
MxcNodeContextShared MXC_NODE_SHARED[MXC_NODE_CAPACITY] = {};

void mxcInterProcessImportNode(void* pParam) {
}
