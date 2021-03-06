//===- CallGraphUpdater.cpp - A (lazy) call graph update helper -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
/// \file
///
/// This file provides interfaces used to manipulate a call graph, regardless
/// if it is a "old style" CallGraph or an "new style" LazyCallGraph.
///
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Utils/CallGraphUpdater.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"

using namespace llvm;

bool CallGraphUpdater::finalize() {
  if (!DeadFunctionsInComdats.empty()) {
    filterDeadComdatFunctions(*DeadFunctionsInComdats.front()->getParent(),
                              DeadFunctionsInComdats);
    DeadFunctions.append(DeadFunctionsInComdats.begin(),
                         DeadFunctionsInComdats.end());
  }

  for (Function *DeadFn : DeadFunctions) {
    DeadFn->removeDeadConstantUsers();

    if (CG) {
      CallGraphNode *OldCGN = CG->getOrInsertFunction(DeadFn);
      CG->getExternalCallingNode()->removeAnyCallEdgeTo(OldCGN);
      OldCGN->removeAllCalledFunctions();
      DeadFn->replaceAllUsesWith(UndefValue::get(DeadFn->getType()));

      assert(OldCGN->getNumReferences() == 0);

      delete CG->removeFunctionFromModule(OldCGN);
      continue;
    }

    // The old style call graph (CG) has a value handle we do not want to
    // replace with undef so we do this here.
    DeadFn->replaceAllUsesWith(UndefValue::get(DeadFn->getType()));

    if (LCG && !ReplacedFunctions.count(DeadFn)) {
      // Taken mostly from the inliner:
      LazyCallGraph::Node &N = LCG->get(*DeadFn);
      auto *DeadSCC = LCG->lookupSCC(N);
      assert(DeadSCC && DeadSCC->size() == 1 &&
             &DeadSCC->begin()->getFunction() == DeadFn);
      auto &DeadRC = DeadSCC->getOuterRefSCC();

      FunctionAnalysisManager &FAM =
          AM->getResult<FunctionAnalysisManagerCGSCCProxy>(*DeadSCC, *LCG)
              .getManager();

      FAM.clear(*DeadFn, DeadFn->getName());
      AM->clear(*DeadSCC, DeadSCC->getName());
      LCG->removeDeadFunction(*DeadFn);

      // Mark the relevant parts of the call graph as invalid so we don't visit
      // them.
      UR->InvalidatedSCCs.insert(DeadSCC);
      UR->InvalidatedRefSCCs.insert(&DeadRC);
    }

    // The function is now really dead and de-attached from everything.
    DeadFn->eraseFromParent();
  }

  bool Changed = !DeadFunctions.empty();
  DeadFunctionsInComdats.clear();
  DeadFunctions.clear();
  return Changed;
}

void CallGraphUpdater::reanalyzeFunction(Function &Fn) {
  if (CG) {
    CallGraphNode *OldCGN = CG->getOrInsertFunction(&Fn);
    OldCGN->removeAllCalledFunctions();
    CG->populateCallGraphNode(OldCGN);
  } else if (LCG) {
    LazyCallGraph::Node &N = LCG->get(Fn);
    LazyCallGraph::SCC *C = LCG->lookupSCC(N);
    updateCGAndAnalysisManagerForCGSCCPass(*LCG, *C, N, *AM, *UR);
  }
}

void CallGraphUpdater::registerOutlinedFunction(Function &NewFn) {
  if (CG)
    CG->addToCallGraph(&NewFn);
  else if (LCG)
    LCG->addNewFunctionIntoSCC(NewFn, *SCC);
}

void CallGraphUpdater::removeFunction(Function &DeadFn) {
  DeadFn.deleteBody();
  DeadFn.setLinkage(GlobalValue::ExternalLinkage);
  if (DeadFn.hasComdat())
    DeadFunctionsInComdats.push_back(&DeadFn);
  else
    DeadFunctions.push_back(&DeadFn);
}

void CallGraphUpdater::replaceFunctionWith(Function &OldFn, Function &NewFn) {
  OldFn.removeDeadConstantUsers();
  ReplacedFunctions.insert(&OldFn);
  if (CG) {
    // Update the call graph for the newly promoted function.
    // CG->spliceFunction(&OldFn, &NewFn);
    CallGraphNode *OldCGN = (*CG)[&OldFn];
    CallGraphNode *NewCGN = CG->getOrInsertFunction(&NewFn);
    NewCGN->stealCalledFunctionsFrom(OldCGN);

    // And update the SCC we're iterating as well.
    CGSCC->ReplaceNode(OldCGN, NewCGN);
  } else if (LCG) {
    // Directly substitute the functions in the call graph.
    LazyCallGraph::Node &OldLCGN = LCG->get(OldFn);
    SCC->getOuterRefSCC().replaceNodeFunction(OldLCGN, NewFn);
  }
  removeFunction(OldFn);
}

bool CallGraphUpdater::replaceCallSite(CallBase &OldCS, CallBase &NewCS) {
  // This is only necessary in the (old) CG.
  if (!CG)
    return true;

  Function *Caller = OldCS.getCaller();
  CallGraphNode *NewCalleeNode =
      CG->getOrInsertFunction(NewCS.getCalledFunction());
  CallGraphNode *CallerNode = (*CG)[Caller];
  if (llvm::none_of(*CallerNode, [&OldCS](const CallGraphNode::CallRecord &CR) {
        return CR.first == &OldCS;
      }))
    return false;
  CallerNode->replaceCallEdge(OldCS, NewCS, NewCalleeNode);
  return true;
}

void CallGraphUpdater::removeCallSite(CallBase &CS) {
  // This is only necessary in the (old) CG.
  if (!CG)
    return;

  Function *Caller = CS.getCaller();
  CallGraphNode *CallerNode = (*CG)[Caller];
  CallerNode->removeCallEdgeFor(CS);
}
