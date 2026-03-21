#include "barcode_step.h"
#include <QDebug>

StepBarcodeRecognition::StepBarcodeRecognition(const BarcodeConfig* config)
    : cfg_(config)
{
    // 创建条码模型
    try
    {
        // 使用默认构造函数，然后调用CreateBarCodeModel方法
        barcodeModel_.CreateBarCodeModel(HTuple(), HTuple());
        qDebug() << "[Barcode] 一维条码模型创建成功";
    }
    catch (const HException& ex)
    {
        qDebug() << "[Barcode] 一维条码模型创建异常:" << ex.ErrorMessage().Text();
    }
    
    // 创建二维数据码模型
    try
    {
        // 创建QR Code模型
        dataCodeModel_.CreateDataCode2dModel("QR Code", HTuple(), HTuple());
        qDebug() << "[Barcode] 二维数据码模型创建成功";
    }
    catch (const HException& ex)
    {
        qDebug() << "[Barcode] 二维数据码模型创建异常:" << ex.ErrorMessage().Text();
    }
}

StepBarcodeRecognition::~StepBarcodeRecognition()
{
    // HBarCode和HDataCode2D会自动清理资源
}

bool StepBarcodeRecognition::is2DCode(const QString& codeType) const
{
    // 判断是否为二维数据码
    return (codeType == "QR Code" || codeType == "qrcode" || codeType == "QR" || 
            codeType == "QR二维码" || codeType == "Data Matrix" || codeType == "DataMatrix" || 
            codeType == "data_matrix" || codeType == "PDF417" || codeType == "Aztec");
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
        // 转换为Halcon图像
        HImage hImg;
        if (input.channels() == 3)
        {
            cv::Mat gray;
            cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
            hImg = HImage("byte", (Hlong)gray.cols, (Hlong)gray.rows, 
                         (void*)gray.data);
        }
        else
        {
            hImg = HImage("byte", (Hlong)input.cols, (Hlong)input.rows, 
                         (void*)input.data);
        }
        
        // 构建条码类型字符串
        QString codeTypeStr;
        if (cfg_->codeTypes.isEmpty())
        {
            codeTypeStr = "auto";
        }
        else
        {
            codeTypeStr = cfg_->codeTypes.first();
        }
        
        // 判断是二维数据码还是一维条码
        bool use2DCode = is2DCode(codeTypeStr);
        
        // 执行条码识别
        int numFound = 0;
        ctx.barcodeResults.clear();
        
        if (use2DCode)
        {
            // 使用二维数据码模型
            qDebug() << "[Barcode] 使用二维数据码模型识别";
            
            HTuple resultHandles, decodedStrings;
            HXLDCont symbolXLDs = dataCodeModel_.FindDataCode2d(hImg, HTuple(), HTuple(), 
                                                               &resultHandles, &decodedStrings);
            
            numFound = decodedStrings.Length();
            
            for (int i = 0; i < numFound; i++)
            {
                BarcodeResult result;
                
                // 获取解码字符串
                try
                {
                    HTuple strTuple = decodedStrings[i];
                    result.data = QString(strTuple.S().Text());
                }
                catch (...)
                {
                    result.data = "Decode Error";
                }
                
                // 获取条码类型
                if (codeTypeStr == "auto" || codeTypeStr == "自动检测")
                {
                    result.type = "QR Code";
                }
                else
                {
                    result.type = codeTypeStr;
                }
                
                // 简化的位置信息
                result.location = QRectF(input.cols * 0.25, input.rows * 0.25, 
                                        input.cols * 0.5, input.rows * 0.5);
                
                result.quality = 100.0;
                
                ctx.barcodeResults.append(result);
                
                qDebug() << "[Barcode] 识别到:" << result.type 
                         << "数据:" << result.data;
            }
        }
        else
        {
            // 使用一维条码模型
            qDebug() << "[Barcode] 使用一维条码模型识别";
            
            HTuple codeType;
            if (codeTypeStr == "auto" || codeTypeStr == "自动检测")
            {
                codeType = "auto";
            }
            else if (codeTypeStr == "EAN-13" || codeTypeStr == "EAN13")
            {
                codeType = "EAN-13";
            }
            else if (codeTypeStr == "EAN-8" || codeTypeStr == "EAN8")
            {
                codeType = "EAN-8";
            }
            else if (codeTypeStr == "Code 128" || codeTypeStr == "Code128")
            {
                codeType = "Code 128";
            }
            else if (codeTypeStr == "Code 39" || codeTypeStr == "Code39")
            {
                codeType = "Code 39";
            }
            else
            {
                codeType = codeTypeStr.toUtf8().constData();
            }
            
            HTuple decodedStrings;
            HRegion symbolRegions = barcodeModel_.FindBarCode(hImg, codeType, &decodedStrings);
            
            numFound = decodedStrings.Length();
            
            for (int i = 0; i < numFound; i++)
            {
                BarcodeResult result;
                
                // 获取解码字符串
                try
                {
                    HTuple strTuple = decodedStrings[i];
                    result.data = QString(strTuple.S().Text());
                }
                catch (...)
                {
                    try
                    {
                        result.data = QString::number(decodedStrings[i].D());
                    }
                    catch (...)
                    {
                        result.data = "Decode Error";
                    }
                }
                
                // 获取条码类型
                if (codeTypeStr == "auto" || codeTypeStr == "自动检测")
                {
                    result.type = "Auto";
                }
                else
                {
                    result.type = codeTypeStr;
                }
                
                // 简化的位置信息
                result.location = QRectF(input.cols * 0.25, input.rows * 0.25, 
                                        input.cols * 0.5, input.rows * 0.5);
                
                result.quality = 100.0;
                
                ctx.barcodeResults.append(result);
                
                qDebug() << "[Barcode] 识别到:" << result.type 
                         << "数据:" << result.data;
            }
        }
        
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
    catch (const HException& ex)
    {
        QString error = QString("条码识别错误: %1").arg(ex.ErrorMessage().Text());
        ctx.barcodeStatus = error;
        qDebug() << "[Barcode]" << error;
    }
}
