import mesa

import agentsJ

import networkx as nx

import random

from csv import writer

from mesa import Model
from mesa.datacollection import DataCollector
from agentsJ import SchellingAgent
from mesa.space import SingleGrid



class Schelling(Model):
    """Model class for the Schelling segregation model."""
    
    # Track the number of moves
    number_of_moves = 0
    

    # MODIFY THE PARAMETERS HERE
    def __init__(
        self,
        input_graph,
        height: int = 40,
        width: int = 40,
        density: float = 0.7,
        minority_pc: float = 0.5,
        homophily: int = 0.6,
        radius: int = 1,
        seed=None,
        number_of_moves = 0,
    ):
        """Create a new Schelling model.

        Args:
            width: Width of the grid
            height: Height of the grid
            density: Initial chance for a cell to be populated (0-1)
            minority_pc: Chance for an agent to be in minority class (0-1)
            homophily: Minimum percent of similar neighbors needed for happiness
            radius: Search radius for checking neighbor similarity
            seed: Seed for reproducibility
        """
        super().__init__(seed=seed)

        # Model parameters
        self.height = height
        self.width = width
        self.density = density
        self.minority_pc = minority_pc
        self.homophily = homophily
        self.radius = radius
        self.input_graph = input_graph
        self.number_of_steps = 0

      
        # Setup a network of choice to be used as the grid
        self.G = self.input_graph
        # Label the nodes for the model
        #self.G = nx.convert_node_labels_to_integers(G0)
        # Construct grid
        self.grid = mesa.space.NetworkGrid(self.G)
        
        # Track happiness
        self.happy = 0

        
        # Create a dictionary to look up agent neighbors instead of doing a search
        self.addresses = {}
        
        # keep track of empty spaces
        self.empty = []
        
        # list of agents - reduces lookup time
        self.agent_list = []
        self.n_agents = 0
        
        # list of unhappy agents
        self.unhappy_list = []
        
        # Create agents and place them on the network nodes with a probability = density
        for node in self.G.nodes():
            self.empty.append(node)
            # update the dictionary
            neighborhood = self.grid.get_neighborhood(node)
            self.addresses[node]=neighborhood
        # spawn agents of one type
        total_agents = self.density * self.G.number_of_nodes() 
        agents0_count = round(total_agents * minority_pc)
        while self.n_agents < agents0_count:
            agent_type = 0
            agent = SchellingAgent(self, agent_type)
            node = random.choice(self.empty)
            self.grid.place_agent(agent, node)
            self.empty.remove(node)
            self.agent_list.append(agent)
            self.n_agents += 1
        # spawn the rest of agents of the other type
        while self.n_agents < total_agents:
            agent_type = 1
            agent = SchellingAgent(self, agent_type)
            node = random.choice(self.empty)
            self.grid.place_agent(agent, node)
            self.empty.remove(node)
            self.agent_list.append(agent)
            self.n_agents += 1

        
        # Perform initial counting of happy agents
        self.agents.shuffle_do("count")

    def step(self):
        # Pick a random agent and decide if they need to move

        # replaced the OOTB function with a plain list of agents
        #one_agent = random.choice(self.agents)
        one_agent = random.choice(self.unhappy_list)
        one_agent.move()
        #self.running = self.happy < len(self.agents)  # Continue until everyone is happy
        self.running = self.happy < self.n_agents  # Continue until everyone is happy
        
        # For debugging we can see if the model is running
        #if self.number_of_steps > 1000 and self.number_of_steps%1000 == 0:
        #    print(self.number_of_steps)
        #self.datacollector.collect(self)
        
