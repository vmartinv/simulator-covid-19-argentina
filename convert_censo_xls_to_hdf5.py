#!/usr/bin/env python3
# -*- coding: utf-8 -*-
from sylk_parser import SylkParser
import os
import glob
import pandas as pd
import errno
import re
import argparse

INDEX = 'area'

def format_col(val):
    return re.sub(r' +', ' ', re.sub(r'(\d+)\.0', r'\1', val))

def parse_xls(xls):
    worksheet = SylkParser(xls)
    SUBTABLE_START = "AREA"
    SUBTABLE_END = "Total"
    SUBTABLE_EMPTY = "Tabla vac"
    area_row = None
    dataset = {}
    cross_cols = None
    current_index = None
    previous_row = None
    for i, row in enumerate(worksheet):
        first_col = str(row[0]).strip()
        if area_row is None:
            if first_col.startswith(SUBTABLE_START):
                number = int(re.match(fr"{SUBTABLE_START}\s*#\s*(\d+)\s*", first_col).group(1))
                current_index = number
                name = row[1]
                area_row = i
                header_row = None
        else:
            if first_col == SUBTABLE_END or first_col.startswith(SUBTABLE_EMPTY):
                if current_index not in dataset:
                    dataset[current_index] = {}
                area_row = None
            elif i > area_row + 2 and first_col: # actual rows
                if current_index not in dataset:
                    dataset[current_index] = {}
                    header_row = previous_row
                    first_header = str(header_row[1]).strip()
                    if first_header != 'Casos':
                        cross_cols = []
                        while str(header_row[1+len(cross_cols)]).strip() != 'Total':
                            cross_cols.append('.'+format_col(header_row[1+len(cross_cols)]))
                    else:
                        cross_cols = ['']
                for j,col in enumerate(cross_cols):
                    value = row[1+j]
                    if not value:
                        print(f"cannot parse row {row}")
                        exit(1)
                    dataset[current_index][format_col(first_col)+col] = value
        previous_row = row
    if len(dataset) < 100:
        return None
    dataset_fixed = {INDEX: []}
    for area, cols in dataset.items():
        for col in cols:
            dataset_fixed[col] = []
    for area, cols in dataset.items():
        dataset_fixed[INDEX].append(area)
        dataset[area] = {col: vals for col,vals in cols.items()}
        for col in dataset_fixed.keys():
            if col != INDEX:
                value = dataset[area][col] if col in dataset[area] else '0'
                value = int(format_col(value.replace('-', '0')))
                dataset_fixed[col].append(value)
    dataset = dataset_fixed
    return dataset

def convert_single(xls, dst):
    if not os.path.exists(xls):
        print(f"Error: file not found {xls}")
        return 1
    dataset = parse_xls(xls)
    if dataset is not None:
        name = os.path.basename(dst).replace('-', '_')
        df = pd.DataFrame(dataset, columns=dataset.keys())
        if dst.endswith('.hdf'):
            df.to_hdf(dst, name, mode='w')
        elif dst.endswith('.csv'):
            df.to_csv(dst, mode='w')
        else:
            print(f"Error: unknown output format for file {dst}")
            return 1
        print(f"Saved to {dst}")
        return 0
    else:
        print(f"Error: error parsing file {xls}")
        return 1

def main():
    argsp = argparse.ArgumentParser(description="""
Convert censo xls format to csv or hdf.
Steps to get the xls files:
    redatam.indec.gob.ar->
    Procesar en En-Linea->Poblacion y vivienda->
    Censo Nacional de Poblacion, Hogares y Viviendas 2010 - Basico->
    Resultados Basicos->
    Frecuencias or Cruces->
    Select desired parameters (only tested with Corte de area=deparmento)->
    Ejecutar->
    Descargar en formato Excel (at the bottom)
""".strip())

    argsp.add_argument('xls',
                        metavar='xls',
                        type=str,
                        help='Path to the xls generated from redatam')
    argsp.add_argument('output',
                        metavar='output',
                        type=str,
                        help='Path to the output file (csv or hdf)')
    args = argsp.parse_args()
    return convert_single(args.xls, args.output)

if __name__ == "__main__":
    exit(main())
