#pragma once
/*
Dynamic cap with periodic evaluation (lightweight).

RATIONALE (2025-09-01):
- Many runs perform huge numbers of moves; computing the cap after every move
  is unnecessary and adds overhead. This version evaluates the Chernoff-style
  cap only every `interval_moves_` moves (default 1000) after a short warm-up.
- The external API matches the previous version so you don't need to change
  call sites:
    * on_move(decreased_unhappy, U_now, delta_cap)
    * exceeded()

MATH (same as before):
- Let moves M, successes S = #times U decreased by ≥1.
- One-sided lower confidence bound for success probability:
    p_L = max(0, S/M - sqrt( log(1/δ) / (2 M) ))
- Project moves needed to finish:
    T_needed ≈ U / max(p_L, p_floor)
- Set cap:
    T_cap = M + ceil( (1 + margin) * T_needed ),
    margin = sqrt( 2*log(1/δ) / max(1, U) ) + 0.5
- If M >= T_cap -> stop run and declare non-settle.
*/

#include <cstddef>
#include <cmath>
#include <algorithm>
#include <limits>

struct DynamicCap {
    // Counters
    std::size_t moves{0};
    std::size_t successes{0};
    std::size_t cap_moves{(std::size_t)-1};

    // Tunables
    std::size_t interval_moves_{1000}; // evaluate the cap every N moves
    std::size_t warmup_moves_{1000};   // wait for some data before evaluating
    double      p_floor_{1e-6};        // floor on success prob to avoid blow-ups

    // Cached constants
    double last_delta_{-1.0};
    double log_inv_delta_{0.0};        // log(1/δ)
    double rad_factor_{0.0};           // sqrt(log(1/δ)/2), used as rad = rad_factor_/sqrt(M)
    double margin_factor_{0.0};        // sqrt(2*log(1/δ))

    void reset() {
        moves = successes = 0;
        cap_moves = (std::size_t)-1;
        // keep tunables as-is
        last_delta_ = -1.0; // force recompute when first used
    }

    // Optional: set evaluation cadence and warm-up (call once after construct/reset).
    void configure(std::size_t interval_moves = 1000,
                   std::size_t warmup_moves   = 1000,
                   double p_floor             = 1e-6) {
        interval_moves_ = std::max<std::size_t>(1, interval_moves);
        warmup_moves_   = warmup_moves;
        p_floor_        = p_floor;
    }

    // Update on each move. We evaluate only every `interval_moves_` steps after warm-up.
    inline void on_move(bool decreased_unhappy,
                        std::size_t U_now,
                        double delta_cap) {
        // Update counters
        moves += 1;
        if (decreased_unhappy) successes += 1;

        // Defer evaluation to save cycles
        if (moves < warmup_moves_) return;
        if ((moves % interval_moves_) != 0) return;

        // Cache log(1/delta) and derived constants
        if (delta_cap != last_delta_) {
            last_delta_   = delta_cap;
            log_inv_delta_ = std::log(1.0 / std::max(1e-300, delta_cap));
            rad_factor_    = std::sqrt(std::max(0.0, log_inv_delta_ / 2.0));
            margin_factor_ = std::sqrt(std::max(0.0, 2.0 * log_inv_delta_));
        }

        // Compute lower confidence bound on success probability
        const double M = static_cast<double>(moves);
        const double S = static_cast<double>(successes);
        const double p_hat = std::max(0.0, S / M);
        const double rad   = rad_factor_ / std::sqrt(M);  // sqrt( log(1/δ) / (2M) )
        const double p_L   = std::max(p_floor_, p_hat - rad);

        // If no unhappy agents remain, caller will stop the run; don't set cap here.
        const double U = static_cast<double>(U_now);
        if (U <= 0.0) return;

        // Project remaining moves and set a cap in absolute move count
        const double T_needed = U / p_L;
        const double margin   = margin_factor_ / std::max(1.0, U) + 0.5;
        const double extra    = std::ceil( (1.0 + margin) * T_needed );

        // Never decrease cap (monotone); only set if larger or not set yet.
        const std::size_t proposed = moves + static_cast<std::size_t>(std::max(0.0, extra));
        if (cap_moves == (std::size_t)-1 || proposed > cap_moves) {
            cap_moves = proposed;
        }
    }

    inline bool exceeded() const {
        return cap_moves != (std::size_t)-1 && moves >= cap_moves;
    }
};
