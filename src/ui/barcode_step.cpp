#include "barcode_step.h"
#include <QDebug>

StepBarcodeRecognition::StepBarcodeRecognition(const BarcodeConfig* config)
    : cfg_(config)
{
    // 创建条码模型
    try
    {
        // 使用正确的参数初始化方式创建条码模型
        // 根据HALCON文档，CreateBarCodeModel需要参数名称和值的元组
        HTuple paramNames, paramValues;
        
        // 设置基础参数
        paramNames[0] = "check_char";
        paramValues[0] = "absent";
        
        paramNames[1] = "element_size_min";
        paramValues[1] = 1.0;
        
        paramNames[2] = "element_size_max";
        paramValues[2] = 100.0;
        
        paramNames[3] = "orientation";
        paramValues[3] = 0;
        
        paramNames[4] = "orientation_tol";
        paramValues[4] = 45;
        
        paramNames[5] = "meas_thresh";
        paramValues[5] = 0.2;
        
        paramNames[6] = "num_scanlines";
        paramValues[6] = 16;
        
        paramNames[7] = "min_identical_scanlines";
        paramValues[7] = 2;
        
        // 创建条码模型
        barcodeModel_.CreateBarCodeModel(paramNames, paramValues);
        
        // 额外设置参数（使用SetBarCodeParam）
        // 注意：small_elements_robustness应该使用数值而不是字符串
        barcodeModel_.SetBarCodeParam("small_elements_robustness", 1);
        
        // quiet_zone参数使用标准值
        barcodeModel_.SetBarCodeParam("quiet_zone", "true");
        
        qDebug() << "[Barcode] 一维条码模型创建成功";
    }
    catch (const HException& ex)
    {
        //qDebug() << "[Barcode] 一维条码模型创建异常:" << ex.ErrorMessage().Text();
    }
    
    // 创建二维数据码模型
    try
    {
        // 创建QR Code模型
        dataCodeModel_.CreateDataCode2dModel("QR Code", HTuple(), HTuple());
        //qDebug() << "[Barcode] 二维数据码模型创建成功";
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
    return (codeType == "QR Code" || codeType == "Data Matrix");
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
        // 转换为灰度图像并进行预处理
        HImage hImg;
        cv::Mat gray;
        
        if (input.channels() == 3)
        {
            cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
        }
        else
        {
            gray = input.clone();
        }
        
        // 直接使用灰度图像，不做额外预处理
        // HALCON的条码识别算法内部有优化的图像处理机制
        //qDebug() << "[Barcode] 图像预处理完成 - 尺寸:" << gray.cols << "x" << gray.rows;
        
        hImg = HImage("byte", (Hlong)gray.cols, (Hlong)gray.rows, 
                     (void*)gray.data);
        
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
        
        // 修复：将"条形码"映射为"auto"
        if (codeTypeStr == "条形码")
        {
            codeTypeStr = "auto";
        }
        
        // 判断是二维数据码还是一维条码
        bool use2DCode = is2DCode(codeTypeStr);
        bool isAutoDetect = (codeTypeStr == "auto" || codeTypeStr == "自动检测");
        
        // 执行条码识别
        int numFound = 0;
        ctx.barcodeResults.clear();
        
        //qDebug() << "[Barcode] 开始识别 - 条码类型:" << codeTypeStr << "是否2D:" << use2DCode;
        
        // 自动检测模式：先尝试二维识别，再尝试一维识别
        if (isAutoDetect)
        {
            qDebug() << "[Barcode] 使用自动检测模式，先尝试QR Code识别";
            
            // 先尝试二维数据码识别（QR Code）
            try
            {
                HTuple resultHandles, decodedStrings;
                HXLDCont symbolXLDs = dataCodeModel_.FindDataCode2d(hImg, HTuple(), HTuple(), 
                                                                   &resultHandles, &decodedStrings);
                
                int num2D = decodedStrings.Length();
                
                for (int i = 0; i < num2D; i++)
                {
                    BarcodeResult result;
                    
                    try
                    {
                        HTuple strTuple = decodedStrings[i];
                        result.data = QString(strTuple.S().Text());
                    }
                    catch (...)
                    {
                        result.data = "Decode Error";
                    }
                    
                    result.type = "QR Code";
                    result.location = QRectF(input.cols * 0.25, input.rows * 0.25, 
                                            input.cols * 0.5, input.rows * 0.5);
                    result.quality = 100.0;
                    
                    ctx.barcodeResults.append(result);
                    numFound++;
                    
                    qDebug() << "[Barcode] 自动检测识别到QR Code:" << result.data;
                }
            }
            catch (const HException& ex)
            {
                qDebug() << "[Barcode] QR Code识别异常:" << ex.ErrorMessage().Text();
            }
            
            // 如果没有识别到QR Code，再尝试一维条码识别
            if (numFound == 0)
            {
                qDebug() << "[Barcode] 未识别到QR Code，尝试一维条码识别";
                
                try
                {
                    HTuple codeType;
                    codeType[0] = "EAN-13";
                    codeType[1] = "EAN-8";
                    codeType[2] = "UPC-A";
                    codeType[3] = "UPC-E";
                    codeType[4] = "Code 128";
                    codeType[5] = "Code 39";
                    codeType[6] = "2/5 Interleaved";
                    codeType[7] = "Codabar";
                    
                    HTuple decodedStrings;
                    HRegion symbolRegions = barcodeModel_.FindBarCode(hImg, codeType, &decodedStrings);
                    
                    int num1D = decodedStrings.Length();
                    
                    for (int i = 0; i < num1D; i++)
                    {
                        BarcodeResult result;
                        
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
                        
                        // 获取实际识别的条码类型
                        try
                        {
                            HTuple decodedType = barcodeModel_.GetBarCodeResult("all", "decoded_types");
                            if (decodedType.Length() > i)
                            {
                                result.type = QString(decodedType[i].S().Text());
                            }
                            else
                            {
                                result.type = "1D Barcode";
                            }
                        }
                        catch (...)
                        {
                            result.type = "1D Barcode";
                        }
                        
                        // 获取位置信息
                        try
                        {
                            if (i < symbolRegions.CountObj())
                            {
                                HTuple row1, col1, row2, col2;
                                symbolRegions.SelectObj(i + 1).SmallestRectangle1(&row1, &col1, &row2, &col2);
                                double x = col1.D();
                                double y = row1.D();
                                double w = col2.D() - col1.D();
                                double h = row2.D() - row1.D();
                                result.location = QRectF(x, y, w, h);
                            }
                            else
                            {
                                result.location = QRectF(input.cols * 0.25, input.rows * 0.25, 
                                                        input.cols * 0.5, input.rows * 0.5);
                            }
                        }
                        catch (...)
                        {
                            result.location = QRectF(input.cols * 0.25, input.rows * 0.25, 
                                                    input.cols * 0.5, input.rows * 0.5);
                        }
                        
                        result.quality = -1.0;
                        
                        ctx.barcodeResults.append(result);
                        numFound++;
                        
                        qDebug() << "[Barcode] 自动检测识别到一维条码:" << result.type << "数据:" << result.data;
                    }
                }
                catch (const HException& ex)
                {
                    qDebug() << "[Barcode] 一维条码识别异常:" << ex.ErrorMessage().Text();
                }
            }
        }
        else if (use2DCode)
        {
            // 使用二维数据码模型
            //qDebug() << "[Barcode] 使用二维数据码模型识别";
            
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
            if (codeTypeStr == "auto" || codeTypeStr == "自动检测" || codeTypeStr == "条形码")
            {
                // 使用常见的一维条码类型列表，提高识别效率
                // 根据 Halcon 文档，指定具体的条码类型列表比 "auto" 更可靠
                codeType[0] = "EAN-13";
                codeType[1] = "EAN-8";
                codeType[2] = "UPC-A";
                codeType[3] = "UPC-E";
                codeType[4] = "Code 128";
                codeType[5] = "Code 39";
                codeType[6] = "2/5 Interleaved";
                codeType[7] = "Codabar";
                qDebug() << "[Barcode] 使用自动检测模式，支持: EAN-13, EAN-8, UPC-A, UPC-E, Code 128, Code 39, 2/5 Interleaved, Codabar";
            }
            else
            {
                codeType = codeTypeStr.toUtf8().constData();
            }
            
            HTuple decodedStrings;
            
            // FindBarCode标准调用方式 - 3参数版本
            HRegion symbolRegions = barcodeModel_.FindBarCode(hImg, codeType, &decodedStrings);
            
            numFound = decodedStrings.Length();
            
            qDebug() << "[Barcode] 识别到" << numFound << "个条码";
            
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
                    // 尝试获取实际识别的条码类型
                    try
                    {
                        // 使用GetBarCodeResult获取解码类型
                        HTuple decodedType = barcodeModel_.GetBarCodeResult("all", "decoded_types");
                        if (decodedType.Length() > i)
                        {
                            result.type = QString(decodedType[i].S().Text());
                        }
                        else
                        {
                            result.type = "Auto";
                        }
                    }
                    catch (...)
                    {
                        result.type = "Auto";
                    }
                }
                else
                {
                    result.type = codeTypeStr;
                }
                
                // 获取位置信息
                try
                {
                    if (i < symbolRegions.CountObj())
                    {
                        HTuple row1, col1, row2, col2;
                        symbolRegions.SelectObj(i + 1).SmallestRectangle1(&row1, &col1, &row2, &col2);
                        double x = col1.D();
                        double y = row1.D();
                        double w = col2.D() - col1.D();
                        double h = row2.D() - row1.D();
                        result.location = QRectF(x, y, w, h);
                    }
                    else
                    {
                        // 使用默认位置
                        result.location = QRectF(input.cols * 0.25, input.rows * 0.25, 
                                                input.cols * 0.5, input.rows * 0.5);
                    }
                }
                catch (...)
                {
                    result.location = QRectF(input.cols * 0.25, input.rows * 0.25, 
                                            input.cols * 0.5, input.rows * 0.5);
                }
                
                // 质量评分功能已禁用
                result.quality = -1.0;  // 表示质量不可用
                
                ctx.barcodeResults.append(result);
                
                qDebug() << "[Barcode] 识别到:" << result.type 
                         << "数据:" << result.data
                         << "质量:" << result.quality;
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
