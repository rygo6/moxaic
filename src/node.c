#include "node.h"

_Thread_local const MxcNodeContext* pNodeContext = NULL;

void* mxcUpdateNode(void* pArg) {
  pNodeContext = (MxcNodeContext*)pArg;
  pContext = pNodeContext->pContext;
  pNodeContext->createFunc(pNodeContext->pInitArg);
  while (isRunning) {
    pNodeContext->updateFunc();
  }
  return 0;
}


void mxcInterProcessImportNode(void* pParam){

}
