#include "node.h"

_Thread_local MxcNodeContext nodeContext;

void* mxcUpdateNode(void* pArg) {
  nodeContext = *(MxcNodeContext*)pArg;
  context = *nodeContext.pContext;
  nodeContext.initFunc(nodeContext.pInitArg);
  while (isRunning) {
    nodeContext.updateFunc();
  }
  return 0;
}


void mxcInterProcessImportNode(void* pParam){

}
