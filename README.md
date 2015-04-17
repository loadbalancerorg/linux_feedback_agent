# linux_feedback_agent
Linux feedback agent to control the real server weight of HAProxy based on server load and other metrics. 
The agent listens on a specfied port and returns the cpu idle as a percentage.

Edit feedback.cfg with the port to listen on and the drain threshold. The drain threshold is the percentange of cpu usage that when reached will tell the agent to mark the server in drain mode to prevent any new connections.
