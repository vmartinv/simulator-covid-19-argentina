#!/usr/bin/env python3
import requests
import os
import zipfile

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

def download_and_extract(url, save_zip, save_file, path):
    if not os.path.exists(save_file):
        download_url(url, save_zip)
        with zipfile.ZipFile(save_zip, 'r') as zip_ref:
            zip_ref.extract(save_file, path)

def get_indec():
    ensure_dir_exists(INDEC_DIR)
    ZIP_SOURCE = 'https://sitioanterior.indec.gob.ar/ftp/cuadros/territorio/codgeo/Codgeo_Pais_x_dpto_con_datos.zip'
    ZIP_FILE = os.path.join(INDEC_DIR, 'pxdptodatosok.zip')

    SHP_FILE = 'pxdptodatosok.shp'
    download_and_extract(ZIP_SOURCE, ZIP_FILE, SHP_FILE, INDEC_DIR)
    download_and_extract(ZIP_SOURCE, ZIP_FILE, SHP_FILE.replace('.shp', '.shx'), INDEC_DIR)

def get_all():
    ensure_dir_exists(DATA_DIR)
    get_indec()

if __name__ == "__main__":
    get_all()