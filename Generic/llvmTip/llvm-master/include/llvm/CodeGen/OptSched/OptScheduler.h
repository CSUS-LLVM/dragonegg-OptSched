//===-

#ifndef OPTSCHED_OPTSCHEDULER_H
#define OPTSCHED_OPTSCHEDULER_H

#include "llvm/CodeGen/ScheduleDAGInstrs.h"

namespace opt_sched {
  class OptScheduler; 

  class OptScheduler : public llvm::ScheduleDAGInstrs {
  };

} // namespace opt_sched
#endif
