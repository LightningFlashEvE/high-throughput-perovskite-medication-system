#ifndef REAGENTBOTTLE_H
#define REAGENTBOTTLE_H

#include <QObject>
#include <QDate>

class ReagentBottle : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ getName WRITE setName NOTIFY nameChanged) 
    Q_PROPERTY(double  initial READ getInitial WRITE setInitial NOTIFY initialChanged) 
    Q_PROPERTY(double  remaining READ getRemaining WRITE setRemaining NOTIFY remainingChanged) 
    Q_PROPERTY(double  height READ getHeight WRITE setHeight NOTIFY heightChanged)
    Q_PROPERTY(double  x READ getX WRITE setX NOTIFY positionChanged)
    Q_PROPERTY(double  y READ getY WRITE setY NOTIFY positionChanged)
    Q_PROPERTY(QString lot READ getLot WRITE setLot NOTIFY lotChanged)
    Q_PROPERTY(QDate   expiry READ getExpiry WRITE setExpiry NOTIFY expiryChanged)
    Q_PROPERTY(QString barcode READ getBarcode WRITE setBarcode NOTIFY barcodeChanged)
    Q_PROPERTY(QString note READ getNote WRITE setNote NOTIFY noteChanged)

public:
    explicit ReagentBottle(QObject *parent = nullptr);

    // 读 带定义
    const QString& getName() const { return m_name; }
    double getInitial()     const { return m_initial; }
    double getRemaining()   const { return m_remaining; }
    double getHeight()      const { return m_height; }
    double getX()           const { return m_x; }
    double getY()           const { return m_y; }
    const QString& getLot() const { return m_lot; }
    QDate getExpiry()       const { return m_expiry; }
    const QString& getBarcode() const { return m_barcode; }
    const QString& getNote()    const { return m_note; }

public slots:
    // 写（带信号）
    void setName(const QString& v);
    void setInitial(double v);
    void setRemaining(double v);
    void setHeight(double v);
    void setX(double v);
    void setY(double v);
    void setPos(double x, double y); // 一次设置 x,y
    void setLot(const QString& v);
    void setExpiry(const QDate& v);
    void setBarcode(const QString& v);
    void setNote(const QString& v);

signals:
    void nameChanged(const QString&);
    void initialChanged(double);
    void remainingChanged(double);
    void heightChanged(double);
    void positionChanged(double x, double y);
    void lotChanged(const QString&);
    void expiryChanged(const QDate&);
    void barcodeChanged(const QString&);
    void noteChanged(const QString&);

private:
    QString m_name;
    double  m_initial  = 0.0;
    double  m_remaining= 0.0;
    double  m_height   = 0.0;
    double  m_x        = 0.0;
    double  m_y        = 0.0;
    QString m_lot;
    QDate   m_expiry;
    QString m_barcode;
    QString m_note;

signals:
};

#endif // REAGENTBOTTLE_H
