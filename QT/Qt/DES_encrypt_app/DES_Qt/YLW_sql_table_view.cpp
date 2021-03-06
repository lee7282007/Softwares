﻿#include <QFile>
#include <QtDebug>
#include <QHeaderView>
#include <QSqlError>
#include <QSqlRecord>
#include <QSqlIndex>
#include <QDateTime>
#include <QMessageBox>

#include "YLW_sql_table_view.h"
#include "YLW_VS_char_set.h"

const QString g_strSqlDriverName                    = "QSQLITE";
const QString g_strConnectName                      = "HWAPP";
static const QString g_strADInfoTableName           = "advinfo";
static const QString g_strVersionTableName          = "version";

static const QString g_strFileClose                 = "关闭";
static const QString g_strFileOpen                  = "打开";

SQLOperateWidget::SQLOperateWidget(QWidget *parent)
    : QWidget(parent)
    , m_strCurrentTable(QString(""))
    , m_pSqlTableModel(NULL)
    , m_bOpenDatabase(false)
{
    this->setAcceptDrops(true);

    m_pTableView = new MyTableView(this);
    this->setAcceptDrops(true);


    m_pGroupBoxStampConvert = new QGroupBox(this);
    m_pLabelUnixBefore = new QLabel(tr("Unix时间戳:"), this);
    m_pLineEditUnixBefore = new QLineEdit(this);
    m_pButtonConvertToBJ = new QPushButton(tr("转换为北京时间:"), this);
    m_pLineEditBJAfter = new QLineEdit(this);

    m_pLabelBJBefore = new QLabel(tr("北京时间(年/月/日 时:分:秒)"), this);
    m_pLineEditBJBefore = new QLineEdit(this);
    m_pButtonConvertToUnix = new QPushButton(tr("转换为Unix时间戳:"), this);
    m_pLineEditUnixAfter = new QLineEdit(this);
    m_pButtonCopyUnix = new QPushButton(tr("复制"), this);

    QHBoxLayout *pHBoxConvertToBJ = new QHBoxLayout;
    pHBoxConvertToBJ->addWidget(m_pLabelUnixBefore);//, 0, Qt::AlignRight);
    pHBoxConvertToBJ->addWidget(m_pLineEditUnixBefore);//, 0, Qt::AlignLeft);
    pHBoxConvertToBJ->addWidget(m_pButtonConvertToBJ);//, 0, Qt::AlignLeft);
    pHBoxConvertToBJ->addWidget(m_pLineEditBJAfter);//, 0, Qt::AlignLeft);
    connect(m_pButtonConvertToBJ, SIGNAL(clicked(bool)), this, SLOT(slotConvertTimeToBJ()));

    QHBoxLayout *pHBoxConvertToUnix = new QHBoxLayout;
    pHBoxConvertToUnix->addWidget(m_pLabelBJBefore, 0, Qt::AlignLeft);
    pHBoxConvertToUnix->addWidget(m_pLineEditBJBefore);
    pHBoxConvertToUnix->addWidget(m_pButtonConvertToUnix);
    pHBoxConvertToUnix->addWidget(m_pLineEditUnixAfter);
    pHBoxConvertToUnix->addWidget(m_pButtonCopyUnix);
    connect(m_pButtonConvertToUnix, SIGNAL(clicked(bool)), this, SLOT(slotConvertTimeToUnix()));

    QVBoxLayout *pVBoxLayoutConvert = new QVBoxLayout;
    pVBoxLayoutConvert->addLayout(pHBoxConvertToBJ);
    pVBoxLayoutConvert->addLayout(pHBoxConvertToUnix);

    m_pGroupBoxStampConvert->setLayout(pVBoxLayoutConvert);

    /* 数据库操作部分 */
    m_pLabelSqlFile = new QLabel(tr("数据库文件:"), this);

    m_pLineEditSqlFile = new FileLineEdit(this);
    connect(m_pLineEditSqlFile, SIGNAL(textChanged(QString)), this, SLOT(slotSqlFileInputChanged(QString)));
    /* 除了QLineEdit自身接受文件拖入以外，也可以将拖入QTableView的文件设置到QLineEdit */
    connect(m_pTableView, SIGNAL(sigFileIn(QString)), m_pLineEditSqlFile, SLOT(setText(QString)));

    m_pButtonSqlFileBrowse = new QPushButton(tr("浏览"), this);
    connect(m_pButtonSqlFileBrowse, SIGNAL(clicked(bool)), this, SLOT(slotBrowseSqlFile()));
    m_pButtonSqlFileClose = new QPushButton(g_strFileClose, this);
    connect(m_pButtonSqlFileClose, SIGNAL(clicked(bool)), this, SLOT(slotCloseSqlFile()));
    m_pButtonSqlFileClose->setEnabled(false);

    m_pLabelDataTable = new QLabel(tr("表:"), this);
    m_pComboBoxDataTable = new CMyComboBox(this);
    connect(m_pComboBoxDataTable, SIGNAL(activated(QString)), this, SLOT(slotSetCurrentTable(QString)));

    m_pButtonRefresh = new QPushButton(QIcon(":/refresh.png"), "", this);
    m_pButtonRefresh->setShortcut(tr("F5"));
//    m_pButtonRefresh->setStatusTip(tr("刷新表数据"));
    /* statusTip应该是在状态栏的提示信息，而toolTip才是在部件上面的提示信息 */
    m_pButtonRefresh->setToolTip(tr("刷新表数据"));
    m_pButtonRefresh->setEnabled(false);
    connect(m_pButtonRefresh, SIGNAL(clicked(bool)), this, SLOT(slotRereshCurrentTable()));

    m_pButtonSaveChanges = new QPushButton(QIcon(":/save.png"), "", this);
    m_pButtonSaveChanges->setShortcut(tr("Ctrl+s"));
    m_pButtonSaveChanges->setToolTip(tr("保存更改"));
    m_pButtonSaveChanges->setEnabled(false);
    connect(m_pButtonSaveChanges, SIGNAL(clicked(bool)), this, SLOT(slotSaveChanges()));

    m_pButtonNewRecord = new QPushButton(tr("新建记录"), this);
    m_pButtonNewRecord->setEnabled(false);
    connect(m_pButtonNewRecord, SIGNAL(clicked(bool)), this, SLOT(slotAddNewRowRecord()));

    m_pButtonDeleteRecord = new QPushButton(tr("删除记录"), this);
    m_pButtonDeleteRecord->setEnabled(false);
    connect(m_pButtonDeleteRecord, SIGNAL(clicked(bool)), this, SLOT(slotDeleteRowRecord()));


    /* 数据库操作部分 */
    QHBoxLayout *pHLayoutSqlFile = new QHBoxLayout;
    pHLayoutSqlFile->addWidget(m_pLabelSqlFile, 0);
    pHLayoutSqlFile->addWidget(m_pLineEditSqlFile, 2);
    pHLayoutSqlFile->addWidget(m_pButtonSqlFileBrowse, 0, Qt::AlignLeft);
    pHLayoutSqlFile->addWidget(m_pButtonSqlFileClose, 0, Qt::AlignLeft);
    pHLayoutSqlFile->addStretch(1);

    QHBoxLayout *pHLayoutTable = new QHBoxLayout;
    pHLayoutTable->addWidget(m_pLabelDataTable, 0, Qt::AlignLeft);
    pHLayoutTable->addWidget(m_pComboBoxDataTable, 3);
    pHLayoutTable->addWidget(m_pButtonRefresh, 0);
    pHLayoutTable->addWidget(m_pButtonSaveChanges, 0);
    /* addStretch是按照比例进行添加空白部分，作用类似于Spacer; addSpacing则是按照像素大小进行添加，窗口大小变化时不好控制 */
    pHLayoutTable->addStretch(4);
    pHLayoutTable->addWidget(m_pButtonNewRecord, 0, Qt::AlignRight);
    pHLayoutTable->addWidget(m_pButtonDeleteRecord, 0, Qt::AlignRight);

    /* 该类的主Layout */
    m_pVLayoutSqlOper = new QVBoxLayout(this);
    m_pVLayoutSqlOper->addWidget(m_pGroupBoxStampConvert);
    m_pVLayoutSqlOper->addLayout(pHLayoutSqlFile);
    m_pVLayoutSqlOper->addLayout(pHLayoutTable);
    m_pVLayoutSqlOper->addWidget(m_pTableView);
}


SQLOperateWidget::~SQLOperateWidget()
{

}

void SQLOperateWidget::setSqlDatabaseFile()
{
    initDatabase();
    if (openDatabase()) {
        /* 只有成功打开了数据库，才能进行初始化的显示 */
        initTableViewShow();
        m_pButtonSqlFileClose->setEnabled(true);
        m_pButtonSqlFileClose->setText(g_strFileClose);
    } else {
        QMessageBox::warning(this, tr("数据库操作"),tr("数据库文件打开失败, 请确认文件是否合法!"));
        m_pButtonSqlFileClose->setText(g_strFileOpen);
    }
}

void SQLOperateWidget::slotSetCurrentTable(const QString &strTableName)
{
    if (strTableName.isEmpty()) {
        qWarning() << "当前所选择的表名为空!";
        return;
    }

    m_strCurrentTable = strTableName;
    qDebug() << "当前所选表名:" << m_strCurrentTable;
    if (m_pSqlTableModel != NULL) {
        m_pSqlTableModel->clear();
        m_pSqlTableModel->deleteLater();
        m_pSqlTableModel = NULL;
    }
    m_pSqlTableModel = new QSqlTableModel(this, m_database);
    m_pSqlTableModel->setTable(m_strCurrentTable);
    connect(m_pSqlTableModel, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)), this, SLOT(slotSqlModelDataChanged()));

    /* 字段改变后同步到数据库的机制 */
    m_pSqlTableModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    m_pSqlTableModel->select();

    /* 有的数据库或平台不支持直接查询当前数据库的大小，则需要加上下面几句，否则不能通过model.rowCount()获取到行数 */
    while (m_pSqlTableModel->canFetchMore()) {
        m_pSqlTableModel->fetchMore();
    }

    m_pButtonRefresh->setEnabled(false);
    m_pButtonSaveChanges->setEnabled(false);
    m_pButtonNewRecord->setEnabled(true);
    setButtonDeleteEnable();

    m_pTableView->setModel(m_pSqlTableModel);

    m_pTableView->show();
}

void SQLOperateWidget::slotBrowseSqlFile()
{
#ifdef Q_OS_UNIX
    QString strFilePath = QFileDialog::getOpenFileName(this, tr("选择一个将计算其MD5值的文件"), "/", "file(*.*)");
#else
    QString strFilePath = QFileDialog::getOpenFileName(this, tr("选择一个将计算其MD5值的文件"), "Desktop", "file(*.*)");
#endif
    if (!strFilePath.isEmpty()) {
        qDebug() << "打开的文件为:" << strFilePath;
        m_pLineEditSqlFile->setText(strFilePath);
    }
}

void SQLOperateWidget::slotSqlFileInputChanged(const QString &strFilePath)
{
    if (!strFilePath.isEmpty()) {
        if (QFile::exists(strFilePath)) {
            if (m_pSqlTableModel) {
                /* 先关闭之前打开的数据库文件 */
                slotCloseSqlFile();
            }
            setSqlDatabaseFile();
        } else {
            qWarning() << "Sql database file is not exist:" << strFilePath;
        }
    }
}

void SQLOperateWidget::slotRereshCurrentTable()
{
    qDebug() << "刷新当前表数据...";
    slotSetCurrentTable(m_strCurrentTable);
}

void SQLOperateWidget::slotSaveChanges()
{
    if (m_pSqlTableModel) {
        if (!m_pSqlTableModel->submitAll()) {
            qWarning() << "数据提交失败:" << m_pSqlTableModel->lastError();
        } else {
            m_pButtonSaveChanges->setEnabled(false);
            m_pButtonRefresh->setEnabled(false);
        }
    }
}

void SQLOperateWidget::slotAddNewRowRecord()
{
    if (m_pSqlTableModel) {
        int rowCount = m_pSqlTableModel->rowCount();
        m_pSqlTableModel->insertRow(rowCount);

        QSqlIndex sqlIndex = m_pSqlTableModel->primaryKey();
        if (!sqlIndex.isEmpty()) {
            QString strPrimaryKeyName = sqlIndex.fieldName(0);
            QString strData = m_pSqlTableModel->data(m_pSqlTableModel->index(rowCount - 1, 0)).toString();
            qDebug() << "Primary key name:" << strPrimaryKeyName;
            qDebug() << "Last primary key value:" << strData;

            m_pSqlTableModel->setData(m_pSqlTableModel->index(rowCount, 0), QString("%1").arg(QDateTime::currentMSecsSinceEpoch()));
        }
    }
}

void SQLOperateWidget::slotDeleteRowRecord()
{
    if (m_pSqlTableModel) {
        /* QTableView自带QItemSelectionModel,不需要再重新new一个，并且也可以共享给其他视图使用 */
        QModelIndexList selectedIndexes = m_pTableView->selectionModel()->selectedRows(0);
        foreach (const QModelIndex modelIndex, selectedIndexes) {
            qDebug() << "Current row about to deleted:" << modelIndex.row();
            m_pSqlTableModel->removeRow(modelIndex.row());
            /* 这里主动发射dataChanged信号，是因为删除行的时候，QT并没有发射此信号，但是增加记录和修改记录时都会发射 */
            emit m_pSqlTableModel->dataChanged(modelIndex, modelIndex);
        }
    }
}

void SQLOperateWidget::slotSqlModelDataChanged()
{
    m_pButtonSaveChanges->setEnabled(true);
    m_pButtonRefresh->setEnabled(true);
}

void SQLOperateWidget::slotConvertTimeToBJ()
{
    if (m_pLineEditUnixBefore->text().constData()->isDigit()) {
        QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(m_pLineEditUnixBefore->text().toLongLong() * 1000);
        if (dateTime.isValid()) {
            m_pLineEditBJAfter->setText(dateTime.toString(Qt::SystemLocaleLongDate));
        } else {
            QMessageBox::warning(this, tr("时间戳转换"), tr("输入的时间戳无效，请核准后重新输入!"));
        }
    }
}

void SQLOperateWidget::slotConvertTimeToUnix()
{
    QDateTime dateTime = QDateTime::fromString(m_pLineEditBJBefore->text(), "yyyy/M/d h:m:s");
    if (dateTime.isValid()) {
        m_pLineEditUnixAfter->setText(QString("%1").arg(dateTime.toMSecsSinceEpoch() / 1000));
    } else {
        QMessageBox::warning(this, tr("时间戳转换"), tr("输入的北京时间无效，请核准后重新输入!"));
    }
}

void SQLOperateWidget::slotCloseSqlFile()
{
    if (m_pButtonSqlFileClose->text() == g_strFileClose) {
        if (m_pButtonSaveChanges->isEnabled()) {
            if (QMessageBox::Save == QMessageBox::warning(this, tr("数据库操作"), tr("当前有数据改变, 是否保存?"), QMessageBox::Save, QMessageBox::Discard)) {
                slotSaveChanges();
            }
        }
        closeDatabase();
        /* 这里需要删除代理 */
        if (m_pSqlTableModel) {
            m_pSqlTableModel->deleteLater();
            m_pSqlTableModel = NULL;
        }
        m_pTableView->clearSpans();
        m_pComboBoxDataTable->clear();
        m_pButtonNewRecord->setEnabled(false);
        m_pButtonDeleteRecord->setEnabled(false);
        m_pButtonSqlFileClose->setText(tr("打开"));
    } else {
        setSqlDatabaseFile();
    }
}

void SQLOperateWidget::dragEnterEvent(QDragEnterEvent *event)
{
    qDebug() << "SQLOperateWidget drag enter event...";
    if (event->mimeData()->hasUrls()) { //hasFormat("text/uri-list")) {
        event->acceptProposedAction();
    }
}

void SQLOperateWidget::dropEvent(QDropEvent *event)
{
    qDebug() << "SQLOperateWidget drop enter event...";
    if (event->mimeData()->hasUrls())
    {
        event->acceptProposedAction();
        QList<QUrl> listUrls = event->mimeData()->urls();
        QFileInfo *pFileInfo = new QFileInfo(listUrls.at(0).toLocalFile());
        m_pLineEditSqlFile->setText(pFileInfo->absoluteFilePath());
    }
}

void SQLOperateWidget::initDatabase()
{
    qDebug() << "初始化数据库:" << m_pLineEditSqlFile->text();

    m_database = QSqlDatabase::addDatabase(g_strSqlDriverName, g_strConnectName);
    m_database.setDatabaseName(m_pLineEditSqlFile->text());

    /* 检查数据库文件是否存在 */
    if (!QFile::exists(m_pLineEditSqlFile->text())) {
        qCritical() << "数据库文件不存在，无法读取数据！";
    }
}

bool SQLOperateWidget::openDatabase()
{
    /* 打开数据库 */
    if (!m_database.open()) {
        qCritical() << "数据库文件打开失败！";
        if (m_database.isValid()) {
            m_database.removeDatabase(g_strConnectName);
        }
        return false;
    }
    m_bOpenDatabase = true;
    qDebug() << "打开连接数据库成功！";
    return true;
}

void SQLOperateWidget::closeDatabase()
{
    qDebug() << "关闭数据库连接！";
    if (m_database.isOpen()) {
        m_database.close();
        /* 网上的方法，避免报connection is still in use的警告 */
        QString name;
        {
            name = QSqlDatabase::database().connectionName();
        } // 超出作用域，隐含对象QSqlDatabase::database()被删除。
        QSqlDatabase::removeDatabase(name);
    }
    m_bOpenDatabase = false;
}

void SQLOperateWidget::initTableViewShow()
{
    QSqlQuery query(m_database);
    QString strExec = QString("%1").arg("SELECT name FROM SQLITE_MASTER WHERE type='table';");
    qDebug() << "准备执行数据查询操作:" << strExec;
    if (query.exec(strExec)) {
        QSqlRecord record = query.record();
        if (record.count() > 0) {
            QStringList strList;
            strList.clear();
            while (query.next()) {
                strList.append(query.value(0).toString());
            }
            if (strList.isEmpty()) {
                qWarning() << "数据库为空!";
            } else {
                foreach (QString str, strList) {
                    qDebug() << str;
                }
                m_pComboBoxDataTable->clear();
                m_pComboBoxDataTable->addItems(strList);
                slotSetCurrentTable(strList[0]);
            }
        } else {
            qWarning() << "数据库表为空!";
        }
    } else {
        qCritical() << "查询数据库所有表名失败!";
        QString strWarning = QString("SQL查询失败，请确认: \n%1").arg(strExec);
        QMessageBox::warning(this, tr("数据库操作"), strWarning);
    }
}

void SQLOperateWidget::setButtonDeleteEnable()
{
    qDebug() << "QSqlTableModel row count:" << m_pSqlTableModel->rowCount();
    if (m_pSqlTableModel && m_pSqlTableModel->rowCount() > 0) {
        m_pButtonDeleteRecord->setEnabled(true);
    } else {
        m_pButtonDeleteRecord->setEnabled(false);
    }
}
