import matplotlib.pyplot as plt
import numpy as np
import pandas as pd


def load_and_clean_bench(file_path):
    """Charge un fichier de benchmark, corrige les en-têtes manquants,

    supprime les espaces et force le typage numérique.
    """
    df = pd.read_csv(file_path)

    # 1. Nettoyage des espaces dans les noms de colonnes
    df.columns = df.columns.str.strip()

    # 2. Si l'en-tête 'pid' est manquant, la colonne est en Index. On la récupère.
    if 'pid' not in df.columns:
        df = df.reset_index()
        df.rename(columns={df.columns[0]: 'pid'}, inplace=True)

    # 3. Nettoyage des espaces résiduels dans les données et conversion en float
    for col in ['avg_delay_ms', 'max_delay_ms']:
        if col in df.columns:
            if df[col].dtype == 'object':
                df[col] = df[col].str.strip()
            df[col] = pd.to_numeric(df[col], errors='coerce')

    # 4. Tri par PID pour conserver la chronologie de création des processus
    df = df.sort_values(by='pid').reset_index(drop=True)
    return df


# ==========================================
# 1. CHARGEMENT DES FICHIERS
# ==========================================
file_1 = './bench_ksched_16coeurs.csv'
file_2 = './bench_kdyna_16coeurs.csv'

df_1 = load_and_clean_bench(file_1)
df_2 = load_and_clean_bench(file_2)

# Détermination du nombre maximal de processus à afficher
num_processes = max(len(df_1), len(df_2))
x = np.arange(num_processes)
width = 0.35  # Largeur des barres

# ==========================================
# 2. INITIALISATION DE LA FIGURE (1x2)
# ==========================================
fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 6))

# ------------------------------------------
# GRAPH_1 : Average Delay (Gauche)
# ------------------------------------------
ax1.bar(
    x[: len(df_1)] - width / 2,
    df_1['avg_delay_ms'],
    width,
    label='scx_ksched',
    color='tab:red',
    alpha=0.9,
)
ax1.bar(
    x[: len(df_2)] + width / 2,
    df_2['avg_delay_ms'],
    width,
    label='scx_kdyna',
    color='tab:orange',
    alpha=0.9,
)

ax1.set_xlabel('Processus (triés par numéro de pid)', fontsize=11)
ax1.set_ylabel('Average Delay (ms)', fontsize=11)
ax1.set_title(
    "Comparaison de l'Average Delay", fontsize=13, fontweight='bold', pad=12
)
ax1.set_xticks(x)
ax1.set_xticklabels([f'P{i+1}' for i in x], rotation=45)
ax1.grid(True, which='both', linestyle='--', alpha=0.4)
ax1.legend()

# ------------------------------------------
# GRAPH_2 : Maximum Delay (Droite)
# ------------------------------------------
ax2.bar(
    x[: len(df_1)] - width / 2,
    df_1['max_delay_ms'],
    width,
    label='scx_ksched',
    color='tab:red',
    alpha=0.9,
)
ax2.bar(
    x[: len(df_2)] + width / 2,
    df_2['max_delay_ms'],
    width,
    label='scx_kdyna',
    color='tab:orange',
    alpha=0.9,
)

ax2.set_xlabel('Processus (triés par numéro de pid)', fontsize=11)
ax2.set_ylabel('Maximum Delay (ms)', fontsize=11)
ax2.set_title(
    'Comparaison du Maximum Delay', fontsize=13, fontweight='bold', pad=12
)
ax2.set_xticks(x)
ax2.set_xticklabels([f'P{i+1}' for i in x], rotation=45)
ax2.grid(True, which='both', linestyle='--', alpha=0.4)
ax2.legend()

# ==========================================
# 3. MISE EN PAGE ET AFFICHAGE
# ==========================================
plt.suptitle(
    'Scheduler user vs Scheduler kernel 16 cœurs',
    fontsize=16,
    fontweight='bold',
    y=1.02,
)
plt.tight_layout()

# plt.savefig('comparatif_generique.png', dpi=300, bbox_inches='tight') # Décommenter pour sauvegarder
plt.show()
