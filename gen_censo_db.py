#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import convert_censo_xls_to_hdf5
import os
import mechanize
import pandas as pd
import os
import re
import glob
import pandas as pd
import errno
from bs4 import BeautifulSoup

def download_all(censo_dir):
    viviendas_url = 'https://redatam.indec.gob.ar/argbin/RpWebEngine.exe/Frequency?&BASE=CPV2010B&ITEM=FREQVIV&MAIN=WebServerMain.inl'
    hogares_url = 'https://redatam.indec.gob.ar/argbin/RpWebEngine.exe/Frequency?&BASE=CPV2010B&ITEM=FREQHOG&MAIN=WebServerMain.inl'
    pop_url = 'https://redatam.indec.gob.ar/argbin/RpWebEngine.exe/Frequency?&BASE=CPV2010B&ITEM=FREQHOG&MAIN=WebServerMain.inl'
    cross_url = 'https://redatam.indec.gob.ar/argbin/RpWebEngine.exe/EasyCross?&BASE=CPV2010B&ITEM=CRUZTOT&MAIN=WebServerMain.inl'
    datasets = [
        {
            'out': os.path.join(censo_dir, 'hogares_urbano_vs_rural.xls'),
            'url': viviendas_url,
            'params': {'VARIABLE': 'VIVIENDA.URP', 'AREABREAK': 'DPTO'},
        },
        {
            'out': os.path.join(censo_dir, 'personas_por_hogar.xls'),
            'url': hogares_url,
            'params': {'VARIABLE': 'HOGAR.TOTPERS', 'AREABREAK': 'DPTO'},
        },
        {
            'out': os.path.join(censo_dir, 'parentesco_jefe_cross_tamanio_familia.xls'),
            'url': cross_url,
            'params': {'VAR1': 'HOGAR.TOTPERS', 'VAR2': 'PERSONA.P01', 'AREABREAK': 'DPTO'},
        },
        {
            'out': os.path.join(censo_dir, 'parentesco_jefe_cross_sexo.xls'),
            'url': cross_url,
            'params': {'VAR1': 'PERSONA.P01', 'VAR2': 'PERSONA.P02', 'AREABREAK': 'DPTO'},
        },
        {
            'out': os.path.join(censo_dir, 'parentesco_jefe_cross_edad.xls'),
            'url': cross_url,
            'params': {'VAR1': 'PERSONA.P01', 'VAR2': 'PERSONA.P03', 'AREABREAK': 'DPTO'},
        },
        {
            'out': os.path.join(censo_dir, 'edad_cross_escolaridad.xls'),
            'url': cross_url,
            'params': {'VAR1': 'PERSONA.P03', 'VAR2': 'PERSONA.P1808', 'AREABREAK': 'DPTO'},
        },
        {
            'out': os.path.join(censo_dir, 'edad_cross_trabaja.xls'),
            'url': cross_url,
            'params': {'VAR1': 'PERSONA.P03', 'VAR2': 'PERSONA.CONDACT', 'AREABREAK': 'DPTO'},
        },
    ]
    for db in datasets:
        download_report(**db)

def download_report(out, url, params):
    if os.path.exists(out):
        return
    print(f"Downloading {out}...")
    browser=mechanize.Browser()
    browser.addheaders = [('User-agent', 'Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.9.0.1) Gecko/2008071615 Fedora/3.0.1-1.fc9 Firefox/3.0.1')]
    browser.open(url)
    browser.select_form(nr=0)
    for k,v in params.items():
        browser.form[k] = [v,]
    browser.submit()
    soup = BeautifulSoup(browser.response().read(), features="lxml")
    frame = soup.find('iframe', {'name': "grid"})["src"]
    # browser.open(frame)
    # browser.follow_link(text='Descargar en formato Excel')
    m = re.match(r'(https://.*redatam.*)/Text\?LFN=([^\.]+).*', frame)
    browser.open(f"{m.group(1)}/reporte.xls?LFN={m.group(2)}.xls")
    with open(out, 'wb') as fout:
        fout.write(browser.response().read())

def convert_all(censo_dir, hdf_file):
    print(f"Generating {hdf_file}...")
    INDEX = 'area'
    combined_dataset = None
    try:
        os.remove(hdf_file)
    except OSError as e:
        if e.errno != errno.ENOENT:
            raise e
    summary = {}
    for xls in sorted(glob.glob(os.path.join(censo_dir, '*.xls'))):
        (filename, ext) = os.path.splitext(xls)
        print(f"Converting {xls}...")
        dataset = convert_censo_xls_to_hdf5.parse_xls(xls)
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
    with open(os.path.join(censo_dir, 'README.md'), 'w') as f:
        f.write('Generado a partir del censo 2010 utilizando https://redatam.indec.gob.ar/\n\n')
        f.write('Fuente: elaboración propia en base a datos del INDEC. Censo Nacional de Población, Hogares y Viviendas 2010, procesado con Redatam+SP\n\n')
        f.write('\n\n')
        f.write(f'# Descripcion de {hdf_file}\n\n')
        f.write('Las tablas son indexadas por departamento.\n\n')
        for t in summary:
            f.write(f'\nTabla {t}, columnas:\n\n')
            for col in sorted(summary[t]):
                f.write(f'    - {col}\n\n')

def store_all(censo_dir, hdf):
    download_all(censo_dir)
    convert_all(censo_dir, hdf)

def main():
    censo_dir = os.path.join('data', 'argentina', 'censo-2010')
    store_all(censo_dir, os.path.join(censo_dir, 'censo.hdf5'))
    return 0

if __name__ == "__main__":
    exit(main())
