#stardis-app : Prototype d'application Stardis

stardis-app est un prototype d'applicatif mettant en oeuvre la bibliothèque
stardis-solver (https://gitlab.com/meso-star/stardis-solver) avec comme objectif
de faire la jonction entre un outil de CAO telle que Salome et le solver.

Pour compiler il suffit de taper `make` après avoir préalablement renseigner
dans le makefile la variable SDIS_SDK qui est le chemin du SDK de Stardis. Voir
les notes de version en ci-dessous pour avoir la bonne version du SDK.


# Note de versions

## v0.1

Evalue une température sonde uniquement, pour un système purement conductif.Les
conditions aux limites peuvent être de type dirichlet ou de type échange. Cette
version repose sur le SDK de Stardis modifié 0.1.0 disponible sur la NAS
(meso-star/repo/Stardis-0.1.0-GNU-Linux64_modified.tar.gz). La modification
consiste en l'ajout de la bibliothèqe sstl et tinyexpr
(https://github.com/codeplea/tinyexpr (https://github.com/codeplea/tinyexpr)



