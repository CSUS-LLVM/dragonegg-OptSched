
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
  utilities.cpp
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

#SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -DIS_DEBUG -DIS_DEBUG_DAG -DIS_DEBUG_BUILD_DAG -DIS_DEBUG_PRE_ORDER -DIS_DEBUG_DEFS_AND_USES")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -DIS_DEBUG -DIS_DEBUG_SLIL_COST -DIS_DEBUG_DEF_USE_COUNT -DIS_DEBUG_STATIC_LOWER_BOUND -DIS_DEBUG_SLIL_COST_LOWER_BOUND")

add_dependencies(LLVMOptSched intrinsics_gen)
