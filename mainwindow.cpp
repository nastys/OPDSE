#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "lyrics.cpp"
#include "diag_lipsync.h"
#include "lipsync_v1_0.cpp"
#include "lipsync_midiseq.h"
#include "diag_lipsync_vsqx.h"
#include "diag_addcommand.h"
#include "diag_format.h"
#include "diag_pvslot.h"
#include "diag_find2.h"
#include "diag_time.h"
#include "diag_merge.h"
#include "pdtime.h"
#include "qstring_helper.h"
#include "debug.h"
#include <QFileDialog>
#include <QFile>
#include <QDataStream>
#include <QInputDialog>
#include <QMessageBox>
#include <QTextBlock>
#include <QCheckBox>
#include <QScrollArea>
#include <QMimeData>
#include <QtEndian>
#include <QDesktopServices>
#include <QDirIterator>

#define leading(number, digits, base) QStringLiteral("%1").arg(number, digits, base, QLatin1Char('0'))
#define pvslot_leading leading(pvslot, 3, 10)

static int pvslot=1;
static bool isLoading=false;
static bool bigEndian=false;
static int dscfmt=4, dscver=335874337;
static QString currentFilePath="";
static bool preModified;
static bool errorState=false;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

#ifdef Q_OS_MACOS
    wrapAction = new QAction(tr("Wrap"));
    wrapAction->setCheckable(true);

    touchBar = new KDMacTouchBar(this);
    touchBar_main();
#endif
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::touchBar_main()
{
#ifdef Q_OS_MACOS
    touchBar->clear();

    QIcon openIcon(QStringLiteral(":/icons/icons8/color/icons/icons8/color/icons8-apri-cartella-96.png"));
    QAction *openAction = new QAction(openIcon, tr(""));
    touchBar->addAction(openAction);
    connect(openAction, &QAction::triggered, this, &MainWindow::on_actionOpen_DSC_triggered);

    QIcon saveIcon(QStringLiteral(":/icons/icons8/color/icons/icons8/color/icons8-salva-96.png"));
    QAction *saveAction = new QAction(saveIcon, tr(""));
    touchBar->addAction(saveAction);
    connect(saveAction, &QAction::triggered, this, &MainWindow::on_actionSave_DSC_triggered);

    QIcon undoIcon(QStringLiteral(":/icons/icons8/color/icons/icons8/color/icons8-annulla-96.png"));
    QAction *undoAction = new QAction(undoIcon, tr(""));
    touchBar->addAction(undoAction);
    connect(undoAction, &QAction::triggered, this, &MainWindow::on_actionUndo_triggered);

    QIcon redoIcon(QStringLiteral(":/icons/icons8/color/icons/icons8/color/icons8-ripeti-96.png"));
    QAction *redoAction = new QAction(redoIcon, tr(""));
    touchBar->addAction(redoAction);
    connect(redoAction, &QAction::triggered, this, &MainWindow::on_actionRedo_triggered);

    QIcon findIcon(QStringLiteral(":/icons/icons8/color/icons/icons8/color/icons8-trova-e-sostituisci-96.png"));
    QAction *findAction = new QAction(findIcon, tr(""));
    touchBar->addAction(findAction);
    connect(findAction, &QAction::triggered, this, &MainWindow::on_actionFind_and_replace_triggered);

    QIcon addIcon(QStringLiteral(":/icons/icons8/color/icons/icons8/color/icons8-aggiungi-96.png"));
    QAction *addAction = new QAction(addIcon, tr(""));
    touchBar->addAction(addAction);
    connect(addAction, &QAction::triggered, this, &MainWindow::on_actionAdd_command_triggered);

    QIcon removeIcon(QStringLiteral(":/icons/icons8/color/icons/icons8/color/icons8-rimuovi-96.png"));
    QAction *removeAction = new QAction(removeIcon, tr(""));
    removeAction->setEnabled(false);
    touchBar->addAction(removeAction);
    connect(removeAction, &QAction::triggered, this, &MainWindow::on_actionRemove_command_triggered);

    touchBar->addSeparator();

    QAction *toolsAction = new QAction(tr("Tools"));
    touchBar->addAction(toolsAction);
    connect(toolsAction, &QAction::triggered, this, &MainWindow::touchBar_tools);
#endif
}

void MainWindow::touchBar_tools()
{
#ifdef Q_OS_MACOS
    touchBar->clear();

    QAction *mainAction = new QAction(tr("Back"));
    touchBar->addAction(mainAction);
    connect(mainAction, &QAction::triggered, this, &MainWindow::touchBar_main);

    touchBar->addSeparator();
    touchBar->addSeparator();

    QAction *timeToolAction = new QAction(tr("Time"));
    touchBar->addAction(timeToolAction);
    connect(timeToolAction, &QAction::triggered, this, &MainWindow::on_actionTime_triggered);

    touchBar->addSeparator();

    touchBar->addAction(wrapAction);
    connect(wrapAction, &QAction::triggered, this, &MainWindow::toggleWrap);
#endif
}

void MainWindow::toggleWrap()
{
    ui->action_Wrap->toggle();
}

void MainWindow::on_actionOpen_DSC_triggered()
{
    QString filepath=QFileDialog::getOpenFileName();
    if(filepath.isEmpty()) return;
    openDSC(filepath);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
    {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    if(event->mimeData()->hasUrls())
        openDSC(event->mimeData()->urls().at(0).toLocalFile());
}

void MainWindow::openDSC(QString filepath, bool testopen)
{
    bool ok;
    QFile file(filepath);
    file.open(QIODevice::ReadOnly);
    QDataStream qds(&file);
    qds.setByteOrder(QDataStream::ByteOrder::LittleEndian);
    int old_dscfmt = dscfmt;
    if(!testopen)
    {
        int autodetected_ver=0, autodetected=0;
        qds >> autodetected_ver;
        autodetected_ver = qFromLittleEndian(autodetected_ver);
#ifdef QT_DEBUG
        debugger("Autodetected format:" << autodetected_ver);
#endif
        switch(autodetected_ver)
        {
        case 285614104: // Arcade
        case 335874337: // Future Tone
        case 369295649:
        case 353510679:
        case 352458520:
        case 335745816:
        case 335618838:
        case 319956249:
        case 319296802:
        case 318845217:
            //case 269615382: // DT 2nd dummy PV
            autodetected = 4;
            break;
        case 269615382: // Mirai DX
            autodetected = 8;
            break;
        case 1: // 1, i.e. no magic... Probably old Arcade/DT1, but it may also be PSP
            autodetected = 3;
            break;
        case 285419544: // DT 2nd/Extend
        case 285349657:
        case 302121504: // f/F
            autodetected = 5;
            break;
        case 1129535056: // F 2nd
            autodetected = 6; // assume F 2nd, but it may also be MGF or X/XHD
            break;
        default:
            autodetected = 0;
        }
        formatSelector(autodetected, autodetected_ver, ok);
        if(!ok) return;
    }
    file.reset();
    isLoading=true;
    ui->textEdit->setUndoRedoEnabled(false);
    DivaScriptOpcode *DSO = formatSelection(dscfmt, ui->textEdit_Log);
    bool updateFilename=false;
    if(!testopen&&dscfmt!=2&&dscfmt==old_dscfmt)
    {
        QStringList mergelist={};
        QPlainTextEdit *internalEdit = new QPlainTextEdit;
        internalEdit->setUndoRedoEnabled(false);
        DSO->readAll(file, qds, internalEdit, uiEditWidgets(), bigEndian?QDataStream::BigEndian:QDataStream::LittleEndian, testopen);
        delete DSO;
        diag_merge diag(this, &mergelist, internalEdit);
        bool accepted = diag.exec();
        if(accepted)
        {
            if(!mergelist.isEmpty())
            {
                mergeDsc(mergelist, ui->textEdit, internalEdit, isLoading, ui->textEdit_Log);
                delete internalEdit;
                isLoading=false;
                setTitleBarText(true);
            }
            else
            {
                updateFilename=true;
                ui->textEdit->setDocument(internalEdit->document());
                //delete internalEdit; // TODO fix memory leak
            }
        }
    }
    else
    {
        DSO->readAll(file, qds, ui->textEdit, uiEditWidgets(), bigEndian?QDataStream::BigEndian:QDataStream::LittleEndian, testopen);
        delete DSO;
        updateFilename=true;
    }

    if(updateFilename)
    {
        ui->textEdit->document()->clearUndoRedoStacks();
        ui->textEdit->document()->setModified(false);

        QFileInfo info(filepath);
        currentFilePath=filepath;
        QString filename=info.fileName();
        pvslot=1;
        if(filename.length()>=6 && filename.startsWith("pv_", Qt::CaseInsensitive))
        {
            QString filename_pvnum = filename.mid(3, 3);
            bool ok2=false;
            int pv_num = filename_pvnum.toInt(&ok2, 10);
            if(ok2) pvslot=pv_num;
        }
        setPvSlot();
        loadPvDbEntry();
        isLoading=false;
        setTitleBarText(false);
    }

    ui->textEdit->setUndoRedoEnabled(true);
    isLoading=false;
    file.close();
}

void MainWindow::formatSelector(int defaultsel, int version, bool& ok)
{
    Diag_Format diag(this);
    ok=false;
    diag.ok=&ok;
    diag.useBigEndian=&bigEndian;
    diag.format=&dscfmt;
    diag.formatVer=&dscver;
    diag.setDefaults(defaultsel, version);
    diag.exec();
    if(ok) ui->textEdit->document()->setModified(true);
}

bool MainWindow::on_actionSave_DSC_triggered()
{
    QFileInfo file(currentFilePath);
    if(file.exists())
    {
        if(ui->actionOverwrite_files_without_as_king->isChecked()||QMessageBox::question(this, "Save", "This will OVERWRITE the file. Continue?")==QMessageBox::Yes)
            return doSave(currentFilePath);
    }
    else return on_actionSave_DSC_as_triggered();

    return false;
}

bool MainWindow::on_actionSave_DSC_as_triggered()
{
    QFileInfo oldFile(currentFilePath);
    QString filepath=QFileDialog::getSaveFileName(this, "Save DSC as", oldFile.path(), "DivaScript (*.dsc);;Binary file (*)");
    if(filepath.isEmpty()||filepath.isNull()) return false;
    bool ok=false;
    formatSelector(dscfmt, dscver, ok);
    if(!ok) return false;
    return doSave(filepath);
}

void MainWindow::on_actionChang_e_format_triggered()
{
    bool ok=false;
    formatSelector(dscfmt, dscver, ok);
}

bool MainWindow::doSave(QString& filepath)
{
    QFile file(filepath);
    file.open(QIODevice::ReadWrite);
    file.resize(0);
    QDataStream qds(&file);
    qds.setByteOrder(bigEndian?QDataStream::BigEndian:QDataStream::LittleEndian);
    DivaScriptOpcode *DSO = formatSelection(dscfmt, ui->textEdit_Log);
    DSO->writeAll(file, qds, ui->textEdit, bigEndian?QDataStream::BigEndian:QDataStream::LittleEndian);
    delete DSO;
    if(errorState) errorState=false;
    if(file.error()) ui->textEdit_Log->append("E: "+file.errorString()+"\n");
    else
    {
        currentFilePath=filepath;
        ui->textEdit->document()->setModified(false);
        file.close();
        return true;
    }
    file.close();
    return false;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    on_actionExit_triggered();
    event->ignore();
}

void MainWindow::on_actionExit_triggered()
{
    if(!ui->textEdit->document()->isModified()) exit(0);
    switch(QMessageBox::question(this, "Exit", "Save your changes?", QMessageBox::StandardButtons(QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel)))
    {
        case QMessageBox::Yes:
            if(!on_actionSave_DSC_triggered()) return;
        case QMessageBox::No:
            exit(0);
        default:
            return;
    }
}

EditWidgets MainWindow::uiEditWidgets()
{
    EditWidgets editWidgets;
    editWidgets.tabedit=ui->tab_edit;
    editWidgets.bartable=ui->tableWidget_edit;
    editWidgets.editname=ui->lineEdit_edit_editname;
    editWidgets.songname=ui->lineEdit_edit_songname;
    editWidgets.songstart=ui->spinBox_edit_starttime;
    editWidgets.modulelist=ui->listWidget_edit_modules;
    editWidgets.stagelist=ui->listWidget_edit_stages;
    editWidgets.motionlist=ui->listWidget_edit_motions;
    editWidgets.effectlist=ui->listWidget_edit_effects;
    editWidgets.handitemlist=ui->listWidget_edit_handitems;
    editWidgets.songend=ui->spinBox_edit_end;
    editWidgets.buttonsfx=ui->spinBox_edit_buttonsfx;

    return editWidgets;
}

void MainWindow::on_actionPSP_to_DT_triggered()
{
    QString ifilepath=QFileDialog::getOpenFileName();
    if(ifilepath.isEmpty()||ifilepath.isNull()) return;

    QString ofilepath=QFileDialog::getSaveFileName();
    if(ofilepath.isEmpty()||ofilepath.isNull()) return;

    QFile ifile(ifilepath);
    ifile.open(QIODevice::ReadOnly);
    QDataStream iqds(&ifile);
    iqds.setByteOrder(QDataStream::ByteOrder::LittleEndian);

    QFile ofile(ofilepath);
    ofile.open(QIODevice::ReadWrite);
    ofile.resize(0);
    QDataStream oqds(&ofile);
    oqds.setByteOrder(QDataStream::ByteOrder::LittleEndian);

    oqds << 1677721600 << 16846880 << 192439748 << -192217088;
    while(!ifile.atEnd())
    {
        int v;
        iqds >> v;
        oqds << v;
    }
    ifile.close();
    ofile.close();
}

void MainWindow::on_actionImport_VTT_triggered()
{
    QString ifilepath=QFileDialog::getOpenFileName();
    if(ifilepath.isEmpty()||ifilepath.isNull()) return;
    QFile ifile(ifilepath);
    ifile.open(QIODevice::ReadOnly);
    fromVtt(ifile, ui->textEdit, ui->plainTextEdit_db, pvslot);
    ifile.close();
}

void MainWindow::on_actionLip_sync_triggered()
{
    QString ifilepath=QFileDialog::getOpenFileName();
    if(ifilepath.isEmpty()||ifilepath.isNull()) return;
    QFile ifile(ifilepath);
    ifile.open(QIODevice::ReadOnly);

    diag_lipsync *diag = new diag_lipsync(this);
    bool result = diag->exec();
    if(result) LipSync_v1_0::lipsyncFromVtt(ui->textEdit, ifile, diag->settings);
    ifile.close();
    delete diag;
}

void MainWindow::on_actionLip_sync_VSQX_triggered()
{
    QString ifilepath=QFileDialog::getOpenFileName();
    if(ifilepath.isEmpty()||ifilepath.isNull()) return;
    diag_lipsync_vsqx *diag = new diag_lipsync_vsqx(this);
    bool result = diag->exec();
    if(result)
    {
        delete diag;
        isLoading = true;
        QFile ifile(ifilepath);
        ifile.open(QIODevice::ReadOnly);
        midiseq::Sequence *sequence = new midiseq::Sequence_VSQX;
        sequence->readSequence(ifile);
        ifile.close();
        midiseq::DscSequence dscseq;
        dscseq.fromSequence(*sequence, diag->settings);
        dscseq.applyCommands(ui->textEdit, diag->settings);
        delete sequence;
        isLoading = false;
    }
}

void MainWindow::on_actionPDA_2_00_to_PDA_1_01_triggered()
{
    DivaScriptOpcode_PDA DSO(ui->textEdit_Log);

    QStringList commandlist;
    commandlist=ui->textEdit->document()->toPlainText().split(';', Qt::SkipEmptyParts).replaceInStrings("\n", "").replaceInStrings(" ", "");
    ui->textEdit->clear();
    for(int i=0; i<commandlist.length();)
    {
        QStringList command = commandlist.at(i).split('(');
        int opcode=DSO.getOpcodeNumber(command.at(0));
        if(opcode==-1)
        {
            i++;
            continue;
        }
        QStringList parameters = command.at(1).split(')').at(0).split(',');
        if(opcode==0x18/*LYRIC*/) parameters.removeLast();
        else if(opcode==0x1F/*MOVE_CAMERA*/) {parameters.removeLast(); parameters.removeLast();}
        int paramcount=DSO.getOpcodeParamCount(opcode);
        while(parameters.length()>paramcount) parameters.removeFirst();
        QString finalcommand = command.at(0)+'(';
        for(int j=0; j<parameters.length(); j++)
        {
            if(j>0) finalcommand.append(", ");
            finalcommand.append(parameters.at(j));
        }
        finalcommand.append(");");
        ui->textEdit->insertPlainText(finalcommand);
        i++;
    }
}

int getNewID(int oldID, QString idcommand)
{
    if(idcommand=="MOUTH_ANIM")
    {
        switch(oldID)
        {
        case 6:
            return 28;
        case 0:
            return 24;
        case 2:
            return 25;
        case 3:
            return 26;
        case 4:
            return 27;
        case 8:
            return 29;
        case 10:
            return 29;
        case 9:
            return 30;
        case 1:
            return 31;
        case 5:
            return 32;
        case 7:
            return 33;

        case 23:
            return 6;
        case 24:
            return 0;
        case 25:
            return 2;
        case 26:
            return 3;
        case 27:
            return 4;
        case 28:
            return 6;
        case 29:
            return 8;
        case 30:
            return 9;
        case 31:
            return 1;
        case 32:
            return 5;
        case 33:
            return 7;
        }
    }
    else if(idcommand=="EXPRESSION")
    {
        switch(oldID)
        {
        case 52:
            return 0;
        case 53:
            return 3;
        case 54:
            return 6;
        case 55:
            return 8;
        case 56:
            return 9;
        case 57:
            return 10;
        case 58:
            return 11;
        case 59:
            return 12;
        case 65:
            return 2;
        case 66:
            return 1;
        case 68:
            return 5;
        case 70:
            return 7;
        case 72:
            return 13;
        case 73:
            return 14;

        case 0:
            return 52;
        case 3:
            return 53;
        case 6:
            return 54;
        case 8:
            return 55;
        case 9:
            return 56;
        case 2:
            return 65;
        case 1:
            return 66;
        case 10:
            return 57;
        case 11:
            return 58;
        case 12:
            return 59;
        case 5:
            return 68;
        case 7:
            return 70;
        case 13:
            return 72;
        case 14:
            return 73;

        case 4:
            return oldID;
        }
    }
    else if(idcommand=="HAND_ANIM")
    {
        switch(oldID)
        {
        case 9:
            return 8;
        case 11:
            return 10;

        case 8:
            return 9;
        case 10:
            return 11;

        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
            return oldID;
        }
    }
    else if(idcommand=="LOOK_ANIM")
    {
        if(oldID<11)
            return oldID+11;

        return oldID-11;
    }

    QMessageBox::warning(nullptr, "Warning", "Unknown "+idcommand+ " ID "+QString::number(oldID)+'.');
    return oldID;
}

void MainWindow::replaceID(QString idcommand)
{
    DivaScriptOpcode_PDA DSO(ui->textEdit_Log);

    QStringList commandlist;
    commandlist=ui->textEdit->document()->toPlainText().split(';', Qt::SkipEmptyParts).replaceInStrings("\n", "");
    ui->textEdit->clear();
    for(int i=0; i<commandlist.length(); i++)
    {
        QStringList command = commandlist.at(i).split('(');
        if(command.at(0)!=idcommand)
        {
            ui->textEdit->insertPlainText(commandlist.at(i)+';');
            continue;
        }
        int opcode=DSO.getOpcodeNumber(command.at(0));
        if(opcode==-1) continue;
        QStringList parameters = command.at(1).split(')').at(0).split(',');

        int offset=0;

        if(idcommand=="HAND_ANIM"&&ui->actionI_have_already_converted_the_DSC_to_PDA_1_01->isChecked()==false)
            offset=2;
        else if(idcommand=="HAND_ANIM"&&ui->actionI_have_already_converted_the_DSC_to_PDA_1_01->isChecked())
            offset=1;
        else if(idcommand=="MOUTH_ANIM"&&ui->actionI_have_already_converted_the_DSC_to_PDA_1_01->isChecked()==false)
            offset=2;
        else if(idcommand=="MOUTH_ANIM"&&ui->actionI_have_already_converted_the_DSC_to_PDA_1_01->isChecked())
            offset=0;
        else if(idcommand=="EXPRESSION"&&ui->actionI_have_already_converted_the_DSC_to_PDA_1_01->isChecked()==false)
            offset=1;
        else if(idcommand=="EXPRESSION"&&ui->actionI_have_already_converted_the_DSC_to_PDA_1_01->isChecked())
            offset=0;
        else if(idcommand=="LOOK_ANIM"&&ui->actionI_have_already_converted_the_DSC_to_PDA_1_01->isChecked()==false)
            offset=1;
        else if(idcommand=="LOOK_ANIM"&&ui->actionI_have_already_converted_the_DSC_to_PDA_1_01->isChecked())
            offset=0;

        QString spc="";
        if(offset>0) spc.append(' ');
        parameters[offset]=spc+QString::number(getNewID(parameters.at(offset).toInt(), idcommand));
        QString finalcommand = command.at(0)+'(';
        for(int j=0; j<parameters.length(); j++)
        {
            if(j>0) finalcommand.append(',');
            finalcommand.append(parameters.at(j));
        }
        finalcommand.append(");");
        ui->textEdit->insertPlainText(finalcommand);
    }
}

void MainWindow::on_action_FT_new_mouth_to_FT_old_mouth_triggered()
{
    replaceID("MOUTH_ANIM");
}

void MainWindow::on_action_Look_FT_old_look_triggered()
{
    replaceID("LOOK_ANIM");
}

void MainWindow::on_action_Expressions_FT_old_expressions_triggered()
{
    replaceID("EXPRESSION");
}

void MainWindow::on_action_Hand_animations_FT_old_animations_triggered()
{
    replaceID("HAND_ANIM");
}

void MainWindow::on_actionAll_of_them_triggered()
{
    replaceID("MOUTH_ANIM");
    replaceID("LOOK_ANIM");
    replaceID("EXPRESSION");
    replaceID("HAND_ANIM");
}

void MainWindow::on_actionBAR_TIME_SET_TARGET_FLYING_TIME_triggered()
{
    QStringList commandlist;
    commandlist=ui->textEdit->document()->toPlainText().split(';', Qt::SkipEmptyParts).replaceInStrings("\n", "");
    ui->textEdit->clear();
    for(int i=0; i<commandlist.length(); i++)
    {
        QStringList command = commandlist.at(i).split('(');
        QString finalcommand;
        if(command.at(0)=="TARGET_FLYING_TIME")
        {
            double tft = command.at(1).split(')').at(0).toInt();
            int bpm=qRound(240000.00/tft);
            finalcommand="BAR_TIME_SET("+QString::number(bpm)+", 3);";
        }
        else if(command.at(0)=="BAR_TIME_SET")
        {
            QStringList parameters = command.at(1).split(')').at(0).split(',');
            double bpm = parameters.at(0).toInt();
            double ts = parameters.at(1).toInt();
            int tft=qRound(1000.0 / (bpm / ((ts + 1) * 60.0)));
            finalcommand="TARGET_FLYING_TIME("+QString::number(tft)+");";
        }
        else
        {
            ui->textEdit->insertPlainText(commandlist.at(i)+';');
            continue;
        }
        ui->textEdit->insertPlainText(finalcommand);
    }
}

void MainWindow::on_actionAbout_triggered()
{
    QMessageBox::about(this, "About Open PD Script Editor", "MIT License\nCopyright (c) 2020 nastys\n\nPermission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the \"Software\"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:\n\nThe above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.\n\nTHE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.");
    QMessageBox i8about;
    i8about.setTextFormat(Qt::RichText);
    //i8about.setIcon();
    i8about.setText("Icons by Icons8<br><a href='https://icons8.it/'>https://icons8.it</a>");
    i8about.setStandardButtons(QMessageBox::Ok);
    //i8about.setParent(this);
    i8about.exec();
    QMessageBox::aboutQt(this, "About Qt");
}

void MainWindow::on_textEdit_cursorPositionChanged()
{
    if(isLoading) return;

    QLayout *curlayout = ui->frame_paramEdit->layout();
    if ( curlayout != NULL )
    {
        QLayoutItem* item;
        while ( ( item = curlayout->takeAt( 0 ) ) != NULL )
        {
            delete item->widget();
            delete item;
        }
        delete curlayout->layout();
    }

    QPlainTextEdit *edit = qobject_cast<QPlainTextEdit *>(sender());
    Q_ASSERT(edit);
    QTextCursor cursor = edit->textCursor();
    cursor.movePosition(QTextCursor::StartOfLine);

    int line = 0;
    while(cursor.positionInBlock()>0) {
        cursor.movePosition(QTextCursor::Up);
        line++;
    }
    QTextBlock block = cursor.block().previous();

    while(block.isValid()) {
        line += block.lineCount();
        block = block.previous();
    }

    QScrollArea *scrollarea = new QScrollArea();
    QWidget *frame = new QWidget(scrollarea);
    QVBoxLayout *layout = new QVBoxLayout(frame);
    scrollarea->setWidgetResizable(true);
    scrollarea->setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAsNeeded);
    scrollarea->setFrameStyle(QFrame::Plain);
    scrollarea->setWidget(frame);
    ui->statusBar->showMessage("Line "+QString::number(line+1));
    QString currline=ui->textEdit->document()->findBlockByLineNumber(line).text();
    if(currline.endsWith(");")) // TODO make it game-specific instead of assuming PDA 2.00
    {
        if(currline.startsWith("TIME("))
        {
            QLabel *label = new QLabel("Time");
            label->setStyleSheet("font-weight: bold");
            layout->addWidget(label);
            QFrame *separator = new QFrame;
            separator->setFrameShape(QFrame::HLine);
            separator->setFrameShadow(QFrame::Sunken);
            layout->addWidget(new QLabel("Waits until this timestamp."));
            layout->addWidget(separator);
            QString timestr=currline.mid(5, currline.length()-7);

            pdsplittime pdtime = pdtime_split(timestr.toInt());

            layout->addWidget(new QLabel("Hours:"));
            QSpinBox *sb_hours = new QSpinBox;
            sb_hours->setValue(pdtime.hours);
            layout->addWidget(sb_hours);
            QSpinBox *sb_minutes = new QSpinBox;
            sb_minutes->setRange(0, 59);
            sb_minutes->setValue(pdtime.minutes);
            layout->addWidget(new QLabel("Minutes:"));
            layout->addWidget(sb_minutes);
            QSpinBox *sb_seconds = new QSpinBox;
            sb_seconds->setRange(0, 59);
            sb_seconds->setValue(pdtime.seconds);
            layout->addWidget(new QLabel("Seconds:"));
            layout->addWidget(sb_seconds);
            QSpinBox *sb_frac = new QSpinBox;
            sb_frac->setRange(0, 99999);
            sb_frac->setValue(pdtime.frac);
            layout->addWidget(new QLabel("Subseconds:"));
            layout->addWidget(sb_frac);
            QCheckBox *cb_move = new QCheckBox;
            cb_move->setText("Move commands");
            layout->addWidget(cb_move);
            layout->addStretch(1);
            /*QPushButton *pb_apply = new QPushButton;
            pb_apply->setText("Apply");
            layout->addWidget(pb_apply);*/
        }
        else if(currline.startsWith("TARGET("))
        {
            QStringList par_cs=chopped(currline, 2).split('(');
            if(par_cs.length()>=2)
            {
                QStringList parameters_str=par_cs.at(1).split(',');

                QLabel *label = new QLabel("Target");
                label->setStyleSheet("font-weight: bold");
                layout->addWidget(label);
                QFrame *separator = new QFrame;
                separator->setFrameShape(QFrame::HLine);
                separator->setFrameShadow(QFrame::Sunken);
                layout->addWidget(new QLabel("Displays a target."));
                layout->addWidget(separator);

                layout->addWidget(new QLabel("Type:"));
                QComboBox *typecb = new QComboBox;
                if(parameters_str.length()>=1)
                {
                    typecb->addItem(QIcon("qrc:/icons/icons8/color/icons8-cerchio-48.png"), "Circle"); // TODO fix icon
                }
                layout->addWidget(typecb);

                layout->addWidget(new QLabel("Horizontal position:"));
                QSpinBox *posx = new QSpinBox;
                posx->setRange(INT_MIN, INT_MAX);
                if(parameters_str.length()>=2) posx->setValue(parameters_str.at(1).toInt());
                layout->addWidget(posx);

                layout->addWidget(new QLabel("Vertical position:"));
                QSpinBox *posy = new QSpinBox;
                posy->setRange(INT_MIN, INT_MAX);
                if(parameters_str.length()>=3) posy->setValue(parameters_str.at(2).toInt());
                layout->addWidget(posy);

                layout->addWidget(new QLabel("Angle:"));
                QSpinBox *angle = new QSpinBox;
                angle->setRange(INT_MIN, INT_MAX);
                if(parameters_str.length()>=4) angle->setValue(parameters_str.at(3).toInt());
                layout->addWidget(angle);

                layout->addWidget(new QLabel("Distance:"));
                QSpinBox *distance = new QSpinBox;
                distance->setRange(INT_MIN, INT_MAX);
                if(parameters_str.length()>=5) distance->setValue(parameters_str.at(4).toInt());
                layout->addWidget(distance);

                layout->addWidget(new QLabel("Amplitude:"));
                QSpinBox *amplitude = new QSpinBox;
                amplitude->setRange(INT_MIN, INT_MAX);
                if(parameters_str.length()>=6) amplitude->setValue(parameters_str.at(5).toInt());
                layout->addWidget(amplitude);

                layout->addWidget(new QLabel("Wave count:"));
                QSpinBox *wavecount = new QSpinBox;
                wavecount->setRange(INT_MIN, INT_MAX);
                if(parameters_str.length()>=7) wavecount->setValue(parameters_str.at(6).toInt());
                layout->addWidget(wavecount);

                /*QPushButton *pb_apply = new QPushButton;
                pb_apply->setText("Apply");
                layout->addWidget(pb_apply);*/
            }
        }
        else if(currline.startsWith("MOUTH_ANIM("))
        {
            QStringList par_cs=chopped(currline, 2).split('(');
            if(par_cs.length()>=2)
            {
                QStringList parameters_str=par_cs.at(1).split(',');

                QLabel *label = new QLabel("Mouth animation");
                label->setStyleSheet("font-weight: bold");
                layout->addWidget(label);
                QFrame *separator = new QFrame;
                separator->setFrameShape(QFrame::HLine);
                separator->setFrameShadow(QFrame::Sunken);
                layout->addWidget(new QLabel("Changes the mouth shape."));
                layout->addWidget(separator);

                layout->addWidget(new QLabel("Performer:"));
                QSpinBox *sb_performer = new QSpinBox;
                sb_performer->setRange(0,5);
                if(parameters_str.length()>=1) sb_performer->setValue(parameters_str.at(0).toInt());
                layout->addWidget(sb_performer);

                layout->addWidget(new QLabel("???:"));
                QSpinBox *sb_unknown = new QSpinBox;
                sb_unknown->setRange(INT_MIN,INT_MAX);
                if(parameters_str.length()>=2) sb_unknown->setValue(parameters_str.at(1).toInt());
                layout->addWidget(sb_unknown);

                layout->addWidget(new QLabel("Animation:"));
                QComboBox *animation = new QComboBox;
                if(parameters_str.length()>=3)
                {
                    QString mouth_string="";
                    switch(parameters_str.at(2).toUInt())
                    {
                    case mouth_ft_a:
                        mouth_string="A";
                        break;
                    case mouth_ft_i:
                        mouth_string="I";
                        break;
                    case mouth_ft_u:
                        mouth_string="U";
                        break;
                    case mouth_ft_e:
                        mouth_string="E";
                        break;
                    case mouth_ft_o:
                        mouth_string="O";
                        break;
                    case mouth_ft_m:
                        mouth_string="M";
                        break;
                    default:
                        mouth_string=parameters_str.at(2);
                    }
                    animation->addItem(mouth_string);
                }
                layout->addWidget(animation);

                layout->addWidget(new QLabel("Transition:"));
                QSpinBox *sb_transition = new QSpinBox;
                sb_transition->setRange(0,INT_MAX);
                if(parameters_str.length()>=4) sb_transition->setValue(parameters_str.at(3).toInt());
                layout->addWidget(sb_transition);

                layout->addWidget(new QLabel("???:"));
                QSpinBox *sb_unknown2 = new QSpinBox;
                sb_unknown2->setRange(INT_MIN,INT_MAX);
                if(parameters_str.length()>=5) sb_unknown2->setValue(parameters_str.at(4).toInt());
                layout->addWidget(sb_unknown2);

                /*QPushButton *pb_apply = new QPushButton;
                pb_apply->setText("Apply");
                layout->addWidget(pb_apply);*/
            }
        }
        else if(currline.startsWith("PV_BRANCH_MODE("))
        {
            QStringList par_cs=chopped(currline, 2).split('(');
            if(par_cs.length()>=2)
            {
                QLabel *label = new QLabel("PV branch mode");
                label->setStyleSheet("font-weight: bold");
                layout->addWidget(label);
                QFrame *separator = new QFrame;
                separator->setFrameShape(QFrame::HLine);
                separator->setFrameShadow(QFrame::Sunken);
                layout->addWidget(new QLabel("Changes the current branch."));
                layout->addWidget(separator);

                layout->addWidget(new QLabel("New branch:"));
                QComboBox *branch = new QComboBox;
                branch->addItems({"Default", "Chance time failed", "Chance time success"});
                branch->setCurrentIndex(par_cs.at(1).toInt());
                layout->addWidget(branch);

                layout->addStretch(1);

                /*QPushButton *pb_apply = new QPushButton;
                pb_apply->setText("Apply");
                layout->addWidget(pb_apply);*/
            }
        }
        else
        {
            QStringList par_cs=chopped(currline, 2).split('(');
            if(par_cs.length()>=2)
            {
                QLabel *label = new QLabel(par_cs.at(0));
                label->setStyleSheet("font-weight: bold");
                layout->addWidget(label);
                QFrame *separator = new QFrame;
                separator->setFrameShape(QFrame::HLine);
                separator->setFrameShadow(QFrame::Sunken);
                layout->addWidget(separator);
                QStringList parameters_str=par_cs.at(1).split(',');
                if(parameters_str.length()>0&&!(parameters_str.at(0).isNull()||parameters_str.at(0).isEmpty()))
                {
                    for(int i=0; i<parameters_str.length(); i++)
                    {
                        layout->addWidget(new QLabel("Parameter "+QString::number(i+1)+':'));
                        QSpinBox *spinbox=new QSpinBox;
                        spinbox->setRange(INT_MIN, INT_MAX);
                        spinbox->setValue(parameters_str.at(i).toInt());
                        layout->addWidget(spinbox);
                    }
                    layout->addStretch(1);
                    /*QPushButton *pb_apply = new QPushButton;
                    pb_apply->setText("Apply");
                    layout->addWidget(pb_apply);*/
                }
                else layout->addStretch(1);
            }
        }

        // Update time
        QStringList commandlist;
        commandlist=ui->textEdit->document()->toPlainText().split(';', Qt::SkipEmptyParts).replaceInStrings("\n", "").replaceInStrings(" ", "");
        int timecmd = findTimeOfCommand(*ui->textEdit, line);
        if(timecmd<0) timecmd = 0;
        QString timestr=commandlist.at(timecmd).mid(5, commandlist.at(timecmd).length()-6);
        ui->lineEdit_time->setText(pdtime_string(pdtime_split(timestr.toInt())));
        // Update branch
        ui->label_Branch->setText("B"+QString(findBranchOfCommand(*ui->textEdit, line)));
    }
    QLayout *container = new QGridLayout;
    //container->setMargin(0);
    container->setContentsMargins(0, 0, 0, 0);
    container->addWidget(scrollarea);
    ui->frame_paramEdit->setLayout(container);
}

void MainWindow::on_actionAdd_command_triggered()
{
    DivaScriptOpcode* DSO;
    DSO=formatSelection(dscfmt, ui->textEdit_Log);
    diag_addcommand *diag = new diag_addcommand(this, DSO);
    QString *command = new QString("");
    int *time = new int;
    diag->command=command;
    diag->time=time;
    bool accepted = diag->exec();
    delete diag;
    delete DSO;
    if(accepted&&!command->isEmpty())
    {
        isLoading=true;
        if(command->endsWith(';')) command->chop(1);
        QStringList commandlist;
		commandlist=ui->textEdit->document()->toPlainText().split(';', Qt::SkipEmptyParts).replaceInStrings("\n", "").replaceInStrings(" ", "");
        insertCommand(commandlist, *time, *command);
        ui->textEdit->clear();
		for(int i=0; i<commandlist.length(); i++)
			if(commandlist.at(i).isEmpty())
				ui->textEdit->insertPlainText(commandlist.at(i));
			else
				ui->textEdit->insertPlainText(commandlist.at(i)+";\n");
        isLoading=false;
    }
}

void MainWindow::on_actionEdits_triggered()
{
    QDesktopServices::openUrl(QUrl("https://github.com/nastys/UltraEdit/releases"));
}

void MainWindow::on_actionCut_triggered()
{
    ui->textEdit->cut();
}

void MainWindow::on_actionCopy_triggered()
{
    ui->textEdit->copy();
}

void MainWindow::on_actionPaste_triggered()
{
    ui->textEdit->paste();
}

void MainWindow::on_actionRedo_triggered()
{
    ui->textEdit->redo();
}

void MainWindow::on_actionUndo_triggered()
{
    ui->textEdit->undo();
}

void MainWindow::on_action_Wrap_toggled(bool arg1)
{
    ui->textEdit->setLineWrapMode(arg1 ? QPlainTextEdit::LineWrapMode::WidgetWidth : QPlainTextEdit::LineWrapMode::NoWrap);
#ifdef Q_OS_MACOS
    wrapAction->setChecked(arg1);
#endif
}

void MainWindow::on_action_Dark_mode_toggled(bool arg1)
{
    ui->actionLight_mode->blockSignals(true);
    ui->actionLight_mode->setChecked(false);
    if(arg1)
    {
        this->setStyleSheet("background-color: rgb(46, 52, 54);\nalternate-background-color: rgb(46, 52, 54);\ncolor: rgb(255, 255, 255);\nborder-color: rgb(238, 238, 236);\nborder-top-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:1, stop:0 rgba(255, 255, 255, 255), stop:1 rgba(0, 0, 0, 255));\nborder-right-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:1, stop:0 rgba(255, 255, 255, 255), stop:1 rgba(0, 0, 0, 255));\nborder-bottom-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:1, stop:0 rgba(255, 255, 255, 255), stop:1 rgba(0, 0, 0, 255));\nborder-left-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:1, stop:0 rgba(255, 255, 255, 255), stop:1 rgba(0, 0, 0, 255));\ngridline-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:1, stop:0 rgba(255, 255, 255, 255), stop:1 rgba(0, 0, 0, 255));");
        ui->textEdit->setStyleSheet("background-color: rgb(0, 0, 0);");
    }
    else
    {
        this->setStyleSheet("");
        ui->textEdit->setStyleSheet("");
    }
    ui->actionLight_mode->blockSignals(false);
}

void MainWindow::on_actionLight_mode_toggled(bool arg1)
{
    ui->action_Dark_mode->blockSignals(true);
    ui->action_Dark_mode->setChecked(false);
    if(arg1)
    {
        this->setStyleSheet("background-color: rgb(238, 238, 236);\nalternate-background-color: rgb(255, 255, 255);\ncolor: rgb(0, 0, 0);\nborder-color: rgb(238, 238, 236);\nborder-top-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:1, stop:0 rgba(255, 255, 255, 255), stop:1 rgba(0, 0, 0, 255));\nborder-right-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:1, stop:0 rgba(255, 255, 255, 255), stop:1 rgba(0, 0, 0, 255));\nborder-bottom-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:1, stop:0 rgba(255, 255, 255, 255), stop:1 rgba(0, 0, 0, 255));\nborder-left-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:1, stop:0 rgba(255, 255, 255, 255), stop:1 rgba(0, 0, 0, 255));\ngridline-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:1, stop:0 rgba(255, 255, 255, 255), stop:1 rgba(0, 0, 0, 255));");
        ui->textEdit->setStyleSheet("background-color: rgb(255, 255, 255);\ncolor: rgb(0, 0, 0);");
    }
    else
    {
        this->setStyleSheet("");
        ui->textEdit->setStyleSheet("");
    }
    ui->action_Dark_mode->blockSignals(false);
}

void MainWindow::on_textEdit_modificationChanged(bool arg1)
{
    if(!isLoading) setTitleBarText(arg1);
}

void MainWindow::setTitleBarText(bool modified)
{
    if(isLoading) return;
    if(!preModified&&modified)
    {
        QFileInfo file(currentFilePath);
        QWidget::setWindowTitle(file.fileName()+" *");
        preModified=true;
    }
    else if(!modified)
    {
        QFileInfo file(currentFilePath);
        QWidget::setWindowTitle(file.fileName());
    }
}

void MainWindow::on_actionSet_PV_slot_triggered()
{
    diag_pvslot diag;
    diag.setPvSlotPtr(&pvslot);
    diag.exec();
    setPvSlot();
}

void MainWindow::setPvSlot()
{
    ui->actionSet_PV_slot->setText("PV "+pvslot_leading);
}

void MainWindow::on_actionFind_and_replace_triggered()
{
    diag_find2 * diag = new diag_find2(nullptr, this, ui->textEdit);
    diag->show();
}

void MainWindow::on_textEdit_Log_textChanged()
{
    ui->tabWidget->setCurrentWidget(ui->tab_log);
    errorState=true;
}

void MainWindow::on_pushButton_List_clicked()
{
    on_actionFind_and_replace_triggered();
}

void MainWindow::on_actionTime_triggered()
{
    diag_time* diag = new diag_time;
    diag->show();
}

void MainWindow::on_actionExport_L_RC_triggered()
{
    QString plaindoc = ui->plainTextEdit_db->document()->toPlainText();
    if(!plaindoc.contains(".lyric.")) // TODO change this
    {
        ui->textEdit_Log->append("E: No lyrics in pv_db.\n");
        return;
    }
    QString filepath = QFileDialog::getSaveFileName(this, "Export LRC", "", "Lyrics (*.lrc);;Text file (*.txt);;Binary file (*)");
    if(filepath.isEmpty()||filepath.isNull()) return;
    QFile file(filepath);
    file.open(QIODevice::ReadWrite);
    file.resize(0);
    if(file.error())
    {
        ui->textEdit_Log->append("E: "+file.errorString()+"\n");
        return;
    }
    QStringList commands = ui->textEdit->document()->toPlainText().split('\n');
    QStringList dblist = plaindoc.split('\n');
    for(int i=0; i<commands.length(); i++)
    {
        QString line = commands.at(i).simplified();
        if(line.startsWith("LYRIC("))
        {
            QStringList paramSplit = line.split(')');
            if(paramSplit.length()>0)
            {
                QStringList paramSplit2 = paramSplit.at(0).mid(6).split(',');
                const QString lyrparam = paramSplit2.at(0);
                QString pv = "pv_"+pvslot_leading+".lyric.";
                QString l_num = leading(lyrparam.toInt(), 3, 10);
                for(int j=0; j<dblist.length(); j++)
                {
                    if(dblist.at(j).startsWith(pv+l_num+"="))
                    {
                        QStringList lyriclinesplit = dblist.at(j).split('=');
                        if(lyriclinesplit.length()>=2)
                        {
                            QString lyricline = lyriclinesplit.at(1);
                            int linetime = findTimeOfCommand(commands, i);
                            if(commands.length()>linetime){
                                QStringList splitlinetime = commands.at(linetime).split(')');
                                if(splitlinetime.length()>0)
                                {
                                    pdsplittime splittime = pdtime_split(splitlinetime.at(0).mid(5).toInt());
                                    QString lrcline = "["+leading((splittime.hours*60)+splittime.minutes, 2, 10).right(2)+":"+leading(splittime.seconds, 2, 10).right(2)+"."+leading(splittime.frac, 2, 10).left(2)+"]"+lyricline+"\n";
                                    file.write(lrcline.toUtf8());
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

void MainWindow::loadPvDbEntry()
{
    // TODO dlcs, etc
    // TODO more than just lyrics

    ui->plainTextEdit_db->clear();
    const QStringList databases({"mdata_pv_db.txt", "pv_db.txt"});
    for(QString database : databases)
    {
        QFile file(database);
        if(!file.open(QIODevice::ReadOnly)) continue;
        QStringList dblines;
        while(!file.atEnd())
        {
            QString line = file.readLine().simplified();
            if(!line.startsWith("pv_"+pvslot_leading+".lyric.")) continue;
            ui->plainTextEdit_db->appendPlainText(line);
        }

        if(!ui->plainTextEdit_db->document()->isEmpty()) break;
    }
}

void MainWindow::on_actionReload_data_base_entry_triggered()
{
    loadPvDbEntry();
}

void MainWindow::on_action_Bulk_check_triggered()
{
    if(QMessageBox::warning(this, "Bulk check", "The current file will be closed. Any unsaved changes will be lost.\n\nContinue?", QMessageBox::Yes|QMessageBox::No)!=QMessageBox::Yes) return;

    QString script_folder = QFileDialog::getExistingDirectory(this, "Select script folder", QString(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if(script_folder.isEmpty()) return;
    //int old_dscfmt = dscfmt;
    //int old_dscver = dscver;

    bool ok;
    formatSelector(dscfmt, dscver, ok);
    if(!ok) return;

    QDirIterator it(script_folder);
    while(it.hasNext())
    {
        QString next = it.next();
        if(next.endsWith(".dsc"))
        {
            ui->textEdit_Log->append("-------------------------------------\n"+next+"\n-------------------------------------\n");
            openDSC(next, true);
            repaint();
        }
    }

    //dscfmt = old_dscfmt;
    //dscver = old_dscver;
}

void MainWindow::on_actionClea_r_log_triggered()
{
    ui->textEdit_Log->clear();
}

void MainWindow::on_actionRemove_command_triggered()
{

}

