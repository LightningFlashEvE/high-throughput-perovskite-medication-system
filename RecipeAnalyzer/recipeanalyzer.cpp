#include "recipeanalyzer.h"
#include "ui_recipeanalyzer.h"
#include <QDebug>
#include <QHeaderView>
#include <QRegularExpression>
#include <QTextStream>
#include <QApplication>
#include <QAbstractItemView>
#include <QTableWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLabel>
#include <QTextEdit>
#include <QHeaderView>
#include <QMessageBox>
#include <QTextStream>
#include <QApplication>
#include <QRegularExpressionMatch>
#include <QRegularExpressionMatchIterator>
#include <cmath>
#include <QJsonDocument>

// 原子量常量
const QMap<QString, double> RecipeAnalyzer::ATOMIC_WEIGHTS = {
    {"H", 1.008}, {"C", 12.01}, {"N", 14.01}, {"O", 16.00}, {"F", 19.00},
    {"I", 126.90}, {"Br", 79.90}, {"Cl", 35.45}, {"Cs", 132.91}, {"Pb", 207.2},
    {"FA", 45.08},   // CH(NH2)2+
    {"MA", 32.07},   // CH3NH3+
};

// 前驱体分子量常量（动态计算）
const QMap<QString, double> RecipeAnalyzer::PRECURSOR_WEIGHTS = {
    {"FAI", 45.08 + 126.90},   // FA + I = 171.98
    {"MAI", 32.07 + 126.90},   // MA + I = 158.97
    {"MABr", 32.07 + 79.90},   // MA + Br = 111.97
    {"MACl", 32.07 + 35.45},   // MA + Cl = 67.52
    {"CsI", 132.91 + 126.90},  // Cs + I = 259.81
    {"PbI2", 207.2 + 2*126.90}, // Pb + 2*I = 461.00
    {"PbBr2", 207.2 + 2*79.90}, // Pb + 2*Br = 367.00
    {"PbCl2", 207.2 + 2*35.45}, // Pb + 2*Cl = 278.10
};

RecipeAnalyzer::RecipeAnalyzer(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::RecipeAnalyzer)
{
    ui->setupUi(this);
    setupUI();
    
    // 连接信号槽
    connect(ui->pushButton_calculate, &QPushButton::clicked, this, &RecipeAnalyzer::onCalculateClicked);
    connect(ui->pushButton_send_recipe, &QPushButton::clicked, this, &RecipeAnalyzer::onSendRecipeClicked);
    connect(ui->lineEdit_formula, &QLineEdit::textChanged, this, &RecipeAnalyzer::onFormulaChanged);
    
    // 新增：初始化高级溶剂选择系统
    setupAdvancedSolventSystem();
}

RecipeAnalyzer::~RecipeAnalyzer()
{
    delete ui;
}

void RecipeAnalyzer::setupUI()
{
    // 设置前驱体表格属性
    ui->tableWidget_precursors->setColumnCount(3);
    ui->tableWidget_precursors->setHorizontalHeaderLabels({"前驱体", "摩尔数 (mol)", "质量 (g)"});
    ui->tableWidget_precursors->horizontalHeader()->setStretchLastSection(true);
    ui->tableWidget_precursors->setAlternatingRowColors(true);
    ui->tableWidget_precursors->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableWidget_precursors->setSortingEnabled(true);
    
    // 设置溶剂表格属性（合并动态选择和结果显示功能）
    ui->tableWidget_solvents->setColumnCount(5);
    ui->tableWidget_solvents->setHorizontalHeaderLabels({"溶剂", "比例 (%)", "体积 (mL)", "状态", "操作"});
    ui->tableWidget_solvents->horizontalHeader()->setStretchLastSection(true);
    ui->tableWidget_solvents->setAlternatingRowColors(true);
    ui->tableWidget_solvents->setSelectionMode(QAbstractItemView::NoSelection); // 禁用行选中效果
    ui->tableWidget_solvents->setFocusPolicy(Qt::NoFocus); // 禁用焦点效果
    ui->tableWidget_solvents->setSortingEnabled(false);
    
    // 设置窗口属性
    setWindowTitle("配方解析器 - 钙钛矿前驱体计算");
    setMinimumSize(500, 700); // 增加最小尺寸以适应双表格


}

void RecipeAnalyzer::onCalculateClicked()
{
    try {
        // 获取输入参数
        QString formula = ui->lineEdit_formula->text().trimmed();
        double molarity = ui->doubleSpinBox_molarity->value();
        double volume = ui->doubleSpinBox_volume->value();
        
        if (formula.isEmpty()) {
            QMessageBox::warning(this, "输入错误", "请输入化学式！");
            return;
        }
        
        if (molarity <= 0 || volume <= 0) {
            QMessageBox::warning(this, "输入错误", "摩尔浓度和体积必须大于0！");
            return;
        }
        
        // 计算前驱体
        QList<PrecursorResult> results = calculatePrecursors(formula, molarity, volume);
        
        // 更新结果表格
        updateResultsTable(results);
        
        // 更新溶剂配比表格（仅使用高级溶剂系统）
        QList<QPair<QString,double>> advancedSolvents = collectAdvancedSolvents(); // 高级方式
        
        // 保持双精度浮点比例用于显示
        QList<QPair<QString,double>> allSolvents = advancedSolvents;
        
        updateSolventsTable(allSolvents, volume);
        
        // 计算并显示验证信息
        QMap<QString, double> elements = parseFormula(formula);
        ValidationInfo validation = calculateValidationInfo(elements, molarity, volume);
        updateValidationInfo(validation);

        // 收集溶剂配置并打包数据
        m_lastPacket = buildRecipePacket(formula, molarity, volume, results, validation);
        // 可选：调试输出
        qDebug().noquote() << QJsonDocument(m_lastPacket).toJson(QJsonDocument::Compact);
        
        qDebug() << "配方解析完成，共计算" << results.size() << "种前驱体";
        
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "计算错误", QString("计算过程中发生错误：\n%1").arg(e.what()));
    } catch (...) {
        QMessageBox::critical(this, "计算错误", "计算过程中发生未知错误！");
    }
}

void RecipeAnalyzer::onFormulaChanged()
{
    // 实时验证化学式格式（可选功能）
    QString formula = ui->lineEdit_formula->text().trimmed();
    if (!formula.isEmpty()) {
        // 可以添加实时验证逻辑
    }
}





QJsonObject RecipeAnalyzer::buildRecipePacket(const QString& formula,
                                              double molarity,
                                              double volume,
                                              const QList<PrecursorResult>& results,
                                              const ValidationInfo& info) const
{
    QJsonObject obj;
    obj["时间戳"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    obj["化学式"] = formula;
    obj["摩尔浓度"] = molarity;
    obj["体积"] = volume;

    // 前驱体列表
    QJsonArray arr;
    for (const auto &r : results) {
        QJsonObject x;
        x["名称"] = r.name;
        x["摩尔数"] = r.moles;
        x["质量"] = r.grams;
        arr.append(x);
    }
    obj["前驱体"] = arr;

    // 溶剂与比例（仅使用高级溶剂系统）
    QJsonArray solvents;
    QList<QPair<QString,double>> advancedSolvents = collectAdvancedSolvents();
    for (const auto& pair : advancedSolvents) {
        QJsonObject s;
        s["名称"] = pair.first;
        s["比例"] = pair.second;
        s["体积"] = volume * pair.second / 100.0; // 计算实际体积
        solvents.append(s);
    }
    obj["溶剂"] = solvents;

    // 验证信息
    QJsonObject ck;
    ck["分子式单元摩尔数"] = info.formulaUnits_mol;
    ck["分子量"] = info.molecularWeight_g;
    ck["A位总摩尔数"] = info.A_site_total_mol;
    ck["Pb总摩尔数"] = info.Pb_mol;
    ck["PbX2提供的Pb摩尔数"] = info.Pb_from_PbX2_mol;
    obj["验证信息"] = ck;

    return obj;
}

// ==================== 新增：高级溶剂选择系统 ====================

void RecipeAnalyzer::setupAdvancedSolventSystem()
{
    // 初始化溶剂库
    m_availableSolvents = {"DMSO", "DMF", "GBL", "NMP", "DMAC", "THF", "Toluene", "CB", "DCB", "Anisole"};
    
    // 收集所有溶剂按钮
    m_solventButtons = {
        ui->pushButton_DMSO, ui->pushButton_DMF, ui->pushButton_GBL, ui->pushButton_NMP,
        ui->pushButton_DMAC, ui->pushButton_THF, ui->pushButton_Toluene, ui->pushButton_CB,
        ui->pushButton_DCB, ui->pushButton_Anisole
    };
    
    // 仅固定“选中态”颜色；其余态使用系统/主题默认，从而随明暗主题变化
    const QString kSolventButtonStyle =
        "QPushButton:checked {"
        "  background-color: #2aa198;"
        "  color: #ffffff;"
        "  border: 1px solid #238b83;"
        "  border-radius: 6px;"  /* 保持与系统默认一致的圆角 */
        "  padding: 6px 12px;"    /* 防止选中后尺寸轻微跳变 */
        "}"
        "QPushButton:checked:hover { background-color: #238b83; }";

    for (QPushButton* btn : m_solventButtons) {
        if (btn) btn->setStyleSheet(kSolventButtonStyle);
    }

    // 连接所有溶剂按钮的信号
    for (QPushButton* btn : m_solventButtons) {
        connect(btn, &QPushButton::clicked, this, &RecipeAnalyzer::onSolventButtonClicked);
    }
    
    // 连接清空按钮
    connect(ui->pushButton_clear_all, &QPushButton::clicked, this, &RecipeAnalyzer::onClearAllSolventsClicked);
    
    // 初始化统一溶剂表格的动态功能
    // tableWidget_solvents 已在 setupUI() 中初始化
    
    updateAdvancedTotal();
}

void RecipeAnalyzer::onSolventButtonClicked()
{
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    if (!button) return;
    
    QString solventName = button->text();
    
    if (button->isChecked()) {
        // 添加溶剂
        addSolventToTable(solventName);
    } else {
        // 移除溶剂
        for (int i = 0; i < m_dynamicSolvents.size(); ++i) {
            if (m_dynamicSolvents[i].first == solventName) {
                removeSolventFromTable(i);
                break;
            }
        }
    }
}

void RecipeAnalyzer::onClearAllSolventsClicked()
{
    // 清空动态溶剂列表
    m_dynamicSolvents.clear();
    
    // 取消所有按钮选中状态
    for (QPushButton* btn : m_solventButtons)
    {
        btn->setChecked(false);
    }
    
    // 更新表格显示
    updateSolventsTableDynamic();
    updateAdvancedTotal();
}

void RecipeAnalyzer::addSolventToTable(const QString& solventName)
{
    // 检查是否已存在
    for (const auto& pair : m_dynamicSolvents) {
        if (pair.first == solventName) {
            return; // 已存在，不重复添加
        }
    }
    
    // 计算当前已使用的百分比，仅根据现有项求和
    double usedPercent = 0.0;
    for (int i = 0; i < m_dynamicSolvents.size(); ++i) {
        usedPercent += m_dynamicSolvents[i].second;
    }

    // 新增溶剂只分配剩余百分比（不打扰已有设置）
    double remaining = qMax(0.0, 100.0 - usedPercent);
    m_dynamicSolvents.append({solventName, remaining});

    // 更新最后一项为只读“剩余”，其它不变
    recalculateAutoPercentages();
    
    // 更新显示
    updateSolventsTableDynamic();
    updateAdvancedTotal();
}

void RecipeAnalyzer::removeSolventFromTable(int row)
{
    if (row >= 0 && row < m_dynamicSolvents.size()) {
        QString solventName = m_dynamicSolvents[row].first;
        m_dynamicSolvents.removeAt(row);
        
        // 找到对应按钮并取消选中
        for (QPushButton* btn : m_solventButtons) {
            if (btn->text() == solventName) {
                btn->setChecked(false);
                break;
            }
        }
        
        // 重新计算最后一项为“剩余”，不动其它
        recalculateAutoPercentages();
        
        // 更新显示
        updateSolventsTableDynamic();
        updateAdvancedTotal();
    }
}

void RecipeAnalyzer::recalculateAutoPercentages()
{
    int count = m_dynamicSolvents.size();
    if (count == 0) return;
    
    if (count == 1) {
        // 只有一个溶剂，设为100%
        m_dynamicSolvents[0].second = 100.0;
        return;
    }

    // 多个溶剂：前 N-1 项保持不变，仅将最后一项设为“剩余”
    double usedPercent = 0.0;
    for (int i = 0; i < count - 1; ++i) {
        usedPercent += m_dynamicSolvents[i].second;
    }
    double remaining = 100.0 - usedPercent;
    if (remaining < 0.0) remaining = 0.0;
    if (remaining > 100.0) remaining = 100.0;
    m_dynamicSolvents[count - 1].second = remaining;
}


void RecipeAnalyzer::onDynamicPercentageChanged()
{
    // 收集当前表格中的百分比值
    for (int i = 0; i < m_dynamicSolvents.size(); ++i) {
        QDoubleSpinBox* spinBox = qobject_cast<QDoubleSpinBox*>(
            ui->tableWidget_solvents->cellWidget(i, 1));
        if (spinBox && !spinBox->isReadOnly()) {
            m_dynamicSolvents[i].second = spinBox->value();
        }
    }
    
    // 如果有多个溶剂，重新计算最后一个的百分比
    if (m_dynamicSolvents.size() > 1) {
        double usedPercent = 0.0;
        for (int i = 0; i < m_dynamicSolvents.size() - 1; ++i) {
            usedPercent += m_dynamicSolvents[i].second;
        }
        
        double remaining = qMax(0.0, 100.0 - usedPercent);
        m_dynamicSolvents.last().second = remaining;
        
        // 更新最后一个输入框的显示
        QDoubleSpinBox* lastSpinBox = qobject_cast<QDoubleSpinBox*>(
            ui->tableWidget_solvents->cellWidget(m_dynamicSolvents.size() - 1, 1));
        if (lastSpinBox) {
            lastSpinBox->setValue(remaining);
        }
    }
    
    updateAdvancedTotal();
}

void RecipeAnalyzer::onRemoveSolventClicked()
{
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    if (!button) return;
    
    int row = button->property("row").toInt();
    removeSolventFromTable(row);
}

void RecipeAnalyzer::updateAdvancedTotal()
{
    double total = 0.0;
    for (const auto& pair : m_dynamicSolvents) {
        total += pair.second;
    }
    
    ui->label_advanced_total->setText(QString("高级选择总计：%1%").arg(total, 0, 'f', 2));
    
    // 如果超过100%，显示警告颜色
    if (total > 100.0) {
        ui->label_advanced_total->setStyleSheet("font-weight: bold; color: #f44336;");
    } else {
        ui->label_advanced_total->setStyleSheet("font-weight: bold; color: #2196F3;");
    }
}

QList<QPair<QString,double>> RecipeAnalyzer::collectAdvancedSolvents() const
{
    return m_dynamicSolvents;
}

QMap<QString, double> RecipeAnalyzer::parseFormula(const QString& formula)
{
    return parseGroup(formula, 1.0);
}

QMap<QString, double> RecipeAnalyzer::parseGroup(const QString& group, double multiplier)
{
    QMap<QString, double> result;
    
    // 正则表达式：匹配 (subgroup)number 或 elementnumber
    QRegularExpression pattern(R"(\(([^\)]+)\)([0-9.]+)?|(FA|MA|[A-Z][a-z]?)([0-9.]+)?)");
    QRegularExpressionMatchIterator iter = pattern.globalMatch(group);
    
    while (iter.hasNext()) {
        QRegularExpressionMatch match = iter.next();
        
        if (match.captured(1).length() > 0) {
            // 匹配到 (subgroup)number
            QString subgroup = match.captured(1);
            double groupMultiplier = match.captured(2).isEmpty() ? 1.0 : match.captured(2).toDouble();
            
            QMap<QString, double> subResult = parseGroup(subgroup, multiplier * groupMultiplier);
            for (auto it = subResult.begin(); it != subResult.end(); ++it) {
                result[it.key()] += it.value();
            }
        } else if (match.captured(3).length() > 0) {
            // 匹配到 elementnumber
            QString element = match.captured(3);
            double count = match.captured(4).isEmpty() ? 1.0 : match.captured(4).toDouble();
            
            result[element] += count * multiplier;
        }
    }
    
    return result;
}

double RecipeAnalyzer::calculateMolecularWeight(const QMap<QString, double>& elements)
{
    double totalWeight = 0.0;
    for (auto it = elements.begin(); it != elements.end(); ++it) {
        totalWeight += getAtomicWeight(it.key()) * it.value();
    }
    return totalWeight;
}

QList<PrecursorResult> RecipeAnalyzer::calculatePrecursors(const QString& formula, double molarity, double volume)
{
    QMap<QString, double> elements = parseFormula(formula);
    double formulaUnits = molarity * (volume / 1000.0); // volume从mL转换为L
    
    // 计算各元素摩尔数
    double n_FA = elements.value("FA", 0.0) * formulaUnits;
    double n_MA = elements.value("MA", 0.0) * formulaUnits;
    double n_Cs = elements.value("Cs", 0.0) * formulaUnits;
    double n_Pb = elements.value("Pb", 0.0) * formulaUnits;
    
    // 计算卤素需求
    QMap<QString, double> halideNeeds = {
        {"I", elements.value("I", 0.0) * formulaUnits},
        {"Br", elements.value("Br", 0.0) * formulaUnits},
        {"Cl", elements.value("Cl", 0.0) * formulaUnits}
    };
    
    // 检查碘是否足够给FA和Cs
    double requiredI = n_FA + n_Cs;
    if (halideNeeds["I"] + 1e-12 < requiredI) {
        throw std::runtime_error(QString("碘不足：FA(%1) + Cs(%2) 需要 %3，但只有 %4")
            .arg(n_FA, 0, 'g', 6)
            .arg(n_Cs, 0, 'g', 6)
            .arg(requiredI, 0, 'g', 6)
            .arg(halideNeeds["I"], 0, 'g', 6)
            .toStdString());
    }
    
    QList<PrecursorResult> results;
    
    // 分配FA -> FAI
    if (n_FA > 0) {
        double faiMoles = n_FA;
        double faiGrams = faiMoles * PRECURSOR_WEIGHTS["FAI"];
        results.append(PrecursorResult("FAI", faiMoles, faiGrams));
        halideNeeds["I"] -= n_FA;
    }
    
    // 分配Cs -> CsI
    if (n_Cs > 0) {
        double csiMoles = n_Cs;
        double csiGrams = csiMoles * PRECURSOR_WEIGHTS["CsI"];
        results.append(PrecursorResult("CsI", csiMoles, csiGrams));
        halideNeeds["I"] -= n_Cs;
    }
    
    // 分配MA到剩余卤素
    if (n_MA > 0) {
        double totalHalideForMA = halideNeeds["I"] + halideNeeds["Br"] + halideNeeds["Cl"];
        if (totalHalideForMA + 1e-12 < n_MA) {
            throw std::runtime_error(QString("卤素不足：MA需要 %1，但只有 %2")
                .arg(n_MA, 0, 'g', 6)
                .arg(totalHalideForMA, 0, 'g', 6)
                .toStdString());
        }
        
        // 按比例分配MA到不同卤素
        QList<QPair<QString, QString>> parts = {
            {"MAI", "I"}, {"MABr", "Br"}, {"MACl", "Cl"}
        };
        
        double remaining = n_MA;
        double total = totalHalideForMA > 0 ? totalHalideForMA : 1.0;
        
        for (int i = 0; i < parts.size(); ++i) {
            QString salt = parts[i].first;
            QString halide = parts[i].second;
            
            double share = n_MA * (halideNeeds[halide] / total);
            double take = (i < parts.size() - 1) ? 
                qMin(share, qMin(halideNeeds[halide], remaining)) : remaining;
            take = qMax(0.0, take);
            
            if (take > 0) {
                double saltMoles = take;
                double saltGrams = saltMoles * PRECURSOR_WEIGHTS[salt];
                results.append(PrecursorResult(salt, saltMoles, saltGrams));
                halideNeeds[halide] -= take;
                remaining -= take;
            }
        }
        
        if (remaining > 1e-8) {
            throw std::runtime_error(QString("MA分配失败：剩余 %1 mol").arg(remaining, 0, 'g', 4).toStdString());
        }
    }
    
    // 剩余卤素分配给PbX2
    double pbI2Moles = halideNeeds["I"] / 2.0;
    double pbBr2Moles = halideNeeds["Br"] / 2.0;
    double pbCl2Moles = halideNeeds["Cl"] / 2.0;
    
    QList<QPair<QString, double>> pbSalts = {
        {"PbI2", pbI2Moles}, {"PbBr2", pbBr2Moles}, {"PbCl2", pbCl2Moles}
    };
    
    for (const auto& salt : pbSalts) {
        if (salt.second > 1e-12) {
            double saltGrams = salt.second * PRECURSOR_WEIGHTS[salt.first];
            results.append(PrecursorResult(salt.first, salt.second, saltGrams));
        }
    }
    
    // 验证Pb平衡
    double pbFromLeadSalts = pbI2Moles + pbBr2Moles + pbCl2Moles;
    if (qAbs(pbFromLeadSalts - n_Pb) > 1e-6) {
        throw std::runtime_error(QString("Pb不平衡：需要 %1，从PbX2得到 %2")
            .arg(n_Pb, 0, 'g', 6)
            .arg(pbFromLeadSalts, 0, 'g', 6)
            .toStdString());
    }
    
    return results;
}

ValidationInfo RecipeAnalyzer::calculateValidationInfo(const QMap<QString, double>& elements, double molarity, double volume)
{
    ValidationInfo info;
    
    double molecularWeight = calculateMolecularWeight(elements);
    double formulaUnits = molarity * (volume / 1000.0); // volume从mL转换为L
    
    info.formulaUnits_mol = formulaUnits;
    info.molecularWeight_g = molecularWeight;
    info.A_site_total_mol = (elements.value("FA", 0.0) + elements.value("MA", 0.0) + elements.value("Cs", 0.0)) * formulaUnits;
    info.Pb_mol = elements.value("Pb", 0.0) * formulaUnits;
    info.Pb_from_PbX2_mol = (elements.value("I", 0.0) + elements.value("Br", 0.0) + elements.value("Cl", 0.0)) * formulaUnits / 2.0;
    
    return info;
}

void RecipeAnalyzer::updateResultsTable(const QList<PrecursorResult>& results)
{
    // 只显示前驱体用量（溶剂配比现在显示在右侧表格）
    ui->tableWidget_precursors->setRowCount(results.size());
    
    for (int i = 0; i < results.size(); ++i) {
        const PrecursorResult& result = results[i];
        
        ui->tableWidget_precursors->setItem(i, 0, new QTableWidgetItem(result.name));
        ui->tableWidget_precursors->setItem(i, 1, new QTableWidgetItem(formatNumber(result.moles, 6)));
        ui->tableWidget_precursors->setItem(i, 2, new QTableWidgetItem(formatNumber(result.grams, 4)));
    }
    
    ui->tableWidget_precursors->resizeColumnsToContents();
}

// 更新动态选择表格（用于溶剂选择和比例设置）
void RecipeAnalyzer::updateSolventsTableDynamic()
{
    ui->tableWidget_solvents->setRowCount(m_dynamicSolvents.size());
    
    for (int i = 0; i < m_dynamicSolvents.size(); ++i) {
        const QString& name = m_dynamicSolvents[i].first;
        double percentage = m_dynamicSolvents[i].second;
        
        // 溶剂名称列
        ui->tableWidget_solvents->setItem(i, 0, new QTableWidgetItem(name));
        
        // 比例输入框（最后一行只读，其他可编辑）
        QDoubleSpinBox* spinBox = new QDoubleSpinBox();
        spinBox->setRange(0.0, 100.0);
        spinBox->setSuffix("%");
        spinBox->setDecimals(2);
        spinBox->setSingleStep(0.01);  // 设置步长为0.01%
        spinBox->setValue(percentage);
        
        // 只有最后一行是只读的（自动计算）
        if (i == m_dynamicSolvents.size() - 1 && m_dynamicSolvents.size() > 1) {
            // 最后一行只读：不改样式，交由主题控制（避免暗色下变成白块）
            spinBox->setReadOnly(true);
        } else {
            connect(spinBox, qOverload<double>(&QDoubleSpinBox::valueChanged), 
                    this, &RecipeAnalyzer::onDynamicPercentageChanged);
        }
        
        ui->tableWidget_solvents->setCellWidget(i, 1, spinBox);
        
        // 体积和状态列暂时为空（计算时才填充）
        ui->tableWidget_solvents->setItem(i, 2, new QTableWidgetItem("-"));
        ui->tableWidget_solvents->setItem(i, 3, new QTableWidgetItem("-"));
        
        // 删除按钮
        QPushButton* removeBtn = new QPushButton("删除");
        removeBtn->setStyleSheet(
            "QPushButton { "
            "    background-color: #ffcdd2; "  // 浅红色
            "    color: #d32f2f; "             // 深红色文字
            "    border: 1px solid #f8bbd9; "
            "    border-radius: 4px; "
            "    padding: 4px 8px; "
            "} "
            "QPushButton:hover { "
            "    background-color: #f44336; "  // 悬浮时深红色
            "    color: white; "
            "} "
            "QPushButton:pressed { "
            "    background-color: #d32f2f; "  // 按下时更深的红色
            "}"
        );
        removeBtn->setProperty("row", i);
        connect(removeBtn, &QPushButton::clicked, this, &RecipeAnalyzer::onRemoveSolventClicked);
        ui->tableWidget_solvents->setCellWidget(i, 4, removeBtn);
    }
    
    ui->tableWidget_solvents->resizeColumnsToContents();
}

void RecipeAnalyzer::updateSolventsTable(const QList<QPair<QString,double>>& solvents, double totalVolume)
{
    ui->tableWidget_solvents->setRowCount(solvents.size());
    
    for (int i = 0; i < solvents.size(); ++i) {
        const QString& name = solvents[i].first;
        const double ratio = solvents[i].second;
        
        // 计算对应体积 (输入已经是mL，直接按比例计算)
        double volume_mL = (totalVolume * ratio) / 100.0;
        
        // 设置表格项
        ui->tableWidget_solvents->setItem(i, 0, new QTableWidgetItem(name));
        ui->tableWidget_solvents->setItem(i, 1, new QTableWidgetItem(QString("%1%").arg(ratio, 0, 'f', 2)));
        ui->tableWidget_solvents->setItem(i, 2, new QTableWidgetItem(formatNumber(volume_mL, 2)));
        
        // 状态：根据比例判断
        QString status;
        if (ratio < 0.01) {  // 使用小阈值而不是精确比较0
            status = "未使用";
        } else if (ratio < 10.0) {
            status = "少量";
        } else if (ratio <= 50.0) {
            status = "适中";
        } else {
            status = "主要";
        }
        ui->tableWidget_solvents->setItem(i, 3, new QTableWidgetItem(status));
        
        // 第5列操作列设为空（计算结果显示模式不需要操作按钮）
        ui->tableWidget_solvents->setItem(i, 4, new QTableWidgetItem("-"));
    }
    
    ui->tableWidget_solvents->resizeColumnsToContents();
}

void RecipeAnalyzer::updateValidationInfo(const ValidationInfo& info)
{
    QString validationText;
    QTextStream stream(&validationText);
    
    stream << "=== 验证信息 ===\n";
    stream << "分子式单元摩尔数: " << formatNumber(info.formulaUnits_mol, 6) << " mol\n";
    stream << "分子量: " << formatNumber(info.molecularWeight_g, 2) << " g/mol\n";
    stream << "A位总摩尔数 (FA+MA+Cs): " << formatNumber(info.A_site_total_mol, 6) << " mol\n";
    stream << "Pb摩尔数: " << formatNumber(info.Pb_mol, 6) << " mol\n";
    stream << "从PbX2的Pb摩尔数: " << formatNumber(info.Pb_from_PbX2_mol, 6) << " mol\n";
    stream << "\n=== 化学式解析 ===\n";
    stream << "✓ 碘分配验证通过\n";
    stream << "✓ 卤素分配验证通过\n";
    stream << "✓ Pb平衡验证通过\n";
    
    ui->textEdit_validation->setPlainText(validationText);
}

double RecipeAnalyzer::getAtomicWeight(const QString& element)
{
    return ATOMIC_WEIGHTS.value(element, 0.0);
}

QString RecipeAnalyzer::formatNumber(double value, int decimals)
{
    return QString::number(value, 'f', decimals);
}

void RecipeAnalyzer::onSendRecipeClicked()
{
    // 检查是否有有效的配方数据
    if (m_lastPacket.isEmpty()) {
        QMessageBox::warning(this, "发送失败", "请先计算配方，然后再发送！");
        return;
    }
    
    // 确认发送
    QString formula = m_lastPacket.value("化学式").toString();
    double molarity = m_lastPacket.value("摩尔浓度").toDouble();
    double volume = m_lastPacket.value("体积").toDouble();
    
    QString confirmMsg = QString("确认发送配方？\n\n"
                                "化学式：%1\n"
                                "摩尔浓度：%2 M\n"
                                "体积：%3 mL")
                                .arg(formula)
                                .arg(molarity)
                                .arg(volume);
    
    int ret = QMessageBox::question(this, "确认发送", confirmMsg, 
                                   QMessageBox::Yes | QMessageBox::No, 
                                   QMessageBox::No);
    
    if (ret == QMessageBox::Yes) {
        // 发射信号，传递配方数据包
        emit recipeReadyToSend(m_lastPacket);
        
        // 显示成功消息
        QMessageBox::information(this, "发送成功", "配方已发送！");
        
        qDebug() << "配方发送成功：" << formula;
    }
}
