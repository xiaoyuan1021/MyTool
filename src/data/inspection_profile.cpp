#include "data/inspection_profile.h"
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <opencv2/imgproc.hpp>

// ==================== TemplateEntry 实现 ====================

bool TemplateEntry::load(const QString& baseDir) {
    QString tplPath = baseDir + "/templates/" + templateName + ".tpl";

    QFile file(tplPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    QByteArray data = file.readAll();
    file.close();
    if (data.isEmpty()) return false;

    try {
        cv::FileStorage fs(std::string(data.begin(), data.end()),
            cv::FileStorage::READ | cv::FileStorage::MEMORY);
        if (!fs.isOpened()) return false;

        fs["templateImage"] >> templateImage;
        if (templateImage.empty()) return false;

        fs["matchMethod"] >> params.matchMethod;

        params.polygonPoints.clear();
        cv::FileNode pts = fs["polygonPoints"];
        if (pts.type() == cv::FileNode::SEQ) {
            for (const auto& pt : pts) {
                params.polygonPoints.append(QPointF(pt["x"], pt["y"]));
            }
        }
    } catch (const cv::Exception&) {
        return false;
    }

    templateFilePath = "templates/" + templateName + ".tpl";
    return true;
}

bool TemplateEntry::save(const QString& baseDir) {
    if (templateImage.empty()) return false;

    QDir dir(baseDir);
    if (!dir.exists("templates")) {
        dir.mkpath("templates");
    }

    QString tplPath = baseDir + "/templates/" + templateName + ".tpl";

    try {
        cv::FileStorage fs(".tpl",
            cv::FileStorage::WRITE | cv::FileStorage::MEMORY | cv::FileStorage::FORMAT_YAML);

        fs << "templateImage" << templateImage;
        fs << "matchMethod" << params.matchMethod;

        fs << "polygonPoints" << "[";
        for (const auto& pt : params.polygonPoints) {
            fs << "{:" << "x" << pt.x() << "y" << pt.y() << "}";
        }
        fs << "]";

        std::string content = fs.releaseAndGetString();

        QFile file(tplPath);
        if (!file.open(QIODevice::WriteOnly)) return false;
        file.write(content.data(), content.size());
        file.close();
    } catch (const cv::Exception&) {
        return false;
    }

    templateFilePath = "templates/" + templateName + ".tpl";
    return true;
}

QJsonObject TemplateEntry::toJson() const {
    QJsonObject obj;
    obj["templateName"] = templateName;
    obj["templateFilePath"] = templateFilePath;
    return obj;
}

void TemplateEntry::fromJson(const QJsonObject& obj) {
    templateName = obj["templateName"].toString();
    templateFilePath = obj["templateFilePath"].toString();
}

// ==================== RoiTemplate 实现 ====================

RoiTemplate RoiTemplate::fromRoiConfig(const RoiConfig& roiConfig, const QSize& refImageSize) {
    RoiTemplate rt;
    rt.name = roiConfig.roiName;
    rt.color = roiConfig.color;
    rt.pipelineConfig = roiConfig.pipelineConfig;
    rt.detectionItems = roiConfig.detectionItems;

    if (refImageSize.width() > 0 && refImageSize.height() > 0) {
        rt.relativeRect = QRectF(
            roiConfig.roiRect.x() / refImageSize.width(),
            roiConfig.roiRect.y() / refImageSize.height(),
            roiConfig.roiRect.width() / refImageSize.width(),
            roiConfig.roiRect.height() / refImageSize.height()
        );
    }

    return rt;
}

RoiConfig RoiTemplate::toRoiConfig(const QSize& currentImageSize) const {
    RoiConfig rc;
    rc.roiName = name;
    rc.color = color;
    rc.pipelineConfig = pipelineConfig;
    rc.detectionItems = detectionItems;
    rc.roiId = generateUniqueId("roi");

    rc.roiRect = QRectF(
        relativeRect.x() * currentImageSize.width(),
        relativeRect.y() * currentImageSize.height(),
        relativeRect.width() * currentImageSize.width(),
        relativeRect.height() * currentImageSize.height()
    );

    return rc;
}

QJsonObject RoiTemplate::toJson() const {
    QJsonObject obj;
    obj["name"] = name;
    obj["color"] = color;

    QJsonObject rect;
    rect["x"] = relativeRect.x();
    rect["y"] = relativeRect.y();
    rect["width"] = relativeRect.width();
    rect["height"] = relativeRect.height();
    obj["relativeRect"] = rect;

    QJsonArray detections;
    for (const auto& item : detectionItems) {
        detections.append(item.toJson());
    }
    obj["detectionItems"] = detections;

    obj["pipelineConfig"] = pipelineConfig.toJson();

    return obj;
}

void RoiTemplate::fromJson(const QJsonObject& obj) {
    name = obj["name"].toString();
    color = obj["color"].toString("#FF6B6B");

    QJsonObject rect = obj["relativeRect"].toObject();
    relativeRect = QRectF(
        rect["x"].toDouble(), rect["y"].toDouble(),
        rect["width"].toDouble(), rect["height"].toDouble()
    );

    detectionItems.clear();
    QJsonArray detections = obj["detectionItems"].toArray();
    for (const auto& val : detections) {
        DetectionItem item;
        item.fromJson(val.toObject());
        detectionItems.append(item);
    }

    QJsonObject pipeConfig = obj["pipelineConfig"].toObject();
    if (!pipeConfig.isEmpty()) {
        // 兼容旧格式：将扁平 barcode_* 字段转为嵌套格式
        if (pipeConfig.contains("barcode_enable") && !pipeConfig.contains("barcode")) {
            QJsonObject barcodeObj;
            barcodeObj["enableBarcode"] = pipeConfig["barcode_enable"];
            barcodeObj["codeTypes"] = pipeConfig["barcode_codeTypes"];
            barcodeObj["maxNumSymbols"] = pipeConfig["barcode_maxNum"];
            barcodeObj["returnQuality"] = pipeConfig["barcode_returnQuality"];
            pipeConfig["barcode"] = barcodeObj;
        }
        pipelineConfig.fromJson(pipeConfig);
    }
}

// ==================== InspectionProfile 实现 ====================

void InspectionProfile::addTemplate(const TemplateEntry& entry) {
    // 同名模板替换
    for (int i = 0; i < templates.size(); ++i) {
        if (templates[i].templateName == entry.templateName) {
            templates[i] = entry;
            updatedAt = QDateTime::currentDateTime();
            return;
        }
    }
    templates.append(entry);
    updatedAt = QDateTime::currentDateTime();
}

bool InspectionProfile::removeTemplate(const QString& templateName) {
    for (int i = 0; i < templates.size(); ++i) {
        if (templates[i].templateName == templateName) {
            templates.removeAt(i);
            updatedAt = QDateTime::currentDateTime();
            return true;
        }
    }
    return false;
}

const TemplateEntry* InspectionProfile::findTemplate(const QString& templateName) const {
    for (const auto& t : templates) {
        if (t.templateName == templateName) {
            return &t;
        }
    }
    return nullptr;
}

QStringList InspectionProfile::templateNames() const {
    QStringList names;
    for (const auto& t : templates) {
        names.append(t.templateName);
    }
    return names;
}

QJsonObject InspectionProfile::toJson() const {
    QJsonObject obj;
    obj["profileId"] = profileId;
    obj["profileName"] = profileName;
    obj["description"] = description;
    obj["createdAt"] = createdAt.toString(Qt::ISODate);
    obj["updatedAt"] = updatedAt.toString(Qt::ISODate);
    obj["refImageWidth"] = referenceImageSize.width();
    obj["refImageHeight"] = referenceImageSize.height();

    // ROI模板列表
    QJsonArray roiArray;
    for (const auto& roi : roiTemplates) {
        roiArray.append(roi.toJson());
    }
    obj["roiTemplates"] = roiArray;

    // 模板库元数据（实际文件另存）
    QJsonArray tplArray;
    for (const auto& tpl : templates) {
        tplArray.append(tpl.toJson());
    }
    obj["templates"] = tplArray;

    // 全局Pipeline配置
    obj["globalPipelineConfig"] = globalPipelineConfig.toJson();

    return obj;
}

void InspectionProfile::fromJson(const QJsonObject& obj) {
    profileId = obj["profileId"].toString();
    profileName = obj["profileName"].toString();
    description = obj["description"].toString();
    createdAt = QDateTime::fromString(obj["createdAt"].toString(), Qt::ISODate);
    updatedAt = QDateTime::fromString(obj["updatedAt"].toString(), Qt::ISODate);
    referenceImageSize = QSize(
        obj["refImageWidth"].toInt(0),
        obj["refImageHeight"].toInt(0)
    );

    roiTemplates.clear();
    QJsonArray roiArray = obj["roiTemplates"].toArray();
    for (const auto& val : roiArray) {
        RoiTemplate rt;
        rt.fromJson(val.toObject());
        roiTemplates.append(rt);
    }

    templates.clear();
    QJsonArray tplArray = obj["templates"].toArray();
    for (const auto& val : tplArray) {
        TemplateEntry te;
        te.fromJson(val.toObject());
        templates.append(te);
    }

    globalPipelineConfig.fromJson(obj["globalPipelineConfig"].toObject());
}

bool InspectionProfile::saveToDirectory(const QString& profilesDir) {
    QString dirPath = profilesDir + "/" + profileName;
    QDir dir(dirPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // 保存 profile.json
    QFile file(dirPath + "/profile.json");
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(QJsonDocument(toJson()).toJson());
    file.close();

    // 保存模板文件
    for (auto& tpl : templates) {
        tpl.save(dirPath);
    }

    return true;
}

bool InspectionProfile::loadFromDirectory(const QString& profileDir) {
    QFile file(profileDir + "/profile.json");
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    fromJson(doc.object());

    // 加载模板图像
    for (auto& tpl : templates) {
        tpl.load(profileDir);
    }

    return true;
}

cv::Mat InspectionProfile::generateThumbnail(const cv::Mat& fullImage, int thumbWidth) const {
    if (fullImage.empty()) return cv::Mat();

    cv::Mat thumb;
    double scale = static_cast<double>(thumbWidth) / fullImage.cols;
    cv::resize(fullImage, thumb, cv::Size(), scale, scale, cv::INTER_AREA);
    return thumb;
}