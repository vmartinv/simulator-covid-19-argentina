#!/usr/bin/env python3
import requests
import os
import zipfile
import fake_population_generator
import glob
import shutil
import convert_school_db

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

def download_and_extract(url, save_zip, save_file=None, path=None):
    if  (save_file is None or not os.path.exists(os.path.join(path or '', save_file))):
        download_url(url, save_zip)
        with zipfile.ZipFile(save_zip, 'r') as zip_ref:
            if save_file is not None:
                zip_ref.extract(save_file, path)
            else:
                zip_ref.extractall(path)

def store_indec():
    ensure_dir_exists(INDEC_DIR)
    ZIP_SOURCE = 'https://sitioanterior.indec.gob.ar/ftp/cuadros/territorio/codgeo/Codgeo_Pais_x_dpto_con_datos.zip'
    ZIP_FILE = os.path.join(INDEC_DIR, 'pxdptodatosok.zip')

    SHP_FILE = 'pxdptodatosok.shp'
    download_and_extract(ZIP_SOURCE, ZIP_FILE, SHP_FILE, INDEC_DIR)
    download_and_extract(ZIP_SOURCE, ZIP_FILE, SHP_FILE.replace('.shp', '.shx'), INDEC_DIR)
    download_and_extract(ZIP_SOURCE, ZIP_FILE, SHP_FILE.replace('.shp', '.dbf'), INDEC_DIR)

def store_densidad():
    ZIP_SOURCE = "https://github.com/datosgobar/densidad-poblacion/archive/master.zip"
    ZIP_FILE = os.path.join(DATA_DIR, 'datosgobar-densidad-poblacion.zip')
    DEST_DIR = os.path.join(DATA_DIR, 'datosgobar-densidad-poblacion')
    ensure_dir_exists(DEST_DIR)
    if len(glob.glob(os.path.join(DEST_DIR, '*'))) != 10:
        download_and_extract(ZIP_SOURCE, ZIP_FILE, None, DEST_DIR)
        for file in glob.glob(os.path.join(DEST_DIR, 'densidad-poblacion-master', 'dataset', '*')):
            shutil.copy(file, DEST_DIR)
        shutil.rmtree(os.path.join(DEST_DIR, 'densidad-poblacion-master'))

def store_fake_population():
    POPULATIONS = [
        ('fake_population_small', 0.01),
        ('fake_population', 1.0),
    ]
    for name, frac in POPULATIONS:
        basefile = os.path.join(DATA_DIR, name)
        output_files = [basefile+ext for ext in ['.dat', '.json', '.gpkg']]
        if any(not os.path.exists(f) for f in output_files):
            print(f"Generating {basefile}...")
            fake_population_generator.generate(frac=frac).to_dat(*output_files)

def store_schools():
    EDUCACION_DIR = os.path.join(DATA_DIR, "ministerio-educacion")
    ensure_dir_exists(EDUCACION_DIR)
    # Taken from https://www.argentina.gob.ar/educacion/planeamiento/info-estadistica/bdd
    DBS = [
        ("https://www.argentina.gob.ar/sites/default/files/2018_base_usuaria_7-_matricula_por_edad.xlsx",
        "2018_base_usuaria_7-_matricula_por_edad.xlsx",
        "matricula_por_edad.hdf"
        ),
        ("https://www.argentina.gob.ar/sites/default/files/2018_base_usuaria_2-_matricula_y_secciones_0.xlsx",
        "2018_base_usuaria_2-_matricula_y_secciones_0.xlsx",
        "matricula_y_secciones.hdf"
        ),
    ]
    for url, xls, hdf in DBS:
        hdf = os.path.join(EDUCACION_DIR, hdf)
        if not os.path.exists(hdf):
            xls = os.path.join(EDUCACION_DIR, xls)
            download_url(url, xls)
            print(f'Generating {hdf}...')
            convert_school_db.convert_xlsx_to_hdf(xls, hdf)


def store_all():
    ensure_dir_exists(DATA_DIR)
    store_indec()
    store_densidad()
    store_schools()
    store_fake_population()

if __name__ == "__main__":
    store_all()
