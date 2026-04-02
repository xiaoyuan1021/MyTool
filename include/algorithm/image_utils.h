#ifndef IMAGE_UTILS_H
#define IMAGE_UTILS_H

#include <opencv2/opencv.hpp>
#include <HalconCpp.h>
#include <QImage>
#include <QLabel>
#include <QCache>
#include <QMutex>
#include <functional>

using namespace cv;
using namespace HalconCpp;

// 缓存键值生成器
struct MatCacheKey {
    size_t hash;
    int type;
    int rows;
    int cols;
    
    MatCacheKey(const cv::Mat& mat) {
        // 使用图像数据指针和尺寸生成哈希
        hash = std::hash<const void*>{}(mat.data);
        type = mat.type();
        rows = mat.rows;
        cols = mat.cols;
    }
    
    bool operator==(const MatCacheKey& other) const {
        return hash == other.hash && type == other.type && 
               rows == other.rows && cols == other.cols;
    }
};

// 为MatCacheKey提供哈希函数
namespace std {
    template<>
    struct hash<MatCacheKey> {
        size_t operator()(const MatCacheKey& key) const {
            return key.hash ^ (key.type << 1) ^ (key.rows << 2) ^ (key.cols << 3);
        }
    };
}

class ImageUtils
{
public:
    //禁止实例化
    ImageUtils()=delete;
    
    static QImage matToQImage(const cv::Mat &mat);
    static Mat qImageToMat(const QImage &image, bool clone = true);

    static Mat HImageToMat(HalconCpp::HObject &H_img);
    static Mat HObjectToMat(const HalconCpp::HObject& hObj, int width, int height);
    static Mat makeStructElement(int ksize);
    static QRect mapLabelToImage(const QRect& rect,const Mat& img,QLabel* label);
    static HRegion MatToHRegion(const cv::Mat& binary);
    static Mat HRegionToMat(const HRegion& region, int width, int height);
    static HImage matToHImage(const cv::Mat& mat);
    
    // 缓存管理
    static void clearCache();
    static void setCacheEnabled(bool enabled);
    static bool isCacheEnabled();
    
private:
    // 缓存存储
    static QCache<MatCacheKey, QImage> s_qimageCache;
    static QCache<MatCacheKey, HImage> s_himageCache;
    static QCache<MatCacheKey, HRegion> s_hregionCache;
    
    // 缓存控制
    static bool s_cacheEnabled;
    static QMutex s_cacheMutex;
    
    // 缓存大小限制
    static const int MAX_CACHE_SIZE = 100; // 最多缓存100个图像
};

#endif // IMAGE_UTILS_H
