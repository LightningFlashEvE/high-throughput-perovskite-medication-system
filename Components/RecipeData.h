#ifndef RECIPEDATA_H
#define RECIPEDATA_H

#include <QString>
#include <QMap>

class RecipeData {
public:
    QString formula;
    QMap<QString, QString> solvent;
    QMap<QString, QString> precursor;
};

#endif // RECIPEDATA_H
