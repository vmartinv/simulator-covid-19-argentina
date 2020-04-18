# COVID-19 Simulation in Argentina population

An under construction [SEIR simulation](https://en.wikipedia.org/wiki/Compartmental_models_in_epidemiology) using census, education and transport databases of Argentina. The databases are obtained from public and government sources.

## Description of the model
- *[docs/model_specification.pdf](https://github.com/vmartinv/simulator-covid-19-argentina/raw/master/docs/model_specification.pdf)* for a mathematical description of the model (Spanish).
- *[Google Slides](https://docs.google.com/presentation/d/1cNZLiriVJxIJajvodh8ViYtx37NB8REJ2n5osU5jKA0/edit?usp=sharing)* for a short description of the project (Spanish).

## Requirements
Install [miniconda](https://docs.conda.io/en/latest/miniconda.html) and run:

    conda env create -f conda_environment.yml -n covid-ar python=3
    conda activate covid-ar
    export PATH=${CONDA_PREFIX}/bin:$PATH
    ./generate_databases.py

## Usage
See different notebooks experiments (`.ipynb` files) with [jupyter](https://jupyter.org):

        jupyter notebook
