#!/usr/bin/env python3
import pandas as pd
import random
import itertools
import functools
from tqdm import tqdm
import geopandas as gpd
from utils.utils import normalize_dpto_name, validate_dpto_indexes
import os
import re
from collections import defaultdict
import struct

DATA_DIR = os.path.join('data', 'argentina')
CENSO_HDF = os.path.join(DATA_DIR, 'censo-2010', 'censo.hdf5')
PXLOC = os.path.join(DATA_DIR, 'datosgobar-densidad-poblacion', 'pais.geojson')

class Person:
    def __init__(self, id, family, zone, edad, sexo, estudia, trabaja):
        self.id = id
        self.family = family
        self.zone = zone
        self.edad = edad
        self.sexo = sexo
        self.estudia = estudia
        self.trabaja = trabaja
    
    def pack(self):
        struct_format = '>IIHB???'
        return struct.pack(struct_format, self.id, self.family, self.zone, int(self.edad), self.sexo == 'Mujer', self.estudia, self.trabaja)

class Population:
    def __init__(self):
        self.people = []
        self.families = []
        self.zones = []
    
    def to_dat(self, dat_file):
        with open(dat_file, mode='wb') as fout:
            for p in tqdm(self.people):
                fout.write(p.pack())

    def to_hdf(self, hdf_file):
        data = {
            'id': pd.Series([int(p.id) for p in self.people], dtype='int32'),
            'family': pd.Series([int(p.family) for p in self.people], dtype='int32'),
            'zone': pd.Series([int(p.zone) for p in self.people], dtype='int32'),
            'edad': pd.Series([int(p.edad) for p in self.people], dtype='int8'),
            'sexo': [p.sexo == 'Mujer' for p in self.people],
            'estudia': [p.estudia for p in self.people],
            'trabaja': [p.trabaja for p in self.people],
        }
        df = pd.DataFrame(data, columns=data.keys())
        df.to_hdf(hdf_file, 'population', mode='w')


class GenWithDistribution:
    def __init__(self, desired_cols, row, mx=None):
        self.cols = desired_cols
        self.cum_weights = list(itertools.accumulate([int(row[c]) for c in desired_cols]))
        self.precalc = []
        self.cur_index = 0
    def _remove_first_point(col):
        return col[col.find('.')+1:]
    def get(self, k=1):
        if self.cur_index+k > len(self.precalc):
            next_size = max(2*k, max(100, 2*len(self.precalc)))
            self.precalc = list(map(GenWithDistribution._remove_first_point,
                   random.choices(self.cols, cum_weights = self.cum_weights, k = next_size)))
            self.cur_index = 0
        self.cur_index += k
        return self.precalc[self.cur_index - k: self.cur_index]

def cross_cols(a, b):
    return {c: [f'{c}.{c2}' for c2 in b] for c in a}

def load_population_census(location_file = PXLOC, census_file = CENSO_HDF):
    geodata = gpd.read_file(location_file, encoding='utf-8')
    #geodata['departamen'] = [normalize_dpto_name(n) for n in geodata['departamen']]
    geodata['area'] = [float(a) for a in geodata['area']]
    geodata['hogares'] = [int(re.sub(r'(\d+).0+', r'\1', x)) if x else 0 for x in geodata['hogares']]
    geodata['dpto_id'] = [int(n) for n in geodata['dpto_id']]
    desired_tables = set([
        '/hogares_urbano_vs_rural',
        '/personas_por_hogar',
        '/parentesco_jefe_cross_tamanio_familia',
        '/parentesco_jefe_cross_sexo',
        '/parentesco_jefe_cross_edad',
        '/edad_cross_escolaridad',
        '/edad_cross_trabaja',
    ])
    tables = []
    with pd.HDFStore(census_file, mode='r') as hdf:
        for k in desired_tables:
            table = hdf.select(k)
            # validate_dpto_indexes(table['area'], geodata['dpto_id'])
            tables.append(table)
    return geodata, functools.reduce(lambda a,b: pd.merge(a, b, how='inner', on='area'), tables)

def generate(genpop_dataset = load_population_census(), frac=1.):
    geodata, census = genpop_dataset
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
    tamanios = {}
    parentescos = {}
    edades = {}
    sexos = {}
    escuelas = {}
    trabajos = {}
    print("Generating distributions by deparment...")
    for index, row in tqdm(census.iterrows(), total=len(census)):
        tamanios[row['area']] = GenWithDistribution(tamanios_familia, row)
        parentescos[row['area']] = {k: GenWithDistribution(v, row) for k, v in tamanio_cross_parentescos.items()}
        edades[row['area']] = {k: GenWithDistribution(v, row) for k, v in parentescos_cross_edad.items()}
        sexos[row['area']] = {k: GenWithDistribution(v, row) for k, v in parentescos_cross_sexo.items()}
        escuelas[row['area']] = {k: GenWithDistribution(v, row) for k, v in edad_cross_escuela.items()}
        trabajos[row['area']] = {k: GenWithDistribution(v, row) for k, v in edad_cross_trabaja.items()}

    print("Generating population...")
    with tqdm(total=40e6*frac, unit="people") as progress:
        for index, row in geodata.sample(frac=frac).iterrows():
            zone_id = len(population.zones)
            population.zones.append(row['geometry'])
            tamanios_flias = tamanios[row['dpto_id']].get(k = int(row['hogares']))
            parentescos_d = parentescos[row['dpto_id']]
            edades_d = edades[row['dpto_id']]
            sexos_d = sexos[row['dpto_id']]
            escuelas_d = escuelas[row['dpto_id']]
            trabajos_d = trabajos[row['dpto_id']]
            by_parentesco = defaultdict(list)
            for tam_flia in tamanios_flias:
                tam_flia_num = int(tam_flia.replace('8 y más', str(random.choices(range(8, 16))[0])))
                parentescos_flia = parentescos_d[tam_flia].get(k=tam_flia_num)
                family_id = len(population.families)
                population.families.append([])
                for member in parentescos_flia:
                    id = len(population.people)
                    population.families[-1].append(id)
                    by_parentesco[member].append(id)
                    population.people.append(Person(id, family_id, zone_id, None, None, False, False))
                    progress.update()
            by_edad = defaultdict(list)
            for parentesco, people in by_parentesco.items():
                edades_par = edades_d[parentesco].get(len(people))
                sexo_par = sexos_d[parentesco].get(len(people))
                for p,edadv,sexov in zip(people, edades_par, sexo_par):
                    population.people[p].edad = edadv
                    population.people[p].sexo = sexov
                    by_edad[edadv].append(p)
            for edadv, people in by_edad.items():
                if int(edadv)>=3:
                    estudian = escuelas_d[edadv].get(len(people))
                    for p,estudiav in zip(people, estudian):
                        population.people[p].estudia = estudiav == 'Asiste'
                if int(edadv)>=14:
                    trabajan = trabajos_d[edadv].get(len(people))
                    for p,trabajav in zip(people, trabajan):
                        population.people[p].trabaja = trabajav == 'Ocupado'
        return population


def main():
    generate(frac=0.01).to_dat(os.path.join(DATA_DIR, 'fake_population_small.dat'))

if __name__ == "__main__":
    main()
