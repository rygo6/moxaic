#include "node.h"

size_t               nodeCount = 0;
MxcNodeContext       nodes[MXC_NODE_CAPACITY] = {};
MxcNodeContextShared nodesShared[MXC_NODE_CAPACITY] = {};

void mxcInterProcessImportNode(void* pParam) {
}
