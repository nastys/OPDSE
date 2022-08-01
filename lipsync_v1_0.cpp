#include "lipsync.h"
#include "command.h"
#include <QProcess>
#include <QVector>
#include <QTextDocumentFragment>

using namespace mouthphonemes_v1_0;

void LipSync_v1_0::lyricToPhoneme(QString &inLyric, QString &outPhonemes, QString &voice)
{
    QProcess process;
    process.setProgram("espeak");
    process.setArguments({"-q", "-x", "-v", voice, "\""+inLyric+"\""});
    process.start();
    process.waitForFinished();
    outPhonemes=process.readAllStandardOutput().simplified();
}

char LipSync_v1_0::phonemeToMouthId(QChar phoneme, lipsync_settings &settings)
{
    if(phoneme=='a'||phoneme=='A'||(phoneme=='@'&&settings.language!=lipsync_language::LATINSCRIPT)) return mouth_ft_a;
    if(phoneme=='i'||phoneme=='I') return mouth_ft_i;
    if(phoneme=='u'||phoneme=='U') return mouth_ft_u;
    if(phoneme=='e'||phoneme=='E'||(phoneme=='3'&&settings.language!=lipsync_language::LATINSCRIPT)) return mouth_ft_e;
    if(phoneme=='o'||phoneme=='O'||(phoneme=='0'&&settings.language!=lipsync_language::LATINSCRIPT)) return mouth_ft_o;
    if((phoneme=='b'&&settings.close_mouth_b)||(phoneme=='p'&&settings.close_mouth_p)||(phoneme=='n'&&settings.close_mouth_n)||(phoneme=='m'&&settings.close_mouth_m)) return mouth_ft_m;
    return -1;
}

void LipSync_v1_0::fillPhonemeList(QVector<lyric> &inLyrics, QVector<phoneme> &outPhonemes, lipsync_settings &settings)
{
    for(int i=0; i<inLyrics.length(); i++)
    {
        QString lyric=inLyrics.at(i).lyric;
        QString phonemes;
        if(settings.language==lipsync_language::LATINSCRIPT) phonemes=lyric;
        else
        {
            QString voice;
            if(settings.language==lipsync_language::OTHER) voice=settings.espeak_voice;
            else if(settings.language==lipsync_language::ENGLISH_US) voice="en-us";
            else if(settings.language==lipsync_language::ENGLISH_RP) voice="en-uk-rp";
            else if(settings.language==lipsync_language::ITALIAN) voice="it";
            else if(settings.language==lipsync_language::SPANISH) voice="es";
            else if(settings.language==lipsync_language::SPANISH_LA) voice="es-la";
            else if(settings.language==lipsync_language::PORTUGUESE) voice="pt-pt";
            else if(settings.language==lipsync_language::PORTUGUESE_BR) voice="pt-br";
            else if(settings.language==lipsync_language::FRENCH) voice="fr-fr";
            else if(settings.language==lipsync_language::FRENCH_BE) voice="fr-be";
            else if(settings.language==lipsync_language::GERMAN) voice="de";
            lyricToPhoneme(lyric, phonemes, voice);
        }
        if(!phonemes.length()) continue;
        QByteArray phoneme_bytes;
        for(int j=0; j<phonemes.length(); j++)
        {
            char id=phonemeToMouthId(phonemes.at(j), settings);
            if(id!=-1) phoneme_bytes.append(id);
        }
        if(phoneme_bytes.length()&&settings.close_mouth_end) phoneme_bytes.append(mouth_ft_m);
        int lyriclength=inLyrics.at(i).endtime-inLyrics.at(i).starttime;
        int phonemelength=lyriclength/(phoneme_bytes.length())+1;
        for(int k=0; k<phoneme_bytes.length(); k++)
        {
            phoneme pstruct;
            pstruct.mouthid=phoneme_bytes.at(k);
            pstruct.time=inLyrics.at(i).starttime+(phonemelength*k);
            outPhonemes.append(pstruct);
        }
    }
}

void LipSync_v1_0::getLyrics(QFile &vtt, QVector<lyric> &lyrics, lipsync_settings &settings)
{
    for(int v=0; vtt.atEnd()==0; v++)
    {
        QString line=vtt.readLine();
        if(line.contains("-->"))
        {
            QString lyricline=QTextDocumentFragment::fromHtml(vtt.readLine()).toPlainText().simplified();
            v++;

            QStringList linesplitarrow=line.split("-->");
            QString stime=linesplitarrow.at(0).simplified();
            QString etime=linesplitarrow.at(1).simplified().split(' ').at(0).simplified();

            lyric lyric;
            lyric.lyric=lyricline;
            lyric.starttime=vttTimeToDivaTime(stime)+settings.offset_before;
            lyric.endtime=vttTimeToDivaTime(etime)+settings.offset_after;
            lyrics.append(lyric);
        }
    }
}

void LipSync_v1_0::lipsyncFromVtt(QPlainTextEdit *dscpte, QFile &vtt, lipsync_settings &settings)
{
    QStringList lyriccommandlist=dscpte->document()->toPlainText().split(';', Qt::SkipEmptyParts).replaceInStrings("\n", "").replaceInStrings(" ", "").filter("LYRIC(");

    QVector<lyric> lyrics;
    getLyrics(vtt, lyrics, settings);

    QVector<phoneme> phonemes;
    fillPhonemeList(lyrics, phonemes, settings);

    QStringList commandlist;
    commandlist=dscpte->document()->toPlainText().split(';', Qt::SkipEmptyParts).replaceInStrings("\n", "").replaceInStrings(" ", "");

    if(settings.delete_existing)
    {
        for(int i=0; i<commandlist.length(); i++)
        {
            if(commandlist.at(i).simplified().startsWith("MOUTH_ANIM("+QString::number(settings.performer)+','))
                commandlist.removeAt(i);
        }
    }

    for(int i=0; i<phonemes.length(); i++)
        if(phonemes.at(i).mouthid!=-1)
            insertCommand(commandlist, phonemes.at(i).time, "MOUTH_ANIM("+QString::number(settings.performer)+",0,"+QString::number(phonemes.at(i).mouthid)+",100,-1)");

    dscpte->document()->clear();
    for(int i=0; i<commandlist.length(); i++)
        if(commandlist.at(i).isEmpty())
            dscpte->insertPlainText(commandlist.at(i));
        else
			dscpte->insertPlainText(commandlist.at(i)+";\n");
}
