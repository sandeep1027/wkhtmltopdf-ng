#ifndef WKHTMLTOPDF_NG_ARG_PARSER_H
#define WKHTMLTOPDF_NG_ARG_PARSER_H

#include "converter/Settings.h"

#include <QList>
#include <QStringList>

struct ParsedArguments {
    GlobalSettings global;
    ObjectSettings object;
    QList<ObjectSettings> objects;
    QStringList inputs;
    QString input;
    QString output;
    bool help = false;
    bool version = false;
    QString error;
};

class ArgParser {
public:
    static ParsedArguments parse(const QStringList& arguments);
    static QString usage();
};

#endif
