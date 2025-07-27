#ifndef PROMPTGENERATORS_H
#define PROMPTGENERATORS_H

#include <QString>
#include <QVector>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include "DataStructures.h"

namespace PromptGenerators {
    QString generateToolDiscoveryPrompt(const QString& userQuery, 
                                       const QVector<ToolInfo>& tools);
    
    QString generateToolExecutionPrompt(ConversationContext* context);
    
    QString getSpecialInstructions(const QString& toolName);
}

#endif // PROMPTGENERATORS_H