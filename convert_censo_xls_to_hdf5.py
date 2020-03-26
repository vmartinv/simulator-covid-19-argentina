#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import xlrd
import os
import glob
import pandas as pd
import errno

DATA_DIR = os.path.join('data', 'argentina', 'censo-2010')
README_FILE = os.path.join(DATA_DIR, 'README.md')
DEBUG = False

def fix_encoding(text):
    fixes = [
        ('·', 'á'),
        ('È', 'é'),
        ('Ì', 'í'),
        ('Û', 'ó'),
        ('˙', 'ú'),
        ('Ò', 'ñ'),
        ('¸', 'ü'),
        ('∞', 'ro'),
        ('—', 'Ñ'),
    ]
    fixed = text
    for orig, repl in fixes:
        fixed = fixed.replace(orig, repl)
    for c in list(fixed):
        if c not in [r for o,r in fixes]+list(" .'-()/") and not c.isalnum():
            print(f"Unrecognized character: {c} in {fixed}")
            exit(1)
    return fixed

INDEX = 'area'

def parse_xls(xls):
    worksheet = xlrd.open_workbook(xls).sheet_by_index(0)
    SUBTABLE_START = "AREA"
    SUBTABLE_END = "Total"
    SUBTABLE_EMPTY = "Tabla vac"
    area_row = None
    dataset = {
        INDEX: []
    }
    for row in range(worksheet.nrows):
        first_col = str(worksheet.cell(row, 0).value)
        if area_row is None:
            if first_col.startswith(SUBTABLE_START):
                name = worksheet.cell(row, 1).value
                dataset[INDEX].append(name)
                area_row = row
        else:
            if first_col.startswith(SUBTABLE_END):
                for k in dataset:
                    if len(dataset[k])<len(dataset[INDEX]):
                        dataset[k].append('0')
                area_row = None
            elif first_col.startswith(SUBTABLE_EMPTY):
                for k in dataset:
                    if len(dataset[k])<len(dataset[INDEX]):
                        dataset[k].append('0')
                area_row = None
            elif row > area_row + 2: # actual rows
                if first_col not in dataset:
                    dataset[first_col] = []
                value = worksheet.cell(row, 1).value
                if not value:
                    print(f"cannot parse row {row}")
                    exit(1)
                dataset[first_col].append(value)
    if len(dataset[INDEX]) < 100:
        return None
    dataset_fixed = {}
    for k in dataset:
        if k != INDEX:
            dataset_fixed[fix_encoding(k)] = [int(v) for v in dataset[k]]
        else:
            dataset_fixed[k] = [fix_encoding(v) for v in dataset[k]]
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
                assert(dataset[INDEX] == combined_dataset[INDEX])
            df = pd.DataFrame(dataset, columns=dataset.keys())
            df.to_hdf(hdf_file, name, mode='a')
            print(f"Saved to {hdf_file}")
        else:
            print(f"Warning: not parsing file {xls}")
    with open(README_FILE, 'w') as f:
        f.write('Generado a partir del censo 2010 utilizando https://redatam.indec.gob.ar/argbin/RpWebEngine.exe/PortalAction?&MODE=MAIN&BASE=CPV2010A&MAIN=WebServerMain.inl\n')
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
