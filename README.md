Project / feature summary:
The Project was based on the Arena tank network starter project. I worked on implementing health, death and respawn to the tanks. With a visible health bar above each tank displaying their respective health.

How to run and test the project:
Unreal version 5.8
Generate visual studio files if needed. Then Open NetworkTankArena.uproject.
Make sure that PIE Settings is set to 2 players and net mode to listen server.
In the advanced settings under multiplayer options start with network emulation set to off. Run and try and shoot the other tank until it dies and wait for it to respawn. Then try with turning on Network Emulation and set it to custom with 150 minimum latency and 300 maximum and 20 packet loss percentage for both incoming and outgoing. 

What you implemented or changed:
I implemented a health, death and respawn cycle. If a tank gets hit, the client sends a request to the server which then updates the health and then replicates it to the clients and plays a damage flash by changing the material to bright red for a short period. Every time the health changes it sends a event that will update the health bar. I simulated death by turning of visibility and collision and disabling all other input actions and sets a timer for when to respawn. The respawn turns everything back on and resets health back to maximum. 

Important client/server, authority and replication decisions.
I decided that the server would always have authority and decide what changes and what is the truth. The server knows the health and replicates it to the client every time it changes.

The network conditions used for testing:
I tested with working baseline with emulation off. And a impaired version with emulation on custom for everyone with 150 minimum latency and 300 maximum latency and 20% packet loss for both incoming and outgoing traffic.

At least one networking/synchronization problem you encountered and how you diagnosed it:
Sometimes when dying there would be no red damage flash and it would appear on respawn instead. Diagnosed it mostly visually by seeing the damage flash appear when it is not supposed to and by using logs that state when something happens and for who. 

The networking improvement or optimization you made, including why you chose it:
I decided to pass through the old health before the tank takes damage when replicating so that i can check weather the new health was more or less than the previous and only show the damage flash if the new health was lower than the old health.

Known limitations, trade-offs or remaining problems.
The client side is still very jittery and suffers from latency which could probably be reduced with prediction and perhaps if needed some interpolation too.
