// reductions.hpp — OpenMP custom reductions centralized
#pragma once

#include <omp.h>
#include "sim/step_dense.hpp"

namespace sim {

// Declare a custom OpenMP reduction for StepDense using merge_step_dense
#pragma omp declare reduction (merge_step_dense : sim::StepDense : \
    sim::merge_step_dense(omp_out, omp_in)) initializer(omp_priv = sim::StepDense{})

} // namespace sim

