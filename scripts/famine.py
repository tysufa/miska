import matplotlib.pyplot as plt
import pandas as pd

# 1. Données de l'Exécution 1 : Mode Pure Min-PID (Ancien Log de Crash)
data_run1 = [
    (18.670528, 212), (18.693030, 211), (18.699635, 210), (18.773884, 213),
    (19.384640, 214), (29.409422, 215), (30.630143, 216), (30.670161, 217),
    (30.950095, 218), (33.409213, 219), (37.041447, 220), (41.603738, 221),
    (42.657274, 222), (43.433840, 223), (44.763362, 224), (46.966073, 225)
]

# Données de l'Exécution 1 : (Nouveau Log de Crash Réel - Watchdog)
data_crash_real = [
    (20.787495, 212), (20.923515, 213), (21.039671, 214), (21.231875, 215),
    (21.756627, 216), (31.342135, 217), (32.804733, 218), (32.941692, 219),
    (33.481206, 220), (35.364889, 221), (38.145146, 222), (43.784363, 223),
    (44.452407, 224), (45.209863, 225), (46.959721, 226), (49.705829, 227)
]

# 2. Données de l'Exécution 2 : Mode VTIME Dynamique (MISES À JOUR)
data_run2 = [
    (82.563736, 211), (82.873309, 212), (83.018433, 213), (83.034951, 214),
    (83.425084, 215), (83.563001, 216), (83.579687, 217), (83.689642, 218),
    (83.810539, 219), (83.925418, 220), (84.041756, 221), (84.141797, 222),
    (84.241904, 223), (84.337432, 224), (84.429152, 225), (84.509555, 226),
    (84.587132, 227), (84.652042, 228), (84.729755, 229), (84.790839, 230),
    (84.841265, 231), (84.929501, 232), (84.942987, 233), (84.978980, 234),
    (85.009978, 235), (85.060686, 236), (85.076467, 237), (85.102612, 238),
    (85.133730, 239), (85.156565, 240), (85.157390, 209)
]

# 3. Données de l'Exécution 3 : Mode Files Active/Expirée (Succès Espace Utilisateur)
data_run3 = [
    (95.175501, 211), (95.195637, 214), (95.358935, 213), (97.857260, 215),
    (98.037274, 216), (98.096552, 217), (99.515066, 218), (100.686436, 219),
    (100.809516, 220), (101.138190, 221), (102.266850, 222), (103.367623, 223),
    (103.713288, 224), (103.885031, 225), (105.070128, 226), (105.937640, 227),
    (106.591838, 229), (107.839092, 230), (108.658811, 231), (109.392572, 232),
    (110.571912, 234), (114.510954, 228), (114.602506, 212), (114.624170, 235),
    (114.639144, 233), (116.790672, 236), (117.610844, 237), (117.795050, 238),
    (119.313522, 239), (122.233872, 240), (122.234983, 208)
]

# Conversion en DataFrames Pandas et tri par Timestamp
df1 = pd.DataFrame(data_run1, columns=['Timestamp', 'PID']).sort_values('Timestamp')
df_crash = pd.DataFrame(data_crash_real, columns=['Timestamp', 'PID']).sort_values('Timestamp')
df2 = pd.DataFrame(data_run2, columns=['Timestamp', 'PID']).sort_values('Timestamp')
df3 = pd.DataFrame(data_run3, columns=['Timestamp', 'PID']).sort_values('Timestamp')

# Normalisation du temps : t=0 pour le premier processus terminé de chaque run
df1['Relative_Time'] = df1['Timestamp'] - df1['Timestamp'].min()
df_crash['Relative_Time'] = df_crash['Timestamp'] - df_crash['Timestamp'].min()
df2['Relative_Time'] = df2['Timestamp'] - df2['Timestamp'].min()
df3['Relative_Time'] = df3['Timestamp'] - df3['Timestamp'].min()

# Création du compteur cumulé de processus terminés (1, 2, 3...)
df1['Cumulative_Tasks'] = range(1, len(df1) + 1)
df_crash['Cumulative_Tasks'] = range(1, len(df_crash) + 1)
df2['Cumulative_Tasks'] = range(1, len(df2) + 1)
df3['Cumulative_Tasks'] = range(1, len(df3) + 1)

# --- Configuration du Graphique ---
plt.figure(figsize=(13, 8))

# Courbe 1 : scx_ksched (Ancien crash - Pointillé fin discret)
plt.step(df1['Relative_Time'], df1['Cumulative_Tasks'], 
         where='post', color='#FEB2B2', linewidth=1.5, linestyle=':',
         label='csx_kdyna avec alpha=0.000001')

# Courbe 1 (Mise à jour) : scx_ksched (Nouveau Crash Réel - Rouge Vif)
plt.step(df_crash['Relative_Time'], df_crash['Cumulative_Tasks'], 
         where='post', color='#E53E3E', linewidth=3, linestyle='--',
         label='scx_ksched')

# Courbe 2 : scx_kdyna (Priorité Dynamique Noyau - Orange)
plt.step(df2['Relative_Time'], df2['Cumulative_Tasks'], 
         where='post', color='#DD6B20', linewidth=2.5,
         label='scx_kdyna avec alpha=0.000035')

# Courbe 3 : scx_mine (Files Active/Expirée Utilisateur - Bleu)
plt.step(df3['Relative_Time'], df3['Cumulative_Tasks'], 
         where='post', color='purple', linewidth=2.5,
         label='scx_dyna (Active/Expirée - Utilisateur)')

# Ajout des points de repère sur les marches
plt.scatter(df_crash['Relative_Time'], df_crash['Cumulative_Tasks'], color='#E53E3E', zorder=3, s=50)
plt.scatter(df1['Relative_Time'], df1['Cumulative_Tasks'], color='#FEB2B2', zorder=3, s=20)
plt.scatter(df2['Relative_Time'], df2['Cumulative_Tasks'], color='#DD6B20', zorder=3, s=40)
plt.scatter(df3['Relative_Time'], df3['Cumulative_Tasks'], color='purple', zorder=3, s=40)

# Personnalisation des axes et titres
plt.title("Terminaison des processus Mandelbrot sous sched_ext", fontsize=14, fontweight='bold', pad=15)
plt.xlabel("Temps écoulé depuis la première fin de processus (en secondes)", fontsize=11)
plt.ylabel("Nombre total de processus terminés", fontsize=11)

# Ajustement dynamique des limites
max_time = max(df1['Relative_Time'].max(), df_crash['Relative_Time'].max(), df2['Relative_Time'].max(), df3['Relative_Time'].max())
max_tasks = max(len(df1), len(df_crash), len(df2), len(df3))
plt.xlim(-1, max_time + 2)
plt.ylim(0, max_tasks + 2)

# Grille de lecture et légende
plt.grid(True, linestyle=':', alpha=0.6)
plt.legend(fontsize=11, loc='lower right')

# Affichage du graphique
plt.tight_layout()
plt.show()
