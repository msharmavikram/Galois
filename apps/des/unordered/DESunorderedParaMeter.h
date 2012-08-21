/** DES ParaMeter unordered version -*- C++ -*-
 * @file
 * @section License
 *
 * Galois, a framework to exploit amorphous data-parallelism in irregular
 * programs.
 *
 * Copyright (C) 2011, The University of Texas at Austin. All rights reserved.
 * UNIVERSITY EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES CONCERNING THIS
 * SOFTWARE AND DOCUMENTATION, INCLUDING ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR ANY PARTICULAR PURPOSE, NON-INFRINGEMENT AND WARRANTIES OF
 * PERFORMANCE, AND ANY WARRANTY THAT MIGHT OTHERWISE ARISE FROM COURSE OF
 * DEALING OR USAGE OF TRADE.  NO WARRANTY IS EITHER EXPRESS OR IMPLIED WITH
 * RESPECT TO THE USE OF THE SOFTWARE OR DOCUMENTATION. Under no circumstances
 * shall University be liable for incidental, special, indirect, direct or
 * consequential damages or loss of profits, interruption of business, or
 * related expenses which may arise from use of Software or Documentation,
 * including but not limited to those resulting from defects in Software and/or
 * Documentation, or loss or inaccuracy of data of any kind.
 *
 * @author M. Amber Hassaan <ahassaan@ices.utexas.edu>
 */

#ifndef DES_UNORDERED_PARMETER_H
#define DES_UNORDERED_PARMETER_H


#include <deque>
#include <functional>

#include <cassert>

#include "DESabstractMain.h"


class DESunorderedParaMeter: public DESabstractMain {

private:
  // some variables used by runLoop 
  std::vector<bool> locks;

  std::deque<GNode> currWorklist;
  std::deque<GNode> nextWorklist;

  size_t step;
  size_t numActivities;
  size_t wlsize;

 
  void initParaMeter (const SimInit& simInit) {
    nextWorklist = std::deque<GNode> ();
    currWorklist = std::deque<GNode> (simInit.getInputNodes ().begin (), simInit.getInputNodes ().end ());

    locks.resize (simInit.getNumNodes (), false);

    step = 0;
    numActivities = 0;
    wlsize = 0;

    printf ("ParaMeter: Step numActivities WLsize\n");
  }

  bool abort (GNode& activeNode) {
    SimObject* srcObj = graph.getData (activeNode, Galois::NONE);

    // first determine that all locks are available
    bool abort = locks[srcObj->getId ()];

    if (!abort) {
      for (Graph::edge_iterator i = graph.edge_begin (activeNode, Galois::NONE), ei =
          graph.edge_end (activeNode, Galois::NONE); i != ei; ++i) {

        SimObject* dstObj = graph.getData (graph.getEdgeDst(i), Galois::NONE);
        abort = abort || locks[dstObj->getId ()];
        if (abort) {
          break;
        }
      }
    }


    // now acquire the locks 
    if (!abort) {
      locks[srcObj->getId ()] = true;

      for (Graph::edge_iterator i = graph.edge_begin (activeNode, Galois::NONE), ei =
          graph.edge_end (activeNode, Galois::NONE); i != ei; ++i) {

        SimObject* dstObj = graph.getData (graph.getEdgeDst(i), Galois::NONE);
        locks[dstObj->getId ()] = true;
      }

    }

    return abort;
  }


  void beginStep () {
    // before starting a computational step
    numActivities = 0;
    nextWorklist.clear ();
    wlsize = currWorklist.size ();
  }


  void finishStep () {

    // after finishing a computational step
    printf ("ParaMeter: %zd %zd %zd \n", step, numActivities, wlsize);

    ++step;
    numActivities = 0;

    // switch worklists
    currWorklist = nextWorklist;
    nextWorklist = std::deque<GNode> ();

    // release all locks
    std::fill (locks.begin (), locks.end (), false);

  }
  
 

public:
  virtual bool isSerial () const { return true; }

  /**
   * Run loop.
   * Does not use GaloisRuntime or Galois worklists
   *
   * To ensure uniqueness of items on the worklist, we keep a list of boolean flags for each node,
   * which indicate whether the node is on the worklist. When adding a node to the worklist, the
   * flag corresponding to a node is set to True if it was previously False. The flag reset to False
   * when the node is removed from the worklist. This list of flags provides a cheap way of
   * implementing set semantics.
   *
   */

  virtual void runLoop (const SimInit& simInit) {

    std::vector<bool> onWlFlags (simInit.getNumNodes (), false);

    // set onWlFlags for input objects
    for (std::vector<GNode>::const_iterator i = simInit.getInputNodes ().begin (), ei = simInit.getInputNodes ().end ();
        i != ei; ++i) {
      SimObject* srcObj = graph.getData (*i, Galois::NONE);
      onWlFlags[srcObj->getId ()] = true;
    }



    initParaMeter (simInit);

    size_t maxPending = 0;

    size_t numEvents = 0;
    size_t numIter = 0;

    while (!currWorklist.empty ()) {

      beginStep ();
      
      while (!currWorklist.empty ()) {
        GNode activeNode = currWorklist.front ();
        currWorklist.pop_front ();

        SimObject* srcObj = graph.getData (activeNode, Galois::NONE);

        if (abort(activeNode)) {
          nextWorklist.push_back (activeNode);
          continue;
        }
        ++numActivities;

        maxPending = std::max (maxPending, srcObj->numPendingEvents ());

        numEvents += srcObj->simulate(graph, activeNode);


        for (Graph::edge_iterator i = graph.edge_begin (activeNode, Galois::NONE), ei =
            graph.edge_end (activeNode, Galois::NONE); i != ei; ++i) {
          const GNode dst = graph.getEdgeDst(i);

          SimObject* dstObj = graph.getData (dst, Galois::NONE);

          if (dstObj->isActive ()) {
            if (!onWlFlags[dstObj->getId ()]) {
              // set the flag to indicate presence on the worklist
              onWlFlags[dstObj->getId ()] = true;
              nextWorklist.push_back (dst);
            }
          }
        }

        if (srcObj->isActive()) {
          nextWorklist.push_back (activeNode);

        } else { 
          // reset the flag to indicate absence on the worklist
          onWlFlags[srcObj->getId ()] = false;
        }

        ++numIter;

      }

      finishStep ();

    }


    std::cout << "Simulation ended" << std::endl;
    std::cout << "Number of events processed = " << numEvents << " Iterations = " << numIter << std::endl;
    std::cout << "Max size of pending events = " << maxPending << std::endl;

  }

};



#endif // DES_UNORDERED_PARMETER_H
