#include "barcode_step.h"
#include <QDebug>
#include <opencv2/imgproc.hpp>

StepBarcodeRecognition::StepBarcodeRecognition(const BarcodeConfig* config)
    : cfg_(config)
{
    // 配置ZXing支持的条码格式
    QStringList formats;
    
    if (cfg_ && !cfg_->codeTypes.isEmpty())
    {
        // 使用配置的条码类型
        for (const QString& type : cfg_->codeTypes)
        {
            if (type == "QR Code" || type == "qrcode")
            {
                formats << "QRCode";
            }
            else if (type == "Data Matrix" || type == "datamatrix")
            {
                formats << "DataMatrix";
            }
            else if (type == "EAN-13" || type == "EAN13")
            {
                formats << "EAN-13";
            }
            else if (type == "auto" || type == "自动检测")
            {
                // 自动检测模式，支持所有格式
                formats << "QRCode" << "DataMatrix" << "EAN-13";
                break;
            }
        }
    }
    
    // 如果没有指定格式或格式为空，使用默认格式
    if (formats.isEmpty())
    {
        formats << "QRCode" << "DataMatrix" << "EAN-13";
    }
    
    reader_.setFormats(formats);
    reader_.setTryRotate(true);
    reader_.setTryInvert(true);
    reader_.setTryDownscale(true);
    
    qDebug() << "[Barcode] ZXing条码读取器初始化完成，支持格式:" << formats;
}

StepBarcodeRecognition::~StepBarcodeRecognition()
{
    // ZXing读取器会自动清理资源
}

bool StepBarcodeRecognition::is2DCode(const QString& codeType) const
{
    // 判断是否为二维数据码
    return (codeType == "QR Code" || codeType == "Data Matrix" || 
            codeType == "QRCode" || codeType == "DataMatrix");
}

QString StepBarcodeRecognition::convertBarcodeType(const QString& zxingType) const
{
    // 将ZXing条码类型转换为用户友好的名称
    if (zxingType == "QRCode") return "QR Code";
    if (zxingType == "DataMatrix") return "Data Matrix";
    if (zxingType == "EAN13") return "EAN-13";
    if (zxingType == "EAN8") return "EAN-8";
    if (zxingType == "UPCA") return "UPC-A";
    if (zxingType == "UPCE") return "UPC-E";
    if (zxingType == "Code128") return "Code 128";
    if (zxingType == "Code39") return "Code 39";
    return zxingType;
}

void StepBarcodeRecognition::run(PipelineContext& ctx)
{
    if (!cfg_ || !cfg_->enableBarcode)
    {
        return;
    }
    
    // 获取输入图像
    cv::Mat input;
    if (!ctx.processed.empty())
    {
        input = ctx.processed;
    }
    else if (!ctx.enhanced.empty())
    {
        input = ctx.enhanced;
    }
    else
    {
        input = ctx.srcBgr;
    }
    
    if (input.empty())
    {
        ctx.barcodeStatus = "输入图像为空";
        return;
    }
    
    try
    {
        // 使用ZXing进行条码识别
        ctx.barcodeResults.clear();
        
        QVector<ZXingBarcodeResult> zxingResults = reader_.readBarcodes(input);
        
        // 转换ZXing结果为BarcodeResult
        for (const auto& zxingResult : zxingResults)
        {
            BarcodeResult result;
            result.data = zxingResult.data;
            result.type = convertBarcodeType(zxingResult.type);
            result.location = zxingResult.location;
            result.quality = zxingResult.quality;
            
            ctx.barcodeResults.append(result);
        }
        
        int numFound = ctx.barcodeResults.size();
        
        // 更新状态
        if (numFound > 0)
        {
            ctx.barcodeStatus = QString("成功识别 %1 个条码").arg(numFound);
        }
        else
        {
            ctx.barcodeStatus = "未检测到条码";
        }
        
        qDebug() << "[Barcode]" << ctx.barcodeStatus;
    }
    catch (const std::exception& ex)
    {
        QString error = QString("条码识别错误: %1").arg(ex.what());
        ctx.barcodeStatus = error;
        qDebug() << "[Barcode]" << error;
    }
    catch (...)
    {
        QString error = "条码识别发生未知错误";
        ctx.barcodeStatus = error;
        qDebug() << "[Barcode]" << error;
    }
}
