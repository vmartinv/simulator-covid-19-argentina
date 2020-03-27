#!/usr/bin/env python3
import pandas as pd
import random
import itertools
from tqdm import tqdm
import geopandas as gpd
from utils.utils import normalize_dpto_name, validate_dpto_indexes
import os

DATA_DIR = os.path.join('data', 'argentina')
CENSO_HDF = os.path.join(DATA_DIR, 'censo-2010', 'censo.hdf5')
PXLOC = os.path.join(DATA_DIR, 'indec', 'pxdptodatosok.shp')

class Person:
    def __init__(self, id, family, edad, sexo, estudia, trabaja):
        self.id = id
        self.family = family
        self.edad = edad
        self.sexo = sexo
        self.estudia = estudia
        self.trabaja = trabaja

class Population:
    def __init__(self):
        self.people = []
        self.families = []
    def to_hdf(self, hdf_file = os.path.join(DATA_DIR, 'fake_population.hdf')):
        data = {
            'id': [p.id for p in self.people],
            'family': [p.family for p in self.people],
            'edad': [p.edad for p in self.people],
            'sexo': [p.sexo for p in self.people],
            'estudia': [p.estudia for p in self.people],
            'trabaja': [p.trabaja for p in self.people],
        }
        df = pd.DataFrame(data, columns=data.keys())
        df.to_hdf(hdf_file, 'population', mode='a')


class GenWithDistribution:
    def __init__(self, desired_cols, row):
        self.cols = desired_cols
        self.cum_weights = list(itertools.accumulate([int(row[c]) for c in desired_cols]))
    def _remove_first_point(col):
        return col[col.find('.')+1:]
    def get(self, k=1):
        return list(map(GenWithDistribution._remove_first_point,
                   random.choices(self.cols, cum_weights = self.cum_weights, k = k)
            ))

def cross_cols(a, b):
    return {c: [f'{c}.{c2}' for c2 in b] for c in a}

def load_population_census(location_file = PXLOC, census_file = CENSO_HDF):
    geodata = gpd.read_file(location_file, encoding='utf-8')
    geodata['departamen'] = [normalize_dpto_name(n) for n in geodata['departamen']]
    geodata['link'] = [int(n) for n in geodata['link']]
    desired_tables = set([
        '/hogares_urbano_vs_rural',
        '/personas_por_hogar',
        '/parentesco_jefe_cross_tamanio_familia',
        '/parentesco_jefe_cross_sexo',
        '/parentesco_jefe_cross_edad',
        '/edad_cross_escolaridad',
        '/edad_cross_trabaja',
    ])
    genpop_dataset = geodata
    with pd.HDFStore(census_file, mode='r') as hdf:
        for k in desired_tables:
            table = hdf.select(k)
            validate_dpto_indexes(table['area'], geodata['link'])
            genpop_dataset = pd.merge(genpop_dataset, table, how='inner', left_on = 'link', right_on = 'area')
    return genpop_dataset

def generate(genpop_dataset = load_population_census()):
    population = Population()
    tamanios_familia = ['1', '2', '3', '4', '5', '6', '7', '8 y más']
    parentescos = ['Cónyuge o pareja', 'Hijo(a) / Hijastro(a)', 'Jefe(a)', 'Nieto(a)', 'Otros familiares', 'Otros no familiares', 'Padre / Madre / Suegro(a)', 'Servicio doméstico y sus familiares', 'Yerno / Nuera']
    edad = list(map(str, range(111)))
    sexo = ['Mujer', 'Varón']
    escuela = ['Asiste', 'Asistió', 'Nunca asistió']
    trabaja = ['Desocupado', 'Inactivo', 'Ocupado']
    tamanio_cross_parentescos = cross_cols(tamanios_familia, parentescos)
    parentescos_cross_edad = cross_cols(parentescos, edad)
    parentescos_cross_sexo = cross_cols(parentescos, sexo)
    edad_cross_escuela = cross_cols(filter(lambda e: int(e)>=3, edad), escuela)
    edad_cross_trabaja = cross_cols(filter(lambda e: int(e)>=14, edad), trabaja)
    with tqdm(total=40e6, unit="people") as progress:
        for index, row in genpop_dataset.iterrows():
            tamanios = GenWithDistribution(tamanios_familia, row).get(k = int(row['hogares']))
            parentescos = {k: GenWithDistribution(v, row) for k, v in tamanio_cross_parentescos.items()}
            edades = {k: GenWithDistribution(v, row) for k, v in parentescos_cross_edad.items()}
            sexos = {k: GenWithDistribution(v, row) for k, v in parentescos_cross_sexo.items()}
            escuelas = {k: GenWithDistribution(v, row) for k, v in edad_cross_escuela.items()}
            trabajos = {k: GenWithDistribution(v, row) for k, v in edad_cross_trabaja.items()}
            for tam_flia in tamanios:
                tam_flia_num = int(tam_flia.replace('8 y más', '8'))
                parentescos_flia = parentescos[tam_flia].get(k=tam_flia_num)
                family_id = len(population.families)
                population.families.append([])
                for member in parentescos_flia:
                    edad = edades[member].get()[0]
                    sexo = sexos[member].get()[0]
                    estudia = escuelas[edad].get() if int(edad)>=3 else 'Nunca asistió'
                    estudia_bool = estudia == 'Asiste'
                    trabaja = trabajos[edad].get() if int(edad)>=14 else 'Inactivo'
                    trabaja_bool = trabajos == 'Ocupado'
                    id = len(population.people)
                    population.families[-1].append(id)
                    population.people.append(Person(id, family_id, edad, sexo, estudia_bool, trabaja_bool))
                    progress.update()
        return population


def main():
    generate().to_hdf()

if __name__ == "__main__":
    main()
