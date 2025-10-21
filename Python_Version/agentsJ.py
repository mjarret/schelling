from mesa import Agent

import random

from csv import writer


class SchellingAgent(Agent):
    """Schelling segregation agent."""
    
    # Keep track of whether an agent is happy or not, instead of reevaluating every step
    happy = False

    def __init__(self, model, agent_type: int) -> None:
        """Create a new Schelling agent.

        Args:
            model: The model instance the agent belongs to
            agent_type: Indicator for the agent's type (minority=1, majority=0)
        """
        super().__init__(model)
        self.type = agent_type

    # This step updates agent's happiness based on their neighbor
    # It also updates the global counter for happy agents, so determine if the model should keep running
    def count(self) -> None:
        #Determine if agent is happy and move if necessary
        neighbors = self.model.grid.get_neighbors(
            self.pos, include_center=False
        )

        # Count similar neighbors. Rewritten to use percentage instead of total
        similar_total = 0
        if len(neighbors) > 0:
            for neighbor in neighbors:
                if neighbor.type == self.type:
                    similar_total += 1 
            similar = similar_total / len(neighbors)
             
        else:
            similar = 1

        # If unhappy, move to a random empty cell:
        if similar >= self.model.homophily:
            self.happy = True
            self.model.happy += 1
        else:
            # just in case make sure the agent is listed as unhappy
            self.happy = False
            # add the agent to the list of unhappy agents
            self.model.unhappy_list.append(self)
        
    # Similar to count function, but we also need to decrement happiness tracker if
    # someone becomes unhappy due to a move
    def recount(self) -> None:
        """Determine if agent is happy and move if necessary."""
        neighbors = self.model.grid.get_neighbors(
            self.pos, include_center=False
        )

        # Count similar neighbors
        similar_total = 0
        if len(neighbors) > 0:
            #similar = sum(neighbor.type == self.type for neighbor in neighbors)
            for neighbor in neighbors:
                if neighbor.type == self.type:
                    similar_total += 1 
            similar = similar_total / len(neighbors)
             
        else:
            similar = 1

        # mark yourself as happy, but only if the agent was unhappy before
        if similar >= self.model.homophily:
            if self.happy == False:       
                self.model.happy += 1
                self.happy = True
                self.model.unhappy_list.remove(self)
        # mark yourself as unhappy, but only if the agent was happy before
        else:
            if self.happy == True:
                self.model.happy -= 1
                self.happy = False
                self.model.unhappy_list.append(self)

        
    # The main step of the agent. Determine if the agent needs to move
    def move(self) -> None:         
        
        # If unhappy, move to a random empty cell:
           
        if self.happy == False:
            
            # gather old neighbors
            neighbors = self.model.grid.get_cell_list_contents(self.model.addresses[self.pos])
            # new position randomly selected from set of empty nodes
            pos = random.choice(self.model.empty)
            # add current position to the list of empty spots
            self.model.empty.append(self.pos)
            # move
            self.model.grid.move_agent(self, pos)
            # remove new position from the list of available spots
            self.model.empty.remove(pos)
            # update number of moves
            self.model.number_of_moves += 1
            # gather new neighbors
            new_neighbors = self.model.grid.get_cell_list_contents(self.model.addresses[self.pos])
            
            # update counts of everyone
            self.recount()
            for n in neighbors:
                n.recount()
            for m in new_neighbors:
                m.recount()
           
            # output move data to a file. This is used for plots of individual steps.
            #unhappy_fraction = 1 - (self.model.happy / self.model.n_agents)
            #data = [self.model.number_of_moves, unhappy_fraction]
            #file = 'lpop_101025.csv'
            #with open(file, 'a', newline='') as f_object:
            #    writer_object = writer(f_object)
            #    writer_object.writerow(data)
            #    f_object.close

