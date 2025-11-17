#ifndef RECIPEQUEUE_H
#define RECIPEQUEUE_H

#include <QObject>
#include <QQueue>
#include <QPair>
#include <QThread>

class RecipeQueue : public QObject
{
    Q_OBJECT
public:
    explicit RecipeQueue(QObject *parent = nullptr);

public slots:
    void process(QString name, int ml)
    {
        // 耗时逻辑：用你的真实实现替换这里
        getSomething(name, ml);
        emit finished(name, ml);
    }

signals:
    void finished(QString name, int ml);

private:
    static void getSomething(const QString& /*name*/, int /*ml*/)
    {
        QThread::msleep(2000); // 示例：阻塞1.5s
    }

};

#endif // RECIPEQUEUE_H
