#include "command.h"
#include "pdtime.h"

int getTimeFromTimeCommand(QString command)
{
    return command.split('(').at(1).simplified().remove(')').toInt();
}

int vttTimeToDivaTime(QString &time)
{
    QStringList timesplit=time.split(':');
    QString msecs="00000";
    QString mins="00";
    QString hours="00";
    if(timesplit.length()>2)
    {
        msecs=timesplit.at(2).simplified().remove('.');
        mins=timesplit.at(1).simplified();
        hours=timesplit.at(0).simplified();
    }
    else
    {
        msecs=timesplit.at(1).simplified().remove('.');
        mins=timesplit.at(0).simplified();
    }
    return ((msecs.toInt() + (mins.toInt()*60 + hours.toInt()*3600)*1000)*100);
}

QString hmsTimeFromDivaTime(int divaTime)
{
    return pdtime_string(pdtime_split(divaTime));
}

bool insertBranch(QStringList &commandList, int line, int branch, QString suffix)
{
    if(branch<0) return 1;

    if(findBranchOfCommand(commandList, line)!=branch)
    {
		commandList.insert(line, "PV_BRANCH_MODE("+QString::number(branch)+")"+suffix);
        return 1;
    }

    return 0;
}

bool insertCommand(QStringList &commandList, int time, QString command, int branch, QString suffix)
{
	if (!commandList.contains("TIME(0)"))
	{
		commandList.insert(0, "TIME(0)");
	}

	if (commandList.last() != "")
	{
		commandList.append("");
	}

    for(int i=0; i<commandList.length(); i++)
    {
        const QString currentCommand=commandList.at(i).simplified();
		if(currentCommand.startsWith("TIME("))
		{
			const int thisTime = getTimeFromTimeCommand(currentCommand);

			if (thisTime == time)
			{
				commandList.insert(i+1, command.simplified());
				return insertBranch(commandList, i, branch, suffix);
			}
			if (thisTime > time)
			{
				// add TIME+command before this line
				commandList.insert(i, command.simplified());
				commandList.insert(i, "TIME("+QString::number(time)+")");
				return insertBranch(commandList, i, branch, suffix);
			}
        }
		else if(i==commandList.length()-1)
		{
			int pvEndLine = -1;
			int endLine = -1;
			for (int j=i; j>=0; j--)
			{
				if (commandList.at(j).startsWith("PV_END()"))
				{
					pvEndLine = j;
					break;
				}
				else if (endLine == -1 && commandList.at(j).startsWith("END()"))
					endLine = j;
			}
			int targetEndLine = pvEndLine == -1 ? endLine : pvEndLine;

			if(targetEndLine == -1)
			{
				commandList.append({"TIME("+QString::number(time)+")", command.simplified()});
				return insertBranch(commandList, commandList.length()-1, branch, suffix);
			}
			commandList.insert(targetEndLine, command.simplified());
			commandList.insert(targetEndLine, "TIME("+QString::number(time)+")");
			return insertBranch(commandList, targetEndLine-1, branch, suffix);


			int pvEndTime = findTimeOfCommand(commandList, "PV_END()"+suffix);
			if(pvEndTime == -1) pvEndTime = findTimeOfCommand(commandList, "END()"+suffix);
			if(pvEndTime == -1)
			{
				commandList.append({"TIME("+QString::number(time)+")", command.simplified()});
				return insertBranch(commandList, commandList.length()-1, branch, suffix);
			}
			commandList.insert(pvEndTime+1, command.simplified());
			commandList.insert(pvEndTime+1, "TIME("+QString::number(time)+")");
			return insertBranch(commandList, pvEndTime, branch, suffix);
        }
	}
	return 0;
}

bool removeCommand(QStringList &commandList, int time, QString command)
{
    for(int i=0; i<commandList.length(); i++)
    {
        const QString currentCommand=commandList.at(i).simplified();
        if(currentCommand.startsWith("TIME("))
        {
            const int tfc=getTimeFromTimeCommand(currentCommand);
            if(tfc==time)
            {
                int p=i;
                do
                {
                    p++;
                    if(commandList.at(p).startsWith("TIME("))
                        return 0;
                }
                while(!commandList.at(p).startsWith(command));
                commandList.removeAt(p);
                return 1;
            }
        }
    }
    return 0; // not removed
}

int findTimeOfCommand(QStringList &commandList, QString command)
{
    int lastTime=-1;
    for(int i=0; i<commandList.length(); i++)
    {
        if(commandList.length()<=i) continue;
        const QString currentCommand=commandList.at(i).simplified();
        if(currentCommand.startsWith("TIME("))
            lastTime = i;
        if(currentCommand.startsWith(command))
            return lastTime;
    }

    return -1;
}

int findTimeOfCommand(QPlainTextEdit &qte, int line)
{
    QStringList docList = qte.document()->toPlainText().split('\n'/*, QString::SkipEmptyParts*/);
    return findTimeOfCommand(docList, line);
}

int findTimeOfCommand(QStringList &commandList, int line)
{
    int lastTime=-1;
    for(int i=0; i<commandList.length(); i++)
    {
        const QString currentCommand=commandList.at(i).simplified();
        if(currentCommand.startsWith("TIME("))
            lastTime = i;
        if(line==i)
            return lastTime;
    }

    return -1;
}

char findBranchOfCommand(QPlainTextEdit &qte, int line)
{
    int lastBranch=0;
    QStringList docList = qte.document()->toPlainText().split('\n'/*, QString::SkipEmptyParts*/);
    for(int i=0; i<qte.document()->lineCount(); i++)
    {
        if(docList.length()<=i) continue;
        const QString currentCommand=docList.at(i).simplified();
        if(currentCommand.startsWith("PV_BRANCH_MODE("))
            lastBranch = currentCommand.split('(').at(1).split(')').at(0).toInt();
        if(line == i)
            switch(lastBranch)
            {
                case 1:
                    return 'F';
                case 2:
                    return 'S';
                default:
                    return 'D';
            }
    }

    return 'D';
}

int findBranchOfCommand(QStringList &commandList, int line)
{
    int lastBranch=-1;
    for(int i=0; i<commandList.length(); i++)
    {
        const QString currentCommand=commandList.at(i).simplified();
        if(currentCommand.startsWith("PV_BRANCH_MODE("))
            lastBranch = i;
		if(i==line && lastBranch != -1)
        {
            QStringList split = commandList.at(lastBranch).simplified().split(')');
            if(split.length()>0)
            {
                return split.at(0).mid(15).toInt();
            }
            return -1;
        }
    }

    return -1;
}
