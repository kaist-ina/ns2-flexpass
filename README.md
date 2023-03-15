# FlexPass ns-2 simulator (EuroSys 2023)

This is an official Github repository for the EuroSys '23 paper "FlexPass: A Case for Flexible Credit-based Transport for Datacenter Networks".
The repository includes our implementation of FlexPass on the ns-2 simulator.

## Quick Start
We provide a pre-built docker image at `ghcr.io/kaist-ina/ns2-flexpass`. Use the following command to set-up the docker image.
```bash
docker pull ghcr.io/kaist-ina/ns2-flexpass
```

## How to run experiments

1. We provide a python script named `runs_simulations.py` to automate simulations. Use following command to run all simulations.
    ```
    docker run --rm -it \
        -v $(pwd)/result:/ns-allinone-2.35/ns-2.35/outputs \
        ghcr.io/kaist-ina/ns2-flexpass \
        python ./run_simulations.py
    ```
    The result will be stored in `result` directory.

2. We provide a python script named `generate_figure.py` to analyze results and generate figures from it.
    ```
    docker run --rm -it \
        -v $(pwd)/result:/ns-allinone-2.35/ns-2.35/outputs \
        ghcr.io/kaist-ina/ns2-flexpass \
        python ./generate_figure.py
    ```
    The CSV files and figures (in PNG) will be stored in the `result` directory.


## Major modifications from the vanilla ns-2.35

- Files in `xpass`:

  Core FlexPass and ExpressPass implementation.

- Files in `workloads`:

  Example workload files for the simulation.

- `queue/broadcom-node.cc` and `queue/broadcom-node.h`:

  Implementation for a Broadcom ASIC switch model. These includes selective dropping for TLT, dynamic buffer management. This is the ns-2 backport from [here](https://github.com/kaist-ina/ns3-tlt-tcp-public).

  *Disclaimer: this module is purely based on authors' personal understanding of Broadcom ASIC. It does not reflect any official confirmation from either Microsoft or Broadcom.*

- Other files in `queue`:

  Implementation of queues that support ExpressPass and FlexPass.

- `tcp/tcp-xpass.cc` and `tcp/tcp-xpass.h`:

  Layering (LY) implementation of ExpressPass.

## Credit
This code repository is based on [https://github.com/kaist-ina/ns2-xpass](https://github.com/kaist-ina/ns2-xpass).