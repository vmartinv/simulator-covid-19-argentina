#!/usr/bin/env python3
import requests
import os
import zipfile
from simpledbf import Dbf5

DATA_DIR = os.path.join('data', 'argentina')
INDEC_DIR = os.path.join(DATA_DIR, 'indec')

def ensure_dir_exists(dir):
    if not os.path.exists(dir):
        os.makedirs(dir)
    
def download_url(url, save_path, chunk_size=128):
    if not os.path.exists(save_path):
        print(f'Downloading {save_path}...')
        r = requests.get(url, stream=True)
        r.raise_for_status()
        with open(save_path, 'wb') as fd:
            for chunk in r.iter_content(chunk_size=chunk_size):
                fd.write(chunk)

def get_indec():
    ZIP_SOURCE = 'https://sitioanterior.indec.gob.ar/ftp/cuadros/territorio/codgeo/Codgeo_Pais_x_dpto_con_datos.zip'
    ZIP_FILE = 'pxdptodatosok.zip'
    DBF_FILE = 'pxdptodatosok.dbf'
    OUTPUT_FILE = 'pxdptodatosok.hdf5'
    ensure_dir_exists(INDEC_DIR)
    if not os.path.exists(os.path.join(INDEC_DIR, OUTPUT_FILE)):
        # get dbf file
        if not os.path.exists(os.path.join(INDEC_DIR, DBF_FILE)):
            download_url(ZIP_SOURCE, os.path.join(INDEC_DIR, ZIP_FILE))
            with zipfile.ZipFile(ZIP_FILE, 'r') as zip_ref:
                zip_ref.extract(DBF_FILE, INDEC_DIR)
        
        dbf = Dbf5(os.path.join(INDEC_DIR, DBF_FILE), codec='utf8')
        dbf.to_pandashdf(os.path.join(INDEC_DIR, OUTPUT_FILE))

def get_all():
    ensure_dir_exists(DATA_DIR)
    get_indec()

if __name__ == "__main__":
    get_all()
