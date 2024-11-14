#include <cstdio>
#include <iostream>
#include <algorithm>
#include <opencv2/core/utility.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <map>

using namespace cv;
using namespace std;

struct ColorDistribution
{
  float data[8][8][8]; // l'histogramme
  int nb;              // le nombre d'échantillons

  ColorDistribution() { reset(); }

  ColorDistribution &operator=(const ColorDistribution &other) = default;

  // Met à zéro l'histogramme
  void reset()
  {
    memset(data, 0, sizeof(data));
    nb = 0;
  }

  // Ajoute l'échantillon color à l'histogramme:
  // met +1 dans la bonne case de l'histogramme et augmente le nb d'échantillons
  void add(Vec3b color)
  {
    data[color[0] >> 5][color[1] >> 5][color[2] >> 5]++;
    nb++;
  }

  // Indique qu'on a fini de mettre les échantillons:
  // divise chaque valeur du tableau par le nombre d'échantillons
  // pour que case représente la proportion des picels qui ont cette couleur.
  void finished()
  {
    for (int i = 0; i < 8; i++)
      for (int j = 0; j < 8; j++)
        for (int k = 0; k < 8; k++)
          data[i][j][k] /= nb;
  }

  // Retourne la distance entre l'histogramme courant et un autre
  float distance(const ColorDistribution &other) const
  {
    float dist = 0;
    for (int i = 0; i < 8; i++)
      for (int j = 0; j < 8; j++)
        for (int k = 0; k < 8; k++)
          if (data[i][j][k] + other.data[i][j][k] > 0)
            dist += (pow((data[i][j][k] - other.data[i][j][k]), 2)) / (data[i][j][k] + other.data[i][j][k]);
    return dist;
  }
};

ColorDistribution
getColorDistribution(Mat input, Point pt1, Point pt2)
{
  ColorDistribution cd;
  for (int y = pt1.y; y < pt2.y; y++)
    for (int x = pt1.x; x < pt2.x; x++)
      cd.add(input.at<Vec3b>(y, x));
  cd.finished();
  return cd;
}

// Retourne la plus petite distance entre h et les histogrammes de couleurs "hists"
float minDistance(const ColorDistribution &h,
                  const std::vector<ColorDistribution> &hists)
{
  float minDist = std::numeric_limits<float>::max();
  for (const ColorDistribution &h2 : hists)
  {
    float dist = h.distance(h2);
    if (dist < minDist)
      minDist = dist;
  }
  return minDist;
}

// Fabrique une nouvelle image, où chaque bloc est coloré selon qu'il est "fond" ou "objet".
Mat recoObject(Mat input,
               const std::vector<std::vector<ColorDistribution>> &all_col_hists,
               const std::vector<Vec3b> &colors,
               const int bloc)
{
  Mat output = input.clone();
  for (int y = 0; y <= input.rows - bloc; y += bloc)
  {
    for (int x = 0; x <= input.cols - bloc; x += bloc)
    {
      // Histogramme de la couleur du bloc actuel
      ColorDistribution cd = getColorDistribution(input, Point(x, y), Point(x + bloc, y + bloc));

      // On determine la distance minimale parmi toutes les catégories (fond + objets)
      float minDist = std::numeric_limits<float>::max();
      int closestCategory = 0; // On défini 0 pour le fond, 1 pour le premier objet...

      for (int i = 0; i < all_col_hists.size(); ++i)
      {
        float dist = minDistance(cd, all_col_hists[i]);
        if (dist < minDist)
        {
          minDist = dist;
          closestCategory = i;
        }
      }

      // On colore le bloc courant selon la catégorie la plus proche
      Vec3b color = colors[closestCategory];

      for (int i = 0; i < bloc; ++i)
      {
        for (int j = 0; j < bloc; ++j)
        {
          output.at<Vec3b>(y + i, x + j) = color;
        }
      }
    }
  }
  return output;
}

Mat relaxLabels(const Mat &labels, int bloc)
{
  // On créer une copie de l'image
  Mat relaxedLabels = labels.clone();

  // On parcourt l'image
  for (int y = 1; y < labels.rows - 1; ++y)
  {
    for (int x = 1; x < labels.cols - 1; ++x)
    {
      std::map<int, int> labelCounts;
      for (int dy = -1; dy <= 1; ++dy)
      {
        for (int dx = -1; dx <= 1; ++dx)
        {
          if (dx == 0 && dy == 0)
            continue;
          int neighborLabel = labels.at<Vec3b>(y + dy, x + dx)[0]; // On récupère le label du voisin
          labelCounts[neighborLabel]++;
        }
      }
      // On détermine le label le plus fréquent
      int currentLabel = labels.at<Vec3b>(y, x)[0]; // Label actuel du bloc central
      int mostFrequentLabel = currentLabel;
      int maxCount = 0;
      for (const auto &labelCount : labelCounts)
      {
        if (labelCount.second > maxCount)
        {
          mostFrequentLabel = labelCount.first;
          maxCount = labelCount.second;
        }
      }
      // On applique le label le plus fréquent si différent du label actuel
      if (mostFrequentLabel != currentLabel && maxCount > 4)
      {
        relaxedLabels.at<Vec3b>(y, x)[0] = mostFrequentLabel;
      }
    }
  }
  return relaxedLabels;
}

std::vector<ColorDistribution> col_hists;                  // histogrammes du fond
std::vector<ColorDistribution> col_hists_object;           // histogrammes de l'objet
std::vector<std::vector<ColorDistribution>> all_col_hists; // tableau de tableau d'histogrammes
float distanceThreshold = 0.05f;

void onTrackbarChange(int value, void *)
{
  distanceThreshold = value / 100.0f;
}

int main(int argc, char **argv)
{
  Mat img_input, img_seg, img_d_bgr, img_d_hsv, img_d_lab;
  VideoCapture *pCap = nullptr;
  const int width = 640;
  const int height = 480;
  const int size = 50;
  const int bbloc = 128;
  // Ouvre la camera
  pCap = new VideoCapture(0);
  if (!pCap->isOpened())
  {
    cout << "Couldn't open image / camera ";
    return 1;
  }
  // Force une camera 640x480 (pas trop grande).
  pCap->set(CAP_PROP_FRAME_WIDTH, 640);
  pCap->set(CAP_PROP_FRAME_HEIGHT, 480);
  (*pCap) >> img_input;
  if (img_input.empty())
    return 1; // probleme avec la camera
  Point pt1(width / 2 - size / 2, height / 2 - size / 2);
  Point pt2(width / 2 + size / 2, height / 2 + size / 2);
  namedWindow("input", 1);
  createTrackbar("Seuil", "input", nullptr, 100, onTrackbarChange);
  imshow("input", img_input);
  bool freeze = false;
  bool reco = false;
  while (true)
  {
    char c = (char)waitKey(50); // attend 50ms -> 20 images/s
    if (pCap != nullptr && !freeze)
      (*pCap) >> img_input;  // récupère l'image de la caméra
    if (c == 27 || c == 'q') // permet de quitter l'application
      break;
    if (c == 'f') // permet de geler l'image
      freeze = !freeze;
    if (c == 'r' && !col_hists.empty() && !col_hists_object.empty()) // permet de lancer la reconnaissance
      reco = !reco;
    if (c == 'v')
    {
      // Partie gauche
      ColorDistribution cd_left = getColorDistribution(img_input, Point(0, 0), Point(width / 2, height));
      // Partie droite
      ColorDistribution cd_right = getColorDistribution(img_input, Point(width / 2, 0), Point(width, height));

      // Calcul de la distance
      float dist = cd_left.distance(cd_right);
      cout << "Distance chi-2 entre les deux distributions de couleur: " << dist << endl;
    }
    if (c == 'b') // Touche pour ajouter un fond
    {
      col_hists.clear(); // vide l'histogramme précédent
      for (int y = 0; y <= height - bbloc; y += bbloc)
      {
        for (int x = 0; x <= width - bbloc; x += bbloc)
        {
          ColorDistribution cd = getColorDistribution(img_input, Point(x, y), Point(x + bbloc, y + bbloc));
          col_hists.push_back(cd);
        }
      }
      if (all_col_hists.empty())
      {
        all_col_hists.push_back(col_hists);
        cout << "Fond ajouté." << endl;
      }
      else
      {
        all_col_hists[0] = col_hists;
        cout << "Fond remplacé." << endl;
      }

      cout << "Histogrammes du fond calculés" << endl;
    }
    if (c == 'a' && !col_hists.empty()) // Touche pour ajouter un objet
    {
      bool add = true;
      ColorDistribution cd = getColorDistribution(img_input, pt1, pt2);
      for (const auto &histList : all_col_hists)
      {
        for (const auto &h : histList)
        {
          if (cd.distance(h) < distanceThreshold)
          {
            cout << "L'histogramme est trop proche d'un objet existant ." << endl;
            add = false;
          }
        }
      }
      if (add == true)
      {
        col_hists_object.clear(); // On vide l'histogramme précédent
        col_hists_object.push_back(cd);
        all_col_hists.push_back(col_hists_object);
        cout << "Nouvel objet ajouté. Nombre total d'objets : " << all_col_hists.size() << endl;
      }
    }
    Mat output = img_input;
    if (reco)
    {
      const std::vector<Vec3b> colors = {Vec3b(0, 0, 0), Vec3b(0, 0, 255), Vec3b(0, 255, 0), Vec3b(255, 0, 0), Vec3b(0, 255, 255)};

      Mat gray;
      cvtColor(img_input, gray, COLOR_BGR2GRAY);
      Mat reco = recoObject(img_input, all_col_hists, colors, 16);

      // On applique la relaxation des labels
      Mat relaxedReco = relaxLabels(reco, 16);

      cvtColor(gray, img_input, COLOR_GRAY2BGR);
      output = 0.5 * relaxedReco + 0.5 * img_input; // mélange reco + caméra
    }
    else
    {
      cv::rectangle(img_input, pt1, pt2, Scalar({255.0, 255.0, 255.0}), 1);
    }
    imshow("input", output); // affiche le flux video
  }
  return 0;
}