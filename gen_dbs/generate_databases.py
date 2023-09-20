#!/usr/bin/env python3
import requests
import os
import zipfile
import fake_population_generator
import glob
import shutil
import convert_school_db
import gen_censo_db

DATA_DIR = os.path.join('data', 'argentina')

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
    INDEC_DIR = os.path.join(DATA_DIR, 'indec')
    ensure_dir_exists(INDEC_DIR)
    exts = ['.shp', '.shx', '.dbf']

    ZIP_SOURCE = 'https://www.indec.gob.ar/ftp/cuadros/territorio/codgeo/Codgeo_Pais_x_loc_con_datos.zip'
    ZIP_FILE = os.path.join(INDEC_DIR, 'pxlocdatos.zip')
    BASE_FILE = 'Codgeo_Pais_x_loc_con_datos/pxlocdatos'
    for ext in exts:
        dst = os.path.join(INDEC_DIR, os.path.basename(BASE_FILE+ext))
        if not os.path.exists(dst):
            download_and_extract(ZIP_SOURCE, ZIP_FILE, BASE_FILE+ext, INDEC_DIR)
            shutil.copyfile(os.path.join(INDEC_DIR, BASE_FILE+ext), dst)

    ZIP_SOURCE = 'https://sitioanterior.indec.gob.ar/ftp/cuadros/territorio/codgeo/Codgeo_Pais_x_dpto_con_datos.zip'
    ZIP_FILE = os.path.join(INDEC_DIR, 'pxdptodatosok.zip')
    BASE_FILE = 'pxdptodatosok'
    for ext in exts:
        download_and_extract(ZIP_SOURCE, ZIP_FILE, BASE_FILE+ext, INDEC_DIR)

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
        ('fake_population_small', 82, 1.0),
        ('fake_population', None, 1.0),
    ]
    for name, prov_id, frac in POPULATIONS:
        basefile = os.path.join(DATA_DIR, name)
        output_files = [basefile+ext for ext in ['.dat', '.json', '.gpkg']]
        if any(not os.path.exists(f) for f in output_files):
            print(f"Generating {basefile}...")
            fake_population_generator.generate(prov_id=prov_id, frac=frac).to_dat(*output_files)

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

def store_transporte():
    TRANSPORTE_DIR = os.path.join(DATA_DIR, 'transporte')
    ensure_dir_exists(TRANSPORTE_DIR)
    URL = "https://ide.transporte.gob.ar/geoserver/observ/ows?service=WFS&version=1.0.0&request=GetFeature&typeName=observ:_3.4.1.4.1.tmda_17_18_view&maxFeatures=1550&outputFormat=application%2Fjson"
    dst = os.path.join(TRANSPORTE_DIR, "tdma2017.geojson")
    download_url(URL, dst)

def store_censo():
    censo_dir = os.path.join(DATA_DIR, 'censo-2010')
    ensure_dir_exists(censo_dir)
    hdf = os.path.join(censo_dir, 'censo.hdf5')
    if not os.path.exists(hdf):
        gen_censo_db.store_all(censo_dir, hdf)

def store_covidar():
    dst = os.path.join(DATA_DIR, 'covid19ardata.csv')
    url = 'https://docs.google.com/spreadsheets/d/16-bnsDdmmgtSxdWbVMboIHo5FRuz76DBxsz_BbsEVWA/export?format=csv&id=16-bnsDdmmgtSxdWbVMboIHo5FRuz76DBxsz_BbsEVWA&gid=0'
    download_url(url, dst)

def store_all():
    ensure_dir_exists(DATA_DIR)
    store_indec()
    store_densidad()
    store_censo()
    store_schools()
    store_transporte()
    store_fake_population()
    store_covidar()

if __name__ == "__main__":
    store_all()
