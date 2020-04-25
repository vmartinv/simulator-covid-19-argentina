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
import json
import numpy as np
from collections import defaultdict
import struct
from scipy.spatial import cKDTree

DATA_DIR = os.path.join('data', 'argentina')
CENSO_HDF = os.path.join(DATA_DIR, 'censo-2010', 'censo.hdf5')
PXLOC = os.path.join(DATA_DIR, 'datosgobar-densidad-poblacion', 'pais.geojson')
SCHOOL_HDF = os.path.join(DATA_DIR, 'ministerio-educacion', 'matricula_y_secciones.hdf')

class Person:
    def __init__(self, id, family, edad, sexo, escuela, trabajo):
        self.id = id
        self.family = family
        self.edad = edad
        self.sexo = sexo
        self.escuela = escuela
        self.trabajo = trabajo

    def pack(self):
        struct_format = '>IB?II'
        return struct.pack(struct_format, self.family, int(self.edad), self.sexo == 'Mujer', self.escuela, self.trabajo)

class Family:
    def __init__(self, id, zone, dpto, prov):
        self.id = id
        self.zone = zone
        self.dpto = dpto
        self.prov = prov

    def pack(self):
        struct_format = '>HHH'
        return struct.pack(struct_format, self.zone, int(self.dpto), int(self.prov))


class Population:
    def __init__(self):
        self.people = []
        self.families = []
        self.nearest_zones = []
        self.nearest_densities = []
        self.geodata = None
        self.dpto_map = {}
        self.prov_map = {}

    def get_dpto_id(self, name):
        if name not in self.dpto_map:
            self.dpto_map[name] = len(self.dpto_map)
        return self.dpto_map[name]

    def get_prov_id(self, name):
        if name not in self.prov_map:
            self.prov_map[name] = len(self.prov_map)
        return self.prov_map[name]

    def calculate_ids(self):
        self.departments = [i[0] for i in sorted(self.dpto_map.items(), key=lambda i: i[1])]
        self.provinces = [i[0] for i in sorted(self.prov_map.items(), key=lambda i: i[1])]

    def to_dat(self, dat_file, json_file, geopackage_file):
        with open(json_file, 'w') as fout:
            json.dump({
                'nearest_zones': self.nearest_zones,
                'nearest_densities': self.nearest_densities,
                'department_densities': self.department_densities,
                'province_densities': self.province_densities,
                'departaments': self.departments,
                'provinces': self.provinces,
            }, fout)
        self.geodata.to_file(geopackage_file, driver="GPKG")
        with open(dat_file, mode='wb') as fout:
            fout.write(struct.pack(">I", len(self.families)))
            for p in tqdm(self.families):
                fout.write(p.pack())
            fout.write(struct.pack(">I", len(self.people)))
            for p in tqdm(self.people):
                fout.write(p.pack())


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

class SchoolIdGenerator:
    def __init__(self):
        self.cur_idx = 0
    def get_school(self):
        self.cur_idx += 1
        return self.cur_idx

class AlumnSchoolIdGenerator:
    def __init__(self, school_gen, mean):
        self.school_gen = school_gen
        try:
            self.mean = int(round(mean))
        except:
            self.mean = 100 #TODO: WHY I GET NAN
        self.cur_capacity = -1
        self.cur_size = 0
        self.cur_idx = 0

    def _gen_new_school(self):
        self.cur_capacity = self.mean
        self.cur_size = 0
        self.cur_idx = self.school_gen.get_school()

    def get_school(self):
        if self.cur_size >= self.cur_capacity:
            self._gen_new_school()
        self.cur_size += 1
        return self.cur_idx


def cross_cols(a, b):
    return {c: [f'{c}.{c2}' for c2 in b] for c in a}

def nearests_zones(geodata, upper_bound=5000, max_nearests=1200, remove_self=True):
    #https://www.eye4software.com/hydromagic/documentation/supported-map-grids/Argentina
    geodata = geodata.to_crs(epsg=5349)
    centroids = np.array(list(zip(geodata.geometry.centroid.x, geodata.geometry.centroid.y)) )
    btree = cKDTree(centroids)
    dist, idx = btree.query(centroids, k=max_nearests, distance_upper_bound=upper_bound)
    idxs = np.stack([dist, idx], axis=2)
    if remove_self:
        condition = lambda d: d>1e-9 and d<max_nearests
    else:
        condition = lambda d: d<max_nearests
    nearests = [[int(id) for d,id in plist if condition(d)] for plist in idxs]
    return nearests

def load_population_census(location_file = PXLOC, census_file = CENSO_HDF, schooldb_file = SCHOOL_HDF):
    geodata = gpd.read_file(location_file, encoding='utf-8')
    #geodata['departamen'] = [normalize_dpto_name(n) for n in geodata['departamen']]
    geodata['area'] = geodata['area'].astype(float)
    geodata['poblacion'] = geodata['poblacion'].astype(float)
    geodata['hogares'] = [int(re.sub(r'(\d+).0+', r'\1', x)) if x else 0 for x in geodata['hogares']]
    geodata['dpto_id'] = geodata['dpto_id'].astype(int)
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

    schooldb = pd.read_hdf(schooldb_file, 'matricula_y_secciones')
    schooldb = schooldb.replace(to_replace="Ciudad de Buenos Aires", value="Ciudad Autónoma de Buenos Aires")
    count_cols = list(filter(lambda s: s.startswith('Alumnos con Sobreedad') or s.startswith('Repitentes') or s.startswith('Matrícula.'), schooldb.columns))
    schooldb['total_alumnos'] = schooldb.loc[:,count_cols].sum(axis=1)
    schooldb = schooldb[['Provincia', 'Ámbito', 'total_alumnos']].groupby(['Provincia', 'Ámbito']).mean().reset_index()
    schooldb = schooldb.pivot_table('total_alumnos', ['Provincia'], 'Ámbito').reset_index()
    PXLOCDPTO = os.path.join(DATA_DIR, 'indec', 'pxdptodatosok.shp')
    provdata = gpd.read_file(PXLOCDPTO, encoding='utf-8')
    provdata['link'] = [int(n) for n in provdata['link']]
    schooldb = pd.merge(provdata[['link', 'provincia']], schooldb, left_on='provincia', right_on='Provincia')
    schooldb.rename(columns={'link':'area', 'Rural': 'Alumnos rural', 'Urbano': 'Alumnos urbano'}, inplace=True)
    schooldb = schooldb[['area', 'Alumnos rural', 'Alumnos urbano']]
    tables.append(schooldb)

    return geodata, functools.reduce(lambda a,b: pd.merge(a, b, how='inner', on='area'), tables)

def generate(genpop_dataset = None, prov_id = None, frac=1.):
    if genpop_dataset is None:
        genpop_dataset = load_population_census()
    geodata, census = genpop_dataset
    population = Population()
    tamanios_familia = ['1', '2', '3', '4', '5', '6', '7', '8 o más']
    parentescos = ['Cónyuge o pareja', 'Hijo(a) / Hijastro(a)', 'Jefe(a)', 'Nieto(a)', 'Otros familiares', 'Otros no familiares', 'Padre / Madre / Suegro(a)', 'Servicio doméstico y sus familiares', 'Yerno / Nuera']
    edad = list(map(str, range(111)))
    sexo = ['Mujer', 'Varón']
    escuela = ['Asiste', 'Asistió', 'Nunca asistió']
    nivel_educactivo = ['Asiste', 'Asistió', 'Nunca asistió']
    trabaja = ['Desocupado', 'Inactivo', 'Ocupado']
    urbano_rural = ['Rural agrupado', 'Rural disperso', 'Urbano']
    tamanio_escuela = ['Alumnos rural', 'Alumnos urbano']
    tamanio_cross_parentescos = cross_cols(tamanios_familia, parentescos)
    parentescos_cross_edad = cross_cols(parentescos, edad)
    parentescos_cross_sexo = cross_cols(parentescos, sexo)
    edad_cross_escuela = cross_cols(filter(lambda e: int(e)>=3, edad), escuela)
    edad_cross_trabaja = cross_cols(filter(lambda e: int(e)>=14, edad), trabaja)
    urbano_rurales = {}
    tamanios_escuelas = {}
    tamanios = {}
    parentescos = {}
    edades = {}
    sexos = {}
    escuelas = {}
    trabajos = {}
    if prov_id != None:
        geodata = geodata[geodata['prov_id'].astype(int)==prov_id]
    geodata = geodata.sample(frac=frac)
    geodata.sort_values(['prov_id', 'dpto_id'], inplace=True)
    population.geodata = geodata

    print("Calculating nearests zones...")
    population.nearest_zones = nearests_zones(geodata, 1000, 1200, remove_self=False)
    areas = np.array(geodata['area'].astype(float))
    poblaciones = np.array(geodata['poblacion'].astype(float))
    population.nearest_densities = [sum(poblaciones[z] for z in zones)/sum(areas[z] for z in zones) for zones in population.nearest_zones]
    school_zones = nearests_zones(geodata, 5000, 1200, remove_self=False)


    print("Generating distributions by deparment...")
    for index, row in tqdm(census.iterrows(), total=len(census)):
        tamanios_escuelas[row['area']] = {c:row[c] for c in tamanio_escuela}
        tamanios[row['area']] = GenWithDistribution(tamanios_familia, row)
        urbano_rurales[row['area']] = GenWithDistribution(urbano_rural, row)
        parentescos[row['area']] = {k: GenWithDistribution(v, row) for k, v in tamanio_cross_parentescos.items()}
        edades[row['area']] = {k: GenWithDistribution(v, row) for k, v in parentescos_cross_edad.items()}
        sexos[row['area']] = {k: GenWithDistribution(v, row) for k, v in parentescos_cross_sexo.items()}
        escuelas[row['area']] = {k: GenWithDistribution(v, row) for k, v in edad_cross_escuela.items()}
        trabajos[row['area']] = {k: GenWithDistribution(v, row) for k, v in edad_cross_trabaja.items()}

    print("Generating population...")
    school_gen = SchoolIdGenerator()
    school_gen_urb = {}
    school_gen_rural = {}
    for zone_id,(_i, row) in enumerate(geodata.iterrows()):
        school_gen_urb[zone_id] = AlumnSchoolIdGenerator(school_gen, tamanios_escuelas[row['dpto_id']]['Alumnos urbano'])
        school_gen_rural[zone_id] = AlumnSchoolIdGenerator(school_gen, tamanios_escuelas[row['dpto_id']]['Alumnos rural'])

    es_flia_urbana = []
    expected_people = geodata['poblacion'].astype(float).astype(int).sum()
    with tqdm(total=expected_people, unit="people") as progress:
        for zone_id, (_i, row) in enumerate(geodata.iterrows()):
            rural_urbano_flias = urbano_rurales[row['dpto_id']].get(k = int(row['hogares']))
            tamanios_flias = tamanios[row['dpto_id']].get(k = int(row['hogares']))
            parentescos_d = parentescos[row['dpto_id']]
            edades_d = edades[row['dpto_id']]
            sexos_d = sexos[row['dpto_id']]
            escuelas_d = escuelas[row['dpto_id']]
            trabajos_d = trabajos[row['dpto_id']]
            by_parentesco = defaultdict(list)
            for tam_flia, urbano in zip(tamanios_flias, rural_urbano_flias):
                urbano = urbano=='Urbano'
                tam_flia_num = int(tam_flia.replace('8 o más', str(random.choices(range(8, 16))[0])))
                parentescos_flia = parentescos_d[tam_flia].get(k=tam_flia_num)
                family_id = len(population.families)
                es_flia_urbana.append(urbano)
                population.families.append(Family(family_id, zone_id, population.get_dpto_id(row['dpto_id']), population.get_prov_id(row['prov_id'])))
                for member in parentescos_flia:
                    id = len(population.people)
                    by_parentesco[member].append(id)
                    population.people.append(Person(id, family_id, None, None, 0, False))
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
                        es_urbana = es_flia_urbana[population.people[p].family]
                        if estudiav == 'Asiste':
                            population.people[p].escuela = school_gen_urb[random.choice(school_zones[zone_id])].get_school() if es_urbana else school_gen_rural[random.choice(school_zones[zone_id])].get_school()
                        else:
                            population.people[p].escuela = 0
                if int(edadv)>=14:
                    trabajan = trabajos_d[edadv].get(len(people))
                    for p,trabajav in zip(people, trabajan):
                        if trabajav == 'Ocupado':
                            population.people[p].trabajo = 1 #TODO: add work enviroments
                        else:
                            population.people[p].trabajo = 0

        population.calculate_ids()

        geo_bydpto = geodata.groupby('dpto_id')[['poblacion', 'area']].sum().reset_index().set_index('dpto_id')
        geo_bydpto['density'] = geo_bydpto['poblacion']/geo_bydpto['area']
        population.department_densities = [geo_bydpto['density'][dpto] for dpto in population.departments]
        geo_byprov = geodata.groupby('prov_id')[['poblacion', 'area']].sum().reset_index().set_index('prov_id')
        geo_byprov['density'] = geo_byprov['poblacion']/geo_byprov['area']
        population.province_densities = [geo_byprov['density'][prov] for prov in population.provinces]
        return population


def main():
    generate(prov_id=66, frac=1.0).to_dat(os.path.join(DATA_DIR, 'fake_population_small.dat'), os.path.join(DATA_DIR, 'fake_population_small.json'), os.path.join(DATA_DIR, 'fake_population_small.gpkg'))

if __name__ == "__main__":
    main()
