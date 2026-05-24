#include "MergeTiffProcessor.h"

#include <QDebug>

MergeTiffProcessor::MergeTiffProcessor() { }

QString MergeTiffProcessor::name() const
{
    return "MergeTiff";
}

// void MergeTiffProcessor::process(ImageUtils::ImportResult &result)
// {
//     qDebug() << result.path;
// }
