#pragma once
#include <QString>
#include <QVector>
#include <QPointF>
namespace tunuino {
struct ConnectorPin { int number; QPointF point; QString signal, description, type; };
}
