#include "node.h"

size_t            MXC_NODE_HANDLE_COUNT = 0;
MxcNodeContext    MXC_NODE_CONTEXT[MXC_NODE_CAPACITY] = {};
MxcNodeContextHot MXC_NODE_CONTEXT_HOT[MXC_NODE_CAPACITY] = {};

void mxcInterProcessImportNode(void* pParam) {
}
