add_llvm_library(LLVMOptSched
  OptScheduler.cpp
  )

add_dependencies(LLVMOptSched intrinsics_gen)
