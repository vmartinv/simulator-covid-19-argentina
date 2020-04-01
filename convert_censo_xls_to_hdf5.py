#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import xlrd
import os
import glob
import pandas as pd
import errno
import re
from utils.utils import fix_encoding, normalize_dpto_name
DATA_DIR = os.path.join('data', 'argentina', 'censo-2010')
README_FILE = os.path.join(DATA_DIR, 'README.md')
DEBUG = False

INDEX = 'area'

def format_col(val):
    return re.sub(r' +', ' ', re.sub(r'(\d+)\.0', r'\1', val))

def parse_xls(xls):
    worksheet = xlrd.open_workbook(xls).sheet_by_index(0)
    SUBTABLE_START = "AREA"
    SUBTABLE_END = "Total"
    SUBTABLE_EMPTY = "Tabla vac"
    area_row = None
    dataset = {}
    cross_cols = None
    current_index = None
    for row in range(worksheet.nrows):
        first_col = str(worksheet.cell(row, 0).value).strip()
        if area_row is None:
            if first_col.startswith(SUBTABLE_START):
                number = int(re.match(fr"{SUBTABLE_START}\s*#\s*(\d+)\s*", first_col).group(1))
                current_index = number
                name = worksheet.cell(row, 1).value
                area_row = row
                header_row = None
        else:
            if first_col == SUBTABLE_END or first_col.startswith(SUBTABLE_EMPTY):
                if current_index not in dataset:
                    dataset[current_index] = {}
                area_row = None
            elif row > area_row + 2 and first_col: # actual rows
                if current_index not in dataset:
                    dataset[current_index] = {}
                    header_row = row - 1
                    first_header = str(worksheet.cell(header_row, 1).value).strip()
                    if first_header != 'Casos':
                        cross_cols = []
                        while str(worksheet.cell(header_row, 1+len(cross_cols)).value).strip() != 'Total':
                            cross_cols.append('.'+format_col(str(worksheet.cell(header_row, 1+len(cross_cols)).value)))
                    else:
                        cross_cols = ['']
                for i,col in enumerate(cross_cols):
                    value = str(worksheet.cell(row, 1+i).value)
                    if not value:
                        print(f"cannot parse row {row}")
                        exit(1)
                    dataset[current_index][format_col(first_col)+col] = value
    if len(dataset) < 100:
        return None
    dataset_fixed = {INDEX: []}
    for area, cols in dataset.items():
        for col in cols:
            dataset_fixed[fix_encoding(col)] = []
    for area, cols in dataset.items():
        dataset_fixed[INDEX].append(area)
        dataset[area] = {fix_encoding(col): vals for col,vals in cols.items()}
        for col in dataset_fixed.keys():
            if col != INDEX:
                value = dataset[area][col] if col in dataset[area] else '0'
                value = int(format_col(value.replace('-', '0')))
                dataset_fixed[col].append(value)
    dataset = dataset_fixed
    return dataset


def convert_all():
    combined_dataset = None
    hdf_file = os.path.join(DATA_DIR, 'censo.hdf5')
    try:
        os.remove(hdf_file)
    except OSError as e:
        if e.errno != errno.ENOENT:
            raise e
    summary = {}
    for xls in sorted(glob.glob(os.path.join(DATA_DIR, '*.xls'))):
        (filename, ext) = os.path.splitext(xls)
        print(f"Converting {xls}...")
        dataset = parse_xls(xls)
        if dataset is not None:
            name = os.path.basename(filename).replace('-', '_')
            summary[name] = set(dataset.keys())-set([INDEX])
            if combined_dataset is None:
                combined_dataset = dataset
            else:
                assert dataset[INDEX] == combined_dataset[INDEX], str(sorted(set(dataset[INDEX])^set(combined_dataset[INDEX])))
            df = pd.DataFrame(dataset, columns=dataset.keys())
            df.to_hdf(hdf_file, name, mode='a')
            print(f"Saved to {hdf_file}")
        else:
            print(f"Warning: not parsing file {xls}")
    with open(README_FILE, 'w') as f:
        f.write('Generado a partir del censo 2010 utilizando https://redatam.indec.gob.ar/\n')
        f.write('Fuente: elaboración propia en base a datos del INDEC. Censo Nacional de Población, Hogares y Viviendas 2010, procesado con Redatam+SP\n')
        f.write('\n')
        f.write('# Descripcion de {hdf_file}\n')
        f.write('Las tablas son indexadas por departamento.\n')
        for t in summary:
            f.write(f'\nTabla {t}, columnas:\n')
            for col in sorted(summary[t]):
                f.write(f'    - {col}\n')

if __name__ == "__main__":
    convert_all()
