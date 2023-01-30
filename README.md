# FlexPass (EuroSys 2023)

This is a Github repository for the artifact evaluation of the EuroSys submission "FlexPass: A Case for Flexible Credit-based Transport for Datacenter Networks".
The repository includes our implementation of FlexPass on ns-2 simulator.

## Quick Start
We provide a pre-built docker image at `ghcr.io/kaist-ina/ns2-flexpass`. Use the following command to set-up the docker image.
```bash
docker clone ghcr.io/kaist-ina/ns2-flexpass
```

## How to run experiments

1. We provide a python script named `/runs_simulations.py` to automate simulations. Use following command to run all simulations.
    ```
    docker run --rm -it \
        -v $(pwd)/result:/ns-allinone-2.35/ns-2.35/outputs \
        ghcr.io/kaist-ina/ns2-flexpass \
        python ./run_simulations.py
    ```
    The result will be stored in `result` directory.

2. We provide a python script named `/generate_figure.py` to analyze results and generate figures from it.
    ```
    docker run --rm -it \
        -v $(pwd)/result:/ns-allinone-2.35/ns-2.35/outputs \
        ghcr.io/kaist-ina/ns2-flexpass \
        python ./generate_figure.py
    ```
    The CSV files and figures (in PNG) will be stored in the `result` directory.

## Note
The current version of the simulator has not gone through refactoring. We plan to refactor the code and release a new version of the code soon.