Theo Zervos
tnz3@pitt.edu

All assigned functions work to the best of my knowledge. If you encounter any issues
running the program, you may need to run with --add-lib-path like so:

--add-lib-path build simulation.py -- --program <program file> --latencies <latencies file> --output <output file>

Program goes wild when no halt is present in assembly but I assume that is the natural behavior
of going beyond the defined program bounds and reading garbage memory.