#stardis-app : Prototype d'application Stardis

stardis-app est un prototype d'applicatif mettant en oeuvre la bibliothèque
stardis-solver (https://gitlab.com/meso-star/stardis-solver) avec comme objectif
de faire la jonction entre un outil de CAO telle que Salome et le solver.

Pour compiler il suffit de taper `make` après avoir préalablement renseigner
dans le makefile la variable SDIS_SDK qui est le chemin du SDK de Stardis. Voir
les notes de version en ci-dessous pour avoir la bonne version du SDK.


# Note de versions

## v0.4

- Conformité C99.
- Build Windows.
- Passage à stardis-solver 0.5 ; l'intégration temporelle n'est toutefois pas
  encore accessible.
- Passage à cmake

## v0.3.2

- Ajout de la fonctionnalité solve_probe_boundary. La fonction solve est
  automatiquement selectionnée en fonction de la distance à la primitive la plus
  proche. Si la distance est inférieure à 2.1 delta, la fonction
  solve_probe_boundary sera utilisée.
- Ajout des conditions limites en flux.

## v0.3.1

Ajout du tranfert radiatif. Ce qui ajoute deux nouveaux parametrès par medium : 

- emissivity;
- specular_fraction.

## v0.3

Passage à la version 0.3.0 du  SDK de stardis-solver. La version modifié du SDK
est disponible sur le NAS
(meso-star/repo/Stardis-0.1.0-GNU-Linux64_modified.tar.gz). Trois fonctionnalités
ont été ajoutés :

 - la puissance volumique;
 - la possibilité de renseigner l'argument fp_to_meter de la fonction solve de
   stardis-solver;
 - une option pour dumper la géométrie après analyse au format VTK avec des tags
   medium front/back et id des conditions limites.


## v0.1

Evalue une température sonde uniquement, pour un système purement conductif.Les
conditions aux limites peuvent être de type dirichlet ou de type échange. Cette
version repose sur le SDK de Stardis modifié 0.1.0 disponible sur la NAS
(meso-star/repo/Stardis-0.1.0-GNU-Linux64_modified.tar.gz). La modification
consiste en l'ajout de la bibliothèqe sstl et tinyexpr
(https://github.com/codeplea/tinyexpr (https://github.com/codeplea/tinyexpr)
