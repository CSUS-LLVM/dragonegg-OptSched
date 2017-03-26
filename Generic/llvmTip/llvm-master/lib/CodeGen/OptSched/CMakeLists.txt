add_llvm_library(LLVMOptSched
  OptScheduler.cpp
  bb_spill.cpp
  OptSchedMachineWrapper.cpp
  buffers.cpp
  config.cpp
  data_dep.cpp
  enumerator.cpp
  gen_sched.cpp
  graph.cpp
  hist_table.cpp
  list_sched.cpp
  logger.cpp
  machine_model.cpp
  OptSchedDagWrapper.cpp
  random.cpp
  ready_list.cpp
  register.cpp
  relaxed_sched.cpp
  sched_basic_data.cpp
  sched_region.cpp
  stats.cpp
  )

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DIS_DEBUG_BBSPILL_COST -DIS_DEBUG_ENUM_ITERS -DIS_DEBUG_ITERS -DIS_DEBUG_RJ -DIS_DEBUG_NODES -DIS_DEBUG_PRINT_BOUNDS -DIS_DEBUG_PRINT_SCHEDS -DIS_DEBUG_SCHED2 -DIS_DEBUG_LATENCIES -DIS_DEBUG_OUT_EDGES -DIS_DEBUG_BOUNDS -DIS_DEBUG_MODEL -DIS_DEBUG_SOLN_DETAILS_1 -DIS_DEBUG -DIS_DEBUG_NODEDOM -DIS_DEBUG_ISSUE_TYPES -DIS_DEBUG_FLOW -DIS_DEBUG_NODES -DIS_DEBUG_READY_LIST -DIS_DEBUG_INFSEBLTY_TEST -DIS_DEBUG_SPD -DIS_DEBUG_TRACE_ENUM -DIS_DEBUG_FIX")

add_dependencies(LLVMOptSched intrinsics_gen)
