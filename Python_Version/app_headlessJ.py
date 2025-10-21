import modelJ

import pandas as pd

import networkx as nx

from modelJ import Schelling

import cProfile, pstats

import time

from mesa.visualization import (
    Slider,
    SolaraViz,
    make_plot_component,
    make_space_component,
)



# Start profiling
#pr = cProfile.Profile()
#pr.enable()


# Setup output dataframe
df = pd.DataFrame(columns=['time_to_settle', 'nodes'])
df2 = pd.DataFrame(columns=['steps_to_settle', '5th', '95th', 'time', 'nodes'])

# Run the actual model as many times as needed
networks = [100, 200, 500, 1000, 2000, 5000, 10000]
# for testing
# networks = [100, 200, 500, 1000]
for size in networks:

    # specify the input graph on creation
    #G0 ="20x20_vN.gexf" # von Neumann neighborhood
    #G0 ="20x20_M.gexf" # Moore neighborhood
    #G0 ="jazz_400.gexf" # Social network
    #G0 ="SW_400.gexf" # Small world network
    
    # Alternatively, just make a new graph in the model itself.

    network = "grid_test"
    G0 = nx.lollipop_graph(int(size * 0.1), int(size * 0.9))

    # bigger networks take longer to run, but also exhibit less variance. So we should be able to get away with running them less times.    
    j = 500
    if size == 10000:
        j = 50
    elif size == 5000:
        j = 100
    elif size == 2000:
        j = 200
    
    # prepare the graph for Mesa's NetworkGrid
    nodes = G0.number_of_nodes()
    G1 = nx.convert_node_labels_to_integers(G0)
    
    # Default model parameters for Schelling - reused from the visual version of the model
    # To modify the actual parameters in the model, update parameters in the modelJ file instead.
    model_params = {
        "seed": {
            "type": "InputText",
            "value": 42,
            "label": "Random Seed",
        },
        "density": Slider("Agent density", 0.8, 0.1, 1.0, 0.1),
        "minority_pc": Slider("Fraction minority", 0.5, 0.0, 1.0, 0.05),
        "homophily": Slider("Homophily", 0.5, 0, 1, 0.05),
        "width": 20,
        "height": 20,
        "input_graph": [G1],
    }

    # keeping stats for this loop only
    df3 = pd.DataFrame(columns=['time_to_settle', 'nodes'])

    # Time measurement
    start = time.time()

    for i in range(j):
        
        # Create the model and run it
        model1 = Schelling(input_graph=G1)
        time_to_settle = 0
        while model1.happy < model1.n_agents:
            model1.step()
            # Stop running the model if it runs into a dead end configuration
            if model1.steps > 500000:
                break
        
        # Record the time it takes to settle the model. Reduce by 1 to account for how Mesa tracks steps.
        time_to_settle = model1.steps - 1
        if i % 100 == 0:
            print(time_to_settle)
        df.loc[len(df.index)] = [time_to_settle, nodes]
        df3.loc[len(df3.index)] = [time_to_settle, nodes]
    

    
    # End time measurement
    end = time.time()
    df_time = (end - start) / j
    
    
    # Summarize the output
    
    df_mean = df3.loc[:, 'time_to_settle'].mean()
    df_5th = df3['time_to_settle'].quantile(q=0.05)
    df_95th = df3['time_to_settle'].quantile(q=0.95)
    df2.loc[len(df2.index)] = [df_mean, df_5th, df_95th, df_time, nodes]
    
    # print the results
    print(network + str(size))
    print(df_mean)
    print(df_5th)
    print(df_95th)
    print(df_time)

# Output the dataframe of results to a CSV file.
#out_file = "schelling_results_" + network + str(j) + ".csv"
out_file = 'lpop_scaling_70_60thr.csv'
df.to_csv(out_file)
out_file2 = 'lpop_summary_70_60thr.csv'
df2.to_csv(out_file2)



# End profiling
#pr.disable()
#stats = pstats.Stats(pr)
#stats.strip_dirs()
#pr.dump_stats('schelling.pstats')
#stats.sort_stats("cumtime").print_stats(10)