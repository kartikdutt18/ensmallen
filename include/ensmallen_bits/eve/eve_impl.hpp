/**
 * @file eve_impl.hpp
 * @author Marcus Edel
 *
 * Implementation of Eve: a gradient based optimization method with Locally
 * and globally adaptive learning rates.
 *
 * ensmallen is free software; you may redistribute it and/or modify it under
 * the terms of the 3-clause BSD license.  You should have received a copy of
 * the 3-clause BSD license along with ensmallen.  If not, see
 * http://www.opensource.org/licenses/BSD-3-Clause for more information.
 */
#ifndef ENSMALLEN_EVE_EVE_IMPL_HPP
#define ENSMALLEN_EVE_EVE_IMPL_HPP

// In case it hasn't been included yet.
#include "eve.hpp"

#include <ensmallen_bits/function.hpp>

namespace ens {

inline Eve::Eve(const double stepSize,
                const size_t batchSize,
                const double beta1,
                const double beta2,
                const double beta3,
                const double epsilon,
                const double clip,
                const size_t maxIterations,
                const double tolerance,
                const bool shuffle) :
    stepSize(stepSize),
    batchSize(batchSize),
    beta1(beta1),
    beta2(beta2),
    beta3(beta3),
    epsilon(epsilon),
    clip(clip),
    maxIterations(maxIterations),
    tolerance(tolerance),
    shuffle(shuffle)
{ /* Nothing to do. */ }

//! Optimize the function (minimize).
template<typename DecomposableFunctionType>
double Eve::Optimize(
    DecomposableFunctionType& function,
    arma::mat& iterate)
{
  typedef Function<DecomposableFunctionType> FullFunctionType;
  FullFunctionType& f(static_cast<FullFunctionType&>(function));

  // Make sure we have all the methods that we need.
  traits::CheckDecomposableFunctionTypeAPI<FullFunctionType>();

  // Find the number of functions to use.
  const size_t numFunctions = f.NumFunctions();

  // To keep track of where we are and how things are going.
  size_t currentFunction = 0;
  double overallObjective = 0;
  double lastOverallObjective = DBL_MAX;

  double objective = 0;
  double lastObjective = 0;
  double dt = 1;

  // The exponential moving average of gradient values.
  arma::mat m = arma::zeros<arma::mat>(iterate.n_rows, iterate.n_cols);

  // The exponential moving average of squared gradient values.
  arma::mat v = arma::zeros<arma::mat>(iterate.n_rows, iterate.n_cols);

  // Now iterate!
  arma::mat gradient(iterate.n_rows, iterate.n_cols);
  const size_t actualMaxIterations = (maxIterations == 0) ?
      std::numeric_limits<size_t>::max() : maxIterations;
  for (size_t i = 0; i < actualMaxIterations; /* incrementing done manually */)
  {
    // Is this iteration the start of a sequence?
    if ((currentFunction % numFunctions) == 0 && i > 0)
    {
      // Output current objective function.
      Info << "Eve: iteration " << i << ", objective " << overallObjective
          << "." << std::endl;

      if (std::isnan(overallObjective) || std::isinf(overallObjective))
      {
        Warn << "Eve: converged to " << overallObjective << "; terminating"
            << " with failure.  Try a smaller step size?" << std::endl;
        return overallObjective;
      }

      if (std::abs(lastOverallObjective - overallObjective) < tolerance)
      {
        Info << "Eve: minimized within tolerance " << tolerance << "; "
            << "terminating optimization." << std::endl;
        return overallObjective;
      }

      // Reset the counter variables.
      lastOverallObjective = overallObjective;
      overallObjective = 0;
      currentFunction = 0;

      if (shuffle) // Determine order of visitation.
        f.Shuffle();
    }

    // Find the effective batch size; we have to take the minimum of three
    // things:
    // - the batch size can't be larger than the user-specified batch size;
    // - the batch size can't be larger than the number of iterations left
    //       before actualMaxIterations is hit;
    // - the batch size can't be larger than the number of functions left.
    const size_t effectiveBatchSize = std::min(
        std::min(batchSize, actualMaxIterations - i),
        numFunctions - currentFunction);

    // Technically we are computing the objective before we take the step, but
    // for many FunctionTypes it may be much quicker to do it like this.
    objective = f.EvaluateWithGradient(iterate, currentFunction,
        gradient, effectiveBatchSize);
    overallObjective += objective;

    m *= beta1;
    m += (1 - beta1) * gradient;

    v *= beta2;
    v += (1 - beta2) * (gradient % gradient);

    const double biasCorrection1 = 1.0 - std::pow(beta1, (double) (i + 1));
    const double biasCorrection2 = 1.0 - std::pow(beta2, (double) (i + 1));

    if (i > 0)
    {
      const double d = std::abs(objective - lastObjective) /
          (std::min(objective, lastObjective) + epsilon);

      dt *= beta3;
      dt += (1 - beta3) * std::min(std::max(d, 1.0 / clip), clip);
    }

    lastObjective = objective;

    iterate -= stepSize / dt * (m / biasCorrection1) /
        (arma::sqrt(v / biasCorrection2) + epsilon);

    i += effectiveBatchSize;
    currentFunction += effectiveBatchSize;
  }

  Info << "Eve: maximum iterations (" << maxIterations << ") reached; "
      << "terminating optimization." << std::endl;

  // Calculate final objective.
  overallObjective = 0;
  for (size_t i = 0; i < numFunctions; i += batchSize)
  {
    const size_t effectiveBatchSize = std::min(batchSize, numFunctions - i);
    overallObjective += f.Evaluate(iterate, i, effectiveBatchSize);
  }
  return overallObjective;
}

} // namespace ens

#endif