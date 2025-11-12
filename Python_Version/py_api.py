import networkx as nx
from modelJ import Schelling


def run_schelling_process_py(cs: int, pl: int,
                             density: float = 0.8,
                             homophily: float = 0.5,
                             seed: int = 42) -> int:
    """Python equivalent of the C++ run_schelling_process.

    Builds a lollipop graph with clique size cs and path length pl,
    initializes the Schelling model, and runs until all agents are happy.

    Returns the number of moves (hitting time).
    """
    G = nx.lollipop_graph(cs, pl)
    G1 = nx.convert_node_labels_to_integers(G)
    m = Schelling(input_graph=G1,
                  density=density,
                  minority_pc=0.5,
                  homophily=homophily,
                  seed=seed)
    while m.happy < m.n_agents:
        m.step()
    return m.number_of_moves

