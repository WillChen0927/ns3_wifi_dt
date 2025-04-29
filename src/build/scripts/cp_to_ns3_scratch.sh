#!/bin/bash

#  ns-3 dir path
NS3_DIR="../../../../ns-3/ns-3-allinone/ns-3.44" 

if [ ! -d "$NS3_DIR/scratch" ]; then
    echo "Error: ns-3 scratch directory not found at $NS3_DIR/scratch"
    exit 1
fi

echo "Copying simulation files to ns-3 scratch directory..."

cp ../../ns3_sim_files/* "$NS3_DIR/scratch/"

echo "Done. Now you can run your simulation from ns-3."