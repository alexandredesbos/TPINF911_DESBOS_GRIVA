# TPINF911_DESBOS_GRIVA

Pour compiler:

    mkdir build
    cd build
    cmake -DCMAKE_BUILD_TYPE=Release ..
    make

Pour exécuter:

    ./main

## Compte rendu

Pour calculer la distance, on a utilisé x2Chi-2 

    for (int i = 0; i < 8; i++)
      for (int j = 0; j < 8; j++)
        for (int k = 0; k < 8; k++)
          if (data[i][j][k] + other.data[i][j][k] > 0)
            dist += (pow((data[i][j][k] - other.data[i][j][k]), 2)) / (data[i][j][k] + other.data[i][j][k]);

# Mode reconnaissance objet

Nous avons commencé par faire la reconnaissance d'un seul objet.
En chossisant un background (touche 'b'), puis un objet de couleur (touche 'a'), on peut le reconnaitre dans le flux video (touche 'r')

<table>
  <tr>
    <td>Reconnaissance d'un surligneur jaune</td>
     <td>Reconnaissance de la peau</td>
  </tr>
  <tr>
    <td><img src="images/surligneur.png" width=auto height=auto></td>
    <td><img src="images/peau.png" width=auto height=auto></td>
  </tr>
 </table>

# Mode reconnaissance de plusieurs objets

Pour reconnaitre plusieurs objets, on a ajouté un tableau d'histogrammes pour reconnaître plusieurs objets.

<table>
  <tr>
    <td>Image sans reconnaissance</td>
     <td>Reconnaissance activé</td>
  </tr>
  <tr>
    <td><img src="images/3colorsRaw.png" width=auto height=auto></td>
    <td><img src="images/3colors.png" width=auto height=auto></td>
  </tr>
 </table>

 ## Optimisation

 Afin d'otimiser le programme, on a ajouté un seuil pour la distance entre les histogrammes. Si la distance est inférieure à ce seuil, on considère que l'objet n'est pas utiles à ajouter. Pour cela on a ajouté une trackbar pour ajuster le seuil.

<table>
  <tr>
    <td>Trackbar pour ajuster le seuil</td>
     <td>Retour du terminal</td>
  </tr>
  <tr>
    <td><img src="images/Screenshot 2024-11-14 at 10.28.07 AM.png" width=auto height=auto></td>
    <td><img src="images/Screenshot 2024-11-14 at 10.28.21 AM.png" width=auto height=auto></td>
  </tr>
 </table>


