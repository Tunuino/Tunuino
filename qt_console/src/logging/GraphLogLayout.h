#pragma once
#include "LogData.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <array>
#include <vector>

namespace tunuino {
constexpr int GraphLogTabCount = 12;
constexpr int MaxGraphsPerTab = 4;
struct LogAxisConfig {
    int channel = -1;
    bool autoscale = true;
    double minimum = 0;
    double maximum = 100;
};
struct LogChartConfig { std::array<LogAxisConfig,2> axes; };
struct LogTabConfig {
    QString name;
    bool dots = false;
    std::vector<LogChartConfig> charts;
};
using GraphLogLayout = std::array<LogTabConfig,GraphLogTabCount>;
inline GraphLogLayout defaultGraphLogLayout() {
    GraphLogLayout layout;
    for (int i=0;i<GraphLogTabCount;++i) {
        layout[i].name=QStringLiteral("Log %1").arg(i+1);
        LogChartConfig chart; chart.axes[0].channel=0; chart.axes[1].channel=2;
        layout[i].charts.push_back(chart);
    }
    layout[0].name=QStringLiteral("Basic");
    LogChartConfig temperatures;temperatures.axes[0].channel=3;temperatures.axes[1].channel=4;
    layout[0].charts.push_back(temperatures);
    layout[1].name=QStringLiteral("Fuel");layout[1].charts[0].axes[0].channel=3;
    layout[2].name=QStringLiteral("Temperatures");
    layout[2].charts[0].axes[0].channel=4;layout[2].charts[0].axes[1].channel=5;
    return layout;
}
inline QByteArray encodeGraphLogLayout(const GraphLogLayout &layout) {
    QJsonArray tabs;
    for (const auto &tab:layout) {
        QJsonArray charts;
        for (const auto &chart:tab.charts) {
            QJsonArray axes;
            for (const auto &axis:chart.axes)
                axes.append(QJsonObject{{"channel",axis.channel},{"autoscale",axis.autoscale},
                    {"minimum",axis.minimum},{"maximum",axis.maximum}});
            charts.append(axes);
        }
        tabs.append(QJsonObject{{"name",tab.name},{"dots",tab.dots},{"charts",charts}});
    }
    return QJsonDocument(QJsonObject{{"format","tunuino-graph-layout-v1"},{"tabs",tabs}}).toJson(QJsonDocument::Compact);
}
// Parse atomically: a corrupt saved layout cannot partly replace the current one.
inline bool decodeGraphLogLayout(const QByteArray &bytes,GraphLogLayout &result) {
    if (bytes.size()>65536) return false;
    const auto document=QJsonDocument::fromJson(bytes);
    const auto root=document.object();
    const auto tabs=root.value("tabs").toArray();
    if (root.value("format").toString()!=QLatin1String("tunuino-graph-layout-v1") || tabs.size()!=GraphLogTabCount) return false;
    GraphLogLayout parsed;
    for (int i=0;i<GraphLogTabCount;++i) {
        const auto tab=tabs[i].toObject();
        if (!tab.value("name").isString() || !tab.value("dots").isBool()) return false;
        auto &target=parsed[i];target.name=tab.value("name").toString().trimmed();
        if (target.name.isEmpty() || target.name.size()>48) return false;
        target.dots=tab.value("dots").toBool();
        const auto charts=tab.value("charts").toArray();
        if (charts.isEmpty() || charts.size()>MaxGraphsPerTab) return false;
        for (const auto &chartValue:charts) {
            if (!chartValue.isArray()) return false;
            const auto axes=chartValue.toArray();
            if (axes.size()!=2) return false;
            LogChartConfig chart;
            for (int side=0;side<2;++side) {
                const auto axis=axes[side].toObject();auto &a=chart.axes[side];
                if (!axis.value("channel").isDouble() || !axis.value("autoscale").isBool()
                    || !axis.value("minimum").isDouble() || !axis.value("maximum").isDouble()) return false;
                const double channel=axis.value("channel").toDouble();
                if (channel!=std::floor(channel) || channel< -1 || channel>=LogChannelCount) return false;
                a.channel=int(channel);a.autoscale=axis.value("autoscale").toBool();
                a.minimum=axis.value("minimum").toDouble();a.maximum=axis.value("maximum").toDouble();
                if (!std::isfinite(a.minimum) || !std::isfinite(a.maximum) || a.minimum>=a.maximum) return false;
            }
            if (chart.axes[0].channel>=0 && chart.axes[0].channel==chart.axes[1].channel) return false;
            target.charts.push_back(chart);
        }
    }
    result=std::move(parsed);return true;
}
}
