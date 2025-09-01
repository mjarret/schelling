# schelling-min

Minimal, modular Schelling simulator with:
- Binary types, density `ρ`, threshold `τ`.
- Graph plug-ins (torus grid, lollipop).
- Multi-threaded runs; bit-packed world.
- Primary metric: **settlement probability** (converges to 0 unhappy).
- **Anytime-valid, variance-adaptive** Empirical–Bernstein confidence sequence.
- Optional live plot via gnuplot (gracefully disabled if unavailable).

## Build
```bash
make clean && make

# Torus grid (wrap), 256x256
./schelling --graph torus --size 256x256 \
  --density 0.70 --threshold 0.50 \
  --eps 0.02 --alpha 1e-4 --seed 123

# Lollipop: clique m=64, path L=512
./schelling --graph lollipop --size 64:512 \
  --density 0.70 --threshold 0.50 \
  --eps 0.03 --alpha 1e-4

