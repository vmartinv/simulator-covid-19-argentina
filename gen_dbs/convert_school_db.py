#!/usr/bin/env python3
import os
import xlrd
import pandas as pd
import re
from sys import argv

def convert_xlsx_to_hdf(xls, hdf_file):
    worksheet = xlrd.open_workbook(xls).sheet_by_index(0)
    TABLE_START = 'ID'
    headers = None
    dataset = None
    for row in range(worksheet.nrows):
        first_col = str(worksheet.cell(row, 0).value).strip()
        if headers is not None and first_col:
            for i,header in enumerate(headers):
                value = str(worksheet.cell(row, i).value).strip()
                dataset[header].append(value)
        if headers is None and first_col == TABLE_START:
            header_height = 1
            while not str(worksheet.cell(row+header_height, 0).value).strip():
                header_height += 1
            last_col = []
            headers = []
            def this_col_contents():
                inserted = False
                col = []
                for j in range(header_height):
                    value = str(worksheet.cell(row+j, len(headers)).value).strip()
                    if not inserted and not value and len(last_col)>j:
                        value = last_col[j]
                    elif value:
                        inserted = True
                    if value:
                        col.append(value)
                if not col or not inserted:
                    return None
                return col
            while len(headers)<worksheet.ncols and this_col_contents() is not None:
                cur_col = this_col_contents()
                headers.append('.'.join(cur_col))
                last_col = cur_col
            dataset = {h: [] for h in headers}
    for h in headers:
        if all(re.match(r'\d*(?:\.0)?$', v) for v in dataset[h]):
            dataset[h] = [int(float(v)) if v else 0 for v in dataset[h]]
        elif all(re.match(r'X?$', v) for v in dataset[h]):
            dataset[h] = [bool(v) for v in dataset[h]]
    df = pd.DataFrame(dataset, columns=dataset.keys())
    (filename, ext) = os.path.splitext(hdf_file)
    name = os.path.basename(filename).replace('-', '_')
    df.to_hdf(hdf_file, name, mode='w')

def main():
    if len(argv) != 3 or not argv[1].endswith(".xlsx") or not argv[2].endswith(".hdf"):
        print(f"""
Usage:  {argv[0]} <XLSX_FILE> <HDF_FILE>
""".strip())
        exit(1)
    convert_xlsx_to_hdf(argv[1], argv[2])

if __name__ == "__main__":
    main()
