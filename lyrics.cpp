#include "lyrics.h"
#include "command.h"
#include <QFile>
#include <QTextDocumentFragment>

int getLyricLine(QStringList &lyrics, QString &lyric)
{
    for(int i=0; i<lyrics.length(); i++) if(lyrics.at(i)==lyric) return i;
    lyrics.append(lyric);
    return lyrics.size()-1;
}

void fromVtt(QFile &file, QPlainTextEdit *textedit, QPlainTextEdit *pvdb, int pvnum)
{
    QStringList commandlist;
    commandlist=textedit->document()->toPlainText().split(';', Qt::SkipEmptyParts).replaceInStrings("\n", "").replaceInStrings(" ", "");
    for(int i=0; i<commandlist.length(); i++)
    {
        if(commandlist.at(i).simplified().startsWith("LYRIC("))
            commandlist.removeAt(i);
    }

    insertCommand(commandlist, 0, "LYRIC(0, -1)");
    QStringList lyricpvdb;
    int maxinserted=0;
    for(unsigned long i=1; file.atEnd()==0; i++)
    {
        QString line=file.readLine();
        if(line.contains("-->"))
        {
            QString lyricline=QTextDocumentFragment::fromHtml(file.readLine()).toPlainText().simplified();
            i++;

            QStringList linesplitarrow=line.split("-->");
            QString stime=linesplitarrow.at(0).simplified();
            QString etime=linesplitarrow.at(1).simplified().split(' ').at(0).simplified();

            int start_time=vttTimeToDivaTime(stime);
            int end_time=vttTimeToDivaTime(etime);

            int lyricn=lyricpvdb.indexOf(lyricline);
            if(lyricn==-1)
            {
                    lyricpvdb.append(lyricline);
                    lyricn=lyricpvdb.length();
            }
            else lyricn++;

            removeCommand(commandlist, start_time, "LYRIC(");
            bool inserted=insertCommand(commandlist, start_time, "LYRIC("+QString::number(lyricn)+", -1)");
            insertCommand(commandlist, end_time, "LYRIC(0, -1)");

            if(inserted&&lyricn>maxinserted)
                maxinserted=lyricn;
        }
    }

    pvdb->clear();
    for(int h=0; h<maxinserted; h++) pvdb->appendPlainText("pv_"+QString("%1.lyric.%2="+lyricpvdb.at(h)).arg(pvnum, 3, 10, QChar('0')).arg(h+1, 3, 10, QChar('0')));
    textedit->document()->clear();
    for(int i=0; i<commandlist.length(); i++)
        if(commandlist.at(i).isEmpty())
            textedit->insertPlainText(commandlist.at(i));
        else
			textedit->insertPlainText(commandlist.at(i)+";\n");
}
