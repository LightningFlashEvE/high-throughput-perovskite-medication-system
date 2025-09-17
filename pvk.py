import re
from collections import defaultdict
from typing import Dict, Tuple

# -------- atomic weights --------
ATOMIC_WEIGHTS = {
    'H': 1.008, 'C': 12.01, 'N': 14.01, 'O': 16.00, 'F': 19.00,
    'I': 126.90, 'Br': 79.90, 'Cl': 35.45, 'Cs': 132.91, 'Pb': 207.2,
    'FA': 45.08,   # CH(NH2)2+
    'MA': 32.07,   # CH3NH3+
}

def mw(formula: str) -> float:
    m = 0.0
    for tok, num in re.findall(r'(FA|MA|Cs|Pb|I|Br|Cl)(\d*)', formula):
        n = int(num) if num else 1
        m += ATOMIC_WEIGHTS[tok] * n
    return m

# -------- allowed precursors only --------
PRECURSOR_WEIGHTS = {
    'FAI':  mw('FAI'),
    'MAI':  mw('MAI'),
    'MABr': mw('MABr'),
    'MACl': mw('MACl'),
    'CsI':  mw('CsI'),
    'PbI2': mw('PbI2'),
    'PbBr2':mw('PbBr2'),
    'PbCl2':mw('PbCl2'),
}

# -------- formula parsing --------
def parse_formula(formula: str) -> Dict[str, float]:
    def parse_group(s: str, mult: float = 1.0) -> Dict[str, float]:
        out = defaultdict(float)
        pattern = r'\(([^\)]+)\)([0-9.]+)?|(FA|MA|[A-Z][a-z]?)([0-9.]+)?'
        for m in re.finditer(pattern, s):
            if m.group(1):  # (...)k
                sub = m.group(1)
                k = float(m.group(2)) if m.group(2) else 1.0
                sub_e = parse_group(sub, mult * k)
                for k2, v2 in sub_e.items(): out[k2] += v2
            elif m.group(3):  # token n
                tok = m.group(3)
                n = float(m.group(4)) if m.group(4) else 1.0
                out[tok] += n * mult
        return out
    return parse_group(formula, 1.0)

def molecular_weight(elements: Dict[str, float]) -> float:
    return sum(ATOMIC_WEIGHTS[e]*c for e,c in elements.items() if e in ATOMIC_WEIGHTS)

# -------- core allocation with restrictions --------
def calculate_precursors(formula: str, molarity_M: float, volume_L: float):
    elems = parse_formula(formula)
    mw_fu = molecular_weight(elems)
    fu_mol = molarity_M * volume_L

    n_FA = elems.get('FA', 0.0) * fu_mol
    n_MA = elems.get('MA', 0.0) * fu_mol
    n_Cs = elems.get('Cs', 0.0) * fu_mol
    n_Pb = elems.get('Pb', 0.0) * fu_mol

    hal_need = {
        'I' : elems.get('I', 0.0)  * fu_mol,
        'Br': elems.get('Br', 0.0) * fu_mol,
        'Cl': elems.get('Cl', 0.0) * fu_mol,
    }

    # Reserve iodine for FA and Cs (required by constraint set)
    required_I = n_FA + n_Cs
    if hal_need['I'] + 1e-12 < required_I:
        raise ValueError(f"Iodine in formula insufficient for FA ({n_FA:.6g}) + Cs ({n_Cs:.6g}). "
                         f"Available I: {hal_need['I']:.6g}")

    precs: Dict[str, Tuple[float, float]] = {}

    # Allocate FA -> FAI
    if n_FA > 0:
        precs['FAI'] = (n_FA, n_FA * PRECURSOR_WEIGHTS['FAI'])
        hal_need['I'] -= n_FA

    # Allocate Cs -> CsI
    if n_Cs > 0:
        precs['CsI'] = (n_Cs, n_Cs * PRECURSOR_WEIGHTS['CsI'])
        hal_need['I'] -= n_Cs

    # Allocate MA over remaining I/Br/Cl
    if n_MA > 0:
        total_hal_for_MA = hal_need['I'] + hal_need['Br'] + hal_need['Cl']
        if total_hal_for_MA + 1e-12 < n_MA:
            raise ValueError(f"Halides insufficient to bind MA. Need {n_MA:.6g}, have {total_hal_for_MA:.6g}.")
        # proportional split, then cap by availability; fix remainder on the last bucket
        parts = [('MAI','I'), ('MABr','Br'), ('MACl','Cl')]
        remaining = n_MA
        total = total_hal_for_MA if total_hal_for_MA>0 else 1.0
        for i,(salt,h) in enumerate(parts):
            share = n_MA * (hal_need[h]/total) if total>0 else 0.0
            take = min(share, hal_need[h], remaining) if i < len(parts)-1 else remaining
            take = max(0.0, take)
            if take > 0:
                mol_prev = precs.get(salt, (0.0,0.0))[0]
                precs[salt] = (mol_prev + take, 0.0)
                hal_need[h] -= take
                remaining -= take
        if remaining > 1e-8:
            # Should not happen, but guard
            raise RuntimeError(f"Failed to allocate all MA; left {remaining:.4g} mol.")

    # Remaining halides go to PbX2
    m_PbI2  = hal_need['I']  / 2.0
    m_PbBr2 = hal_need['Br'] / 2.0
    m_PbCl2 = hal_need['Cl'] / 2.0
    for salt, mol in (('PbI2',m_PbI2), ('PbBr2',m_PbBr2), ('PbCl2',m_PbCl2)):
        if mol > 1e-12:
            precs[salt] = (precs.get(salt,(0.0,0.0))[0] + mol, 0.0)

    # Check lead balance
    pb_from_lead_salts = m_PbI2 + m_PbBr2 + m_PbCl2
    if abs(pb_from_lead_salts - n_Pb) > 1e-6:
        raise ValueError(f"Lead mismatch: Pb needed {n_Pb:.6g}, from PbX2 {pb_from_lead_salts:.6g}. "
                         f"Check A-site sum vs Pb in formula.")

    # Fill grams
    for salt,(mol,_) in list(precs.items()):
        precs[salt] = (mol, mol * PRECURSOR_WEIGHTS[salt])

    checks = {
        'fu_mol': fu_mol,
        'mw_per_fu_g': mw_fu,
        'A_FA_MA_Cs_mol': n_FA + n_MA + n_Cs,
        'Pb_mol': n_Pb,
        'Pb_from_PbX2_mol': pb_from_lead_salts,
    }
    return precs, checks, elems

if __name__ == "__main__":
    formula = "Cs0.17FA0.83Pb(I0.8Br0.2)3"
    M = 1.0
    V = 1.0
    precs, checks, elems = calculate_precursors(formula, M, V)
    for k,(mol,g) in sorted(precs.items()):
        print(f"{k:6s} : {mol:.6f} mol, {g:.4f} g")