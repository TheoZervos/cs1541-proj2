#########################################
#		Inputs to the simulation		#
#########################################
import argparse
parser = argparse.ArgumentParser(description='Configuration options for this simulation.')

parser.add_argument('--program',
                    dest="program",
                    required=True,
                    action='store',
                    help='The program to run in the simulator')

parser.add_argument('--configuration',
                    dest="configuration",
                    required=True,
                    action='store',
                    help='The json file with the desired configuration')

parser.add_argument('--output',
                    dest='output',
                    required=False,
                    action='store',
                    help='The outfile to store statistics from the running of a program')

# Parse arguments
args = parser.parse_args()

## Print info ##
print("Running simulator using program "+args.program)
print("Configurations in  "+args.configuration)
print(f"Output statistics to be stored in {args.output}")

#####################################
#	Load JSON file with latencies	#
#####################################
import json
with open(args.configuration, 'r') as inp_file:
  configuration=json.load(inp_file)
print("Latencies:")
print(json.dumps(configuration, indent=2))
# Latencies will look like
# {
#   "liz": 20,
#   "sw": 150,
#   "lw": 150,
#   "put": 1000,
#   "halt": 1,
# }

# Now the simulation
import sst

# Add our core to the simulation!
core = sst.Component("XSim","XSim.Core")
core.addParams({
  "configuration": args.configuration,
  "program": args.program,
  "output": args.output,
  "verbose": 0
})
# Configure the memory interface in our CPU to use the standard interface
iface = core.setSubComponent("memory", "memHierarchy.standardInterface")
#iface.addParams({"debug" : 1, "debug_level" : 10})

#L1 cache component: mem interface --> cache --> DRAM
#cache freq, size, and associativity parsed from json config 
cache = sst.Component("l1_cache", "memHierarchy.Cache")
cache.addParams(
  {
    "cache_frequency": configuration["clock"], 
    "cache_size": configuration["cache"]["size"],
    "associativity": str(configuration["cache"]["associativity"]), #1,2,4,8
    "access_latency_cycles": "1", #1 clock cycle
    "cache_line_size": "16", #16 bytes
    "L1": "true",
})

# Now we add the memory to the simulation
# In this case we're using a simple memory controller (the memory frontend)
# This will map RAM from memory add_range_start (0) to add_range_end (64KiB)
memory = sst.Component("data_memory", "memHierarchy.MemController")
memory.addParams(
{
	'clock':		"1GHz",
	"verbose" : 		2,
	"addr_range_end":	64*1024-1,
})

# Memory access timing model we attach it to the memory backend
#	- this does the specifics of the memory simulation
## SimpleMem has a constant access latency for all accesses
## 64KiB memory with 1000ns access latency
memory_timing = memory.setSubComponent("backend", "memHierarchy.simpleMem")
memory_timing.addParams({
	"access_time" : "1000ns",
	"mem_size" : "64KiB"
})

#cpu interface -> cache -> cpu link (replacing interface ->cpu link) 
cpu_cache_link = sst.Link("cpu_cache_link")
cpu_cache_link.connect(
    (iface,  "lowlink",  "500ps"),
    (cache,  "highlink", "500ps")
)

cache_mem_link = sst.Link("cache_mem_link")
cache_mem_link.connect(
    (cache,  "lowlink",  "500ps"),
    (memory, "highlink", "500ps")
)

# Enable statistics for all components
sst.setStatisticLoadLevel(7)
sst.setStatisticOutput("sst.statOutputConsole")
sst.enableAllStatisticsForAllComponents()


