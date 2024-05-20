#include "node.h"

size_t                 MXC_COMP_NODE_HANDLE_COUNT = 0;
MxcNodeContext         MXC_COMP_NODE_CONTEXT[MXC_NODE_CAPACITY] = {};
MxcNodeContextHot      MXC_COMP_NODE_CONTEXT_HOT[MXC_NODE_CAPACITY] = {};

volatile uint64_t      MXC_NODE_CURRENT[MXC_NODE_CAPACITY] = {};
volatile uint64_t      MXC_NODE_SIGNAL[MXC_NODE_CAPACITY] = {};

void mxcInterProcessImportNode(void* pParam) {
}
