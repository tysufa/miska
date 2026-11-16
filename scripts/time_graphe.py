import matplotlib.pyplot as plt
import numpy as np

# 1. Préparation des données
# Remplacez ces listes par vos propres valeurs
programmes = ['scx_ksched 4 coeurs', 'scx_mine 4 coeurs', 'scx_kdyna 4 coeurs', 'scx_ksched 16 coeurs', 'scx_mine 16 coeurs', 'scx_kdyna 16 coeurs']
data = {
    'scx_ksched 4 coeurs': [13.46, 13.45, 13.6, 14, 13.57],
    'scx_mine 4 coeurs': [16.18, 16.77, 16.6, 16.69, 16.4],
    'scx_kdyna 4 coeurs': [13.2, 13.1, 13.06, 13.12, 13.2],
    'scx_ksched 16 coeurs': [7.1, 7.1, 7.17, 6.98, 7.18],
    'scx_mine 16 coeurs': [7.46, 7.4, 7.4, 7.5, 7.35],
    'scx_kdyna 16 coeurs': [5.66, 5.6, 5.78, 5.66, 5.69],
}

# 2. Calcul des moyennes et des écarts-types
moyennes = [np.mean(data[prog]) for prog in programmes]
ecarts_types = [np.std(data[prog]) for prog in programmes]

# 3. Création du graphique
plt.figure(figsize=(10, 6))

couleurs = ['red', 'blue', 'orange', 'red', 'blue', 'orange']

# Création des barres avec les barres d'erreur
plt.bar(programmes, moyennes, yerr=ecarts_types, capsize=10, 
        color=couleurs, edgecolor='black', alpha=0.7)

# Ajout des détails
plt.title('Comparaison du temps d\'exécution des schedulers', fontsize=14)
plt.ylabel('Temps (secondes)', fontsize=12)
plt.xlabel('schedulers', fontsize=12)
plt.grid(axis='y', linestyle='--')

# Affichage
plt.tight_layout()
plt.show()
