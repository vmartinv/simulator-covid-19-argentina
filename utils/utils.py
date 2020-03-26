import re
import pandas as pd

def fix_encoding(text):
    fixes = [
        ('·', 'á'),
        ('È', 'é'),
        ('Ì', 'í'),
        ('Û', 'ó'),
        ('˙', 'ú'),
        ('Ò', 'ñ'),
        ('¸', 'ü'),
        ('∞', 'º'),
        ('—', 'Ñ'),
    ]
    fixed = text.strip()
    for orig, repl in fixes:
        fixed = fixed.replace(orig, repl)
    for c in list(fixed):
        if c not in [r for o,r in fixes]+list(" .'-()/") and not c.isalnum():
            print(f"Unrecognized character: `{c}` in {fixed}")
            exit(1)
    return fixed

def normalize_dpto_name(name):
    name = re.sub(r"^Comuna (\d)$", r"Comuna 0\1", name)
    name = name.replace('Adolfo Gonzales Chaves', 'Adolfo Gonzáles Chaves')
    name = name.replace('Atreuco', 'Atreucó')
    name = name.replace('Catan Lil', 'Catán Lil')
    name = name.replace('Chical Co', 'Chical Có')
    name = name.replace('Atreuco', 'Atreucó')
    name = name.replace('Coronel de Marina L. Rosales', 'Coronel de Marina Leonardo Rosales')
    name = name.replace('Eldorado', 'El Dorado')
    name = name.replace('Famallá', 'Famaillá')
    name = name.replace('General Juan F.Quiroga', 'General Juan F. Quiroga')
    name = name.replace('Islas del Atlántico sur', 'Islas del Atlántico Sur')
    name = name.replace('Lacar', 'Lácar')
    name = name.replace('Martires', 'Mártires')
    name = name.replace('Nogoya', 'Nogoyá')
    name = name.replace('Ojo de agua', 'Ojo de Agua')
    name = name.replace('Paclin', 'Paclín')
    name = name.replace('Paso de Indios', 'Paso de los Indios')
    name = name.replace('Pichi Mahuida', 'Pichi Mahuída')
    name = name.replace('Pilnaniyeu', 'Pilcaniyeu')
    name = name.replace('Pirane', 'Pirané')
    name = name.replace('Poman', 'Pomán')
    name = name.replace('San Cristobal', 'San Cristóbal')
    name = name.replace('Yavi', 'Yaví')
    # https://es.wikipedia.org/wiki/Arrecifes_(Argentina)
    name = name.replace('Bartolomé Mitre', 'Arrecifes')
    name = name.replace('Pelegrini', 'Pellegrini')
    return re.sub(r' +', ' ', name)

def validate_dpto_indexes(col1, col2):
    col1 = set(col1)
    col2 = set(col2)
    diff = sorted(col1^col2)
    if diff:
        print("Error validate_dpto_indexes!")
        diff_table = {'id': [], 'en_col1': [], 'en_col2': [],}
        for d in diff:
            diff_table['id'].append(d)
            diff_table['en_col1'].append(d in col1)
            diff_table['en_col2'].append(d in col2)
        print(pd.DataFrame(data=diff_table))
        exit(1)
    else:
        print("ok!")
