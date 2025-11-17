#ifndef RECIPEEXECUTOR_H
#define RECIPEEXECUTOR_H

#include <QObject>
#include <QQueue>

class RecipeExecutor : public QObject
{
    Q_OBJECT
public:
    explicit RecipeExecutor(QObject *parent = nullptr);

signals:
};

#endif // RECIPEEXECUTOR_H
