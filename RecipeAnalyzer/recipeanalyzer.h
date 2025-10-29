#ifndef RECIPEANALYZER_H
#define RECIPEANALYZER_H

#include <QWidget>
#include <QMap>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QRegularExpression>
#include <QTableWidgetItem>
#include <QMessageBox>
#include <QTextStream>

namespace Ui {
class RecipeAnalyzer;
}

// 前驱体结果结构
struct PrecursorResult {
    QString name;           // 前驱体名称 (如 "FAI", "MAI")
    double moles;           // 摩尔数
    double grams;           // 质量(克)
    
    PrecursorResult(const QString& n = "", double m = 0.0, double g = 0.0) 
        : name(n), moles(m), grams(g) {}
};

// 验证信息结构
struct ValidationInfo {
    double formulaUnits_mol;        // 分子式单元摩尔数
    double molecularWeight_g;       // 分子量 (g/mol)
    double A_site_total_mol;        // A位总摩尔数 (FA+MA+Cs)
    double Pb_mol;                  // Pb摩尔数
    double Pb_from_PbX2_mol;        // 从PbX2来的Pb摩尔数
};

class RecipeAnalyzer : public QWidget
{
    Q_OBJECT

public:
    explicit RecipeAnalyzer(QWidget *parent = nullptr);
    ~RecipeAnalyzer();

signals:
    // 发送配方信号，传递配方数据包
    void recipeReadyToSend(const QJsonObject& recipePacket);

private slots:
    void onCalculateClicked();
    void onFormulaChanged();
    void onSendRecipeClicked(); // 新增：发送配方按钮点击槽函数
    
    // 新增：高级溶剂选择功能
    void onSolventButtonClicked();
    void onClearAllSolventsClicked();
    void onDynamicPercentageChanged();
    void onRemoveSolventClicked();

private:
    // UI相关
    void setupUI();
    void updateResultsTable(const QList<PrecursorResult>& results);
    void updateSolventsTable(const QList<QPair<QString,double>>& solvents, double totalVolume);
    void updateSolventsTableDynamic(); // 新增：统一溶剂表格的动态选择功能
    void updateValidationInfo(const ValidationInfo& info);
    
    // 新增：高级溶剂管理
    void setupAdvancedSolventSystem();
    void updateAdvancedTotal();
    void addSolventToTable(const QString& solventName);
    void removeSolventFromTable(int row);
    void recalculateAutoPercentages();
    QList<QPair<QString,double>> collectAdvancedSolvents() const; // name, percentage
    QJsonObject buildRecipePacket(const QString& formula,
                                  double molarity,
                                  double volume,
                                  const QList<PrecursorResult>& results,
                                  const ValidationInfo& info) const;
    
    // 核心计算功能
    QMap<QString, double> parseFormula(const QString& formula);
    double calculateMolecularWeight(const QMap<QString, double>& elements);
    QList<PrecursorResult> calculatePrecursors(const QString& formula, double molarity, double volume);
    ValidationInfo calculateValidationInfo(const QMap<QString, double>& elements, double molarity, double volume);
    
    // 辅助方法
    QMap<QString, double> parseGroup(const QString& group, double multiplier = 1.0);
    double getAtomicWeight(const QString& element);
    QString formatNumber(double value, int decimals = 6);
    
    // 常量数据
    static const QMap<QString, double> ATOMIC_WEIGHTS;
    static const QMap<QString, double> PRECURSOR_WEIGHTS;
    
    Ui::RecipeAnalyzer *ui;
    QJsonObject m_lastPacket;
    
    // 新增：高级溶剂选择数据
    QStringList m_availableSolvents;
    QList<QPair<QString,double>> m_dynamicSolvents; // name, percentage
    QList<QPushButton*> m_solventButtons;
};

#endif // RECIPEANALYZER_H