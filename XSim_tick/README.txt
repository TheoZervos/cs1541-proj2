Theo Zervos
tnz3@pitt.edu

Michael Puthumana
mip132@pitt.edu

All assigned functions work to the best of out knowledge. If you encounter any issues
running the program, you may need to run with --add-lib-path like so:

--add-lib-path build simulation.py -- --program program_file.t --configuration configuration_file.json --output output_file.json

Program finished when end of instructions is reached even if halt is not present. This was done
to make testing easier but does not influence the outcome of program runs (the pipeline must flush before program ends)