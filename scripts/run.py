#!/usr/bin/env python3
import argparse
import os
import subprocess
import sys

def main():
    ap = argparse.ArgumentParser(description="Configure, build, and run the experiment with a fixed graph.")
    ap.add_argument('--graph', default='path', choices=['path', 'clique'], help='Graph type (compile-time)')
    ap.add_argument('--k', type=int, default=2, help='Number of colors')
    ap.add_argument('--n', type=int, default=64, help='Number of vertices')
    ap.add_argument('args', nargs=argparse.REMAINDER, help='Args for the executable after -- (e.g., --steps ...)')
    opts = ap.parse_args()

    graph_map = { 'path': 'PATH', 'clique': 'CLIQUE' }
    graph_define = graph_map[opts.graph]

    build_dir = 'build'
    os.makedirs(build_dir, exist_ok=True)

    cmake_cmd = [
        'cmake', '-S', '.', '-B', build_dir,
        f'-DGRAPH={graph_define}', f'-DK={opts.k}', f'-DN={opts.n}',
        '-DCMAKE_BUILD_TYPE=Release'
    ]
    print(' '.join(cmake_cmd))
    subprocess.check_call(cmake_cmd)

    build_cmd = ['cmake', '--build', build_dir, '-j']
    print(' '.join(build_cmd))
    subprocess.check_call(build_cmd)

    exe = os.path.join(build_dir, 'run_experiment')
    run_cmd = [exe] + opts.args
    print(' '.join(run_cmd))
    return subprocess.call(run_cmd)

if __name__ == '__main__':
    sys.exit(main())

