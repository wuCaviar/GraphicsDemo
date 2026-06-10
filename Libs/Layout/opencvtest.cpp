//#define EVALDLL_EXPORTS // 需要放在#include "eval.h"前面
#include <iostream>
#include <Windows.h>
#include<algorithm>
#include <stdio.h>
#include<opencv2/opencv.hpp>
#include <thread>
using namespace cv;
using namespace std;
#include <vector>
#include <cmath>
#include <tiff.h>
#include <tiffio.h>
#include <tiffconf.h>
#include "jpeglib.h"
extern int picType;
#include "exif.h"
#include "opencvtest.h"
#include<io.h>
#include<Windows.h>	
#include<gdiplus.h>
#include<fstream>
#include <filesystem>

namespace fs = std::experimental::filesystem;

double distance(const cv::Point& p1, const cv::Point& p2) {
	return std::sqrt(std::pow(p2.x - p1.x, 2) + std::pow(p2.y - p1.y, 2));
}

std::vector<cv::Point> insertPointsWithFixedSpacing(
	std::vector<cv::Point>& contour,
	double spacing = 2,
	bool closed = true
) {
	std::vector<cv::Point> newContour;
	if (contour.empty()) return newContour;
	size_t n = contour.size();
	size_t endIndex = closed ? n : n - 1; // 闭合轮廓处理所有边，非闭合处理前 n-1 条边
	// 添加起点（非闭合轮廓的第一个点）
	newContour.push_back(contour[0]);
	for (size_t i = 0; i < endIndex; ++i) {
		const cv::Point& p1 = contour[i];
		const cv::Point& p2 = contour[(i + 1) % n]; // 安全获取下一个点
		double edgeLength = distance(p1, p2);
		int numPoints = static_cast<int>(edgeLength / spacing); // 需要插入的点数
		// 处理短边：直接添加终点（但跳过闭合轮廓的最后一条边的终点）
		if (numPoints == 0) {
			// 非闭合轮廓或非最后一条边时添加终点
			if (!closed || i != n - 1) {
				newContour.push_back(p2);
			}
			continue;
		}
		// 计算步长并插入点
		double stepX = (p2.x - p1.x) / (numPoints + 1.0);
		double stepY = (p2.y - p1.y) / (numPoints + 1.0);
		for (int j = 1; j <= numPoints; ++j) {
			int x = static_cast<int>(std::round(p1.x + j * stepX));
			int y = static_cast<int>(std::round(p1.y + j * stepY));
			newContour.push_back(cv::Point(x, y));
		}
		// 添加当前边的终点（闭合轮廓的最后一条边终点跳过）
		if (!closed || i != n - 1) {
			newContour.push_back(p2);
		}
	}
	// 闭合轮廓：确保首尾相连（添加第一个点作为终点）
	if (closed && !newContour.empty() && newContour.back() != contour[0]) {
		newContour.push_back(contour[0]);
	}
	return newContour;
}


// 计算点积
float dot(const Point& a, const Point& b) {
	return a.x * b.x + a.y * b.y;
}

// 计算叉积
float cross(const Point& a, const Point& b) {
	return a.x * b.y - a.y * b.x;
}

// 计算向量长度
float length(const Point& v) {
	return std::sqrt(v.x * v.x + v.y * v.y);
}

// 归一化向量
Point normalize(const Point& v) {
	float len = length(v);
	if (len < 1e-6) return Point(0, 0);
	return v * (1.0f / len);
}

// 计算向量的垂直向量(逆时针旋转90度)
Point perpendicular(const Point& v) {
	return Point(-v.y, v.x);
}


#include <opencv2/opencv.hpp>
#include <vector>
#include <cmath>
#include <iostream>

// 自定义二维向量/点类，避免与OpenCV的Point类冲突
struct Vector2 {
	float x, y;
	Vector2(float x = 0, float y = 0) : x(x), y(y) {}
	Vector2 operator-(const Vector2& other) const {
		return Vector2(x - other.x, y - other.y);
	}
	Vector2 operator+(const Vector2& other) const {
		return Vector2(x + other.x, y + other.y);
	}
	Vector2 operator*(float scalar) const {
		return Vector2(x * scalar, y * scalar);
	}
	float dot(const Vector2& other) const {
		return x * other.x + y * other.y;
	}
	float cross(const Vector2& other) const {
		return x * other.y - y * other.x;
	}
	// 计算向量长度
	float length() const {
		return std::sqrt(x * x + y * y);
	}
	// 归一化向量
	Vector2 normalize() const {
		float len = length();
		if (len < 1e-6) return Vector2(0, 0);
		return Vector2(x / len, y / len);
	}
	// 计算垂直向量(逆时针旋转90度)
	Vector2 perpendicular() const {
		return Vector2(-y, x);
	}
};

// 类型转换函数
Vector2 toVector2(const cv::Point& cvPoint) {
	return Vector2(static_cast<float>(cvPoint.x), static_cast<float>(cvPoint.y));
}

cv::Point toCvPoint(const Vector2& vec) {
	return cv::Point(static_cast<int>(vec.x), static_cast<int>(vec.y));
}

// 多边形膨胀算法 - 计算轮廓的外部线条
std::vector<cv::Point> expandContour(const std::vector<cv::Point>& contour, float distance = 10.0f) {
	int count = contour.size();
	if (count < 3) return contour; // 至少需要3个点才能构成多边形
	// 转换为自定义Vector2类型进行计算
	std::vector<Vector2> customContour;
	for (const auto& pt : contour) {
		customContour.push_back(toVector2(pt));
	}
	std::vector<Vector2> expandedPoints;
	std::vector<Vector2> normals;
	// 计算每条边的法向量(指向多边形外部)
	for (int i = 0; i < count; i++) {
		int next = (i + 1) % count;
		Vector2 edge = customContour[next] - customContour[i];
		Vector2 normal = edge.normalize().perpendicular();
		normals.push_back(normal);
	}
	// 计算偏移后的顶点
	for (int i = 0; i < count; i++) {
		int prev = (i + count - 1) % count;
		// 获取当前顶点两侧边的法向量
		Vector2 n1 = normals[prev];
		Vector2 n2 = normals[i];
		// 计算法向量的平均方向(角平分线)
		Vector2 bisector = (n1 + n2).normalize();
		// 计算膨胀距离因子
		float theta = std::atan2(n1.cross(n2), n1.dot(n2));
		float factor = distance / std::cos(theta / 2.0f);
		// 计算偏移后的顶点位置
		Vector2 offset = bisector * factor;
		Vector2 newPoint = customContour[i] + offset;
		expandedPoints.push_back(newPoint);
	}
	// 转换回OpenCV点类型
	std::vector<cv::Point> cvExpandedPoints;
	for (const auto& pt : expandedPoints) {
		cvExpandedPoints.push_back(toCvPoint(pt));
	}
	return cvExpandedPoints;
}


bool GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
	UINT num, size;
	Gdiplus::GetImageEncodersSize(&num, &size);
	Gdiplus::ImageCodecInfo* pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));
	Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);
	bool found = false;
	for (UINT ix = 0; !found && ix < num; ++ix) {
		if (0 == _wcsicmp(pImageCodecInfo[ix].MimeType, format) == 0) {
			*pClsid = pImageCodecInfo[ix].Clsid;
			found = true;
		}
	}
	free(pImageCodecInfo);
	return found;
}

WCHAR * charToWCHAR(const char *s) {
	int w_nlen = MultiByteToWideChar(CP_ACP, 0, s, -1, NULL, 0);
	WCHAR *ret;
	ret = (WCHAR*)malloc(sizeof(WCHAR)*w_nlen);
	memset(ret, 0, sizeof(ret));
	MultiByteToWideChar(CP_ACP, 0, s, -1, ret, w_nlen);
	return ret;
}

// 将 std::string 转换为 WCHAR*（宽字符字符串）
WCHAR* StringToWCHAR(const std::string& str) {
	// 第一步：计算需要的宽字符缓冲区大小
	int bufferSize = MultiByteToWideChar(
		CP_UTF8,       // 源字符串编码（根据实际情况调整，如 CP_ACP 表示系统默认ANSI）
		0,             // 转换选项（0 表示默认）
		str.c_str(),   // 源多字节字符串
		-1,            // 源字符串长度（-1 表示自动计算，包含终止符）
		NULL,          // 目标缓冲区（先传NULL获取所需大小）
		0              // 目标缓冲区大小（0 表示仅计算大小）
	);
	if (bufferSize == 0) {
		// 转换失败，返回NULL
		return NULL;
	}
	// 第二步：分配宽字符缓冲区（使用vector自动管理内存，避免内存泄漏）
	std::vector<WCHAR> buffer(bufferSize);
	// 第三步：执行转换
	int result = MultiByteToWideChar(
		CP_UTF8,
		0,
		str.c_str(),
		-1,
		buffer.data(),  // 目标缓冲区
		bufferSize      // 缓冲区大小
	);
	if (result == 0) {
		// 转换失败
		return NULL;
	}
	// 返回缓冲区地址（注意：vector生命周期需与返回值使用周期一致）
	return buffer.data();
}

// 将 UTF-8 编码的字符串转为 wstring（用于处理含中文的 Windows 文件路径）
std::wstring Utf8ToWstring(const std::string& utf8Str) {
	if (utf8Str.empty()) return std::wstring();
	int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, NULL, 0);
	if (wlen <= 0) return std::wstring();
	std::vector<WCHAR> wbuf(wlen);
	MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, wbuf.data(), wlen);
	return std::wstring(wbuf.data());
}

void DrawTitle(std::string context_string, int width, int start_index) {
	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
	Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
	std::wstring context;
	context = charToWCHAR(context_string.c_str());
	double height = 230;
	int	x_print_resolution_ratio = 1;
	Gdiplus::Bitmap*  pbmp = new Gdiplus::Bitmap(width / x_print_resolution_ratio, height);
	Gdiplus::Graphics g(pbmp);
	Gdiplus::SolidBrush whiteBrush(Gdiplus::Color(255, 255, 255));
	g.FillRectangle(&whiteBrush, 0, 0, pbmp->GetWidth(), pbmp->GetHeight());
	Gdiplus::SolidBrush brush(Gdiplus::Color(0, 0, 0));  // 黑色
	Gdiplus::FontFamily fontFamily(L"Microsoft Sans Serif");
	int font_size = 225;
	Gdiplus::Font font(&fontFamily, font_size / x_print_resolution_ratio, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
	// 测量文字的尺寸
	Gdiplus::RectF textBounds;
	g.MeasureString(context.c_str(), static_cast<int>(context.length()), &font, Gdiplus::PointF(0, 0), &textBounds);
	// 计算使文字居中的坐标
	float x = (width - textBounds.Width) / 2;
	float y = (height - textBounds.Height) / 2 + 23;
	Gdiplus::PointF centerPos(x, y);
	g.DrawString(context.c_str(), static_cast<int>(context.length()), &font, centerPos, &brush);
	CLSID encoder;
	//std::string ansiStr = "Hello, 世界";  // 假设是UTF-8编码的字符串
	//WCHAR* wideStr = StringToWCHAR(ansiStr);
	std::wstring wstr = L"./title_imp/" + std::to_wstring(start_index) + L".bmp";
	const WCHAR* wideStr = wstr.c_str();  // 注意：指针仅在 wstr 有效时可用
	if (!GetEncoderClsid(L"mime/bmp", &encoder)) return;
	pbmp->Save(wideStr, &encoder, NULL);
	delete		pbmp;
}

////写入标题
//cv::Mat DrawTitle(string text, int width) {
//	// 创建一个空白图像
//	cv::Mat image;
//	cv::Scalar textColor;
//	if (picType == 1)
//	{
//		cv::Mat image2 = cv::Mat::zeros(150, width, CV_8UC3);
//		image.setTo(cv::Scalar(255, 255, 255));
//		image = image2;
//		cv::Scalar textColor2(0, 255, 0); // 绿色
//		textColor = textColor2;
//	}
//	else if (picType == 2)
//	{
//		cv::Mat image2 = cv::Mat::zeros(150, width, CV_8UC4);
//		image.setTo(cv::Scalar(255, 255, 255, 255));
//		image = image2;
//		cv::Scalar textColor2(0, 255, 0, 0); // 绿色
//		textColor = textColor2;
//	}
//	else return image;
//	// 定义文本的起始位置
//	cv::Point textOrg(50, 130);
//
//	// 定义字体、字号、颜色和粗细
//	int fontFace = cv::FONT_HERSHEY_SIMPLEX;
//	double fontScale = 5;
//	int thickness = 3;
//
//	// 在图像上添加文字
//	cv::putText(image, text, textOrg, fontFace, fontScale, textColor, thickness);
//	//cv::imwrite("output_with_dashed_border.jpg", image);
//	// 显示带有文字的图像
//	cv::imshow("Image with Text", image);
//
//
//	return image;
//}


// 读取 CMYK TIFF 图像并转换为 RGB 的 OpenCV Mat
cv::Mat readCMYKImage(const std::string& filename) {
	TIFF* tif = TIFFOpen(filename.c_str(), "r");
	if (!tif) {
		std::cerr << "无法打开 TIFF 文件: " << filename << std::endl;
		return cv::Mat();
	}

	uint32 width, height;
	float x_resolution = 0;
	float y_resolution = 0;
	TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
	TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);
	TIFFGetField(tif, TIFFTAG_XRESOLUTION, &x_resolution);
	TIFFGetField(tif, TIFFTAG_YRESOLUTION, &y_resolution);
	std::vector<unsigned char> cmykData(width * height * 4);
	for (uint32 y = 0; y < height; ++y) {
		TIFFReadScanline(tif, &cmykData[y * width * 4], y);
	}
	TIFFClose(tif);

	cv::Mat rgbImage(height, width, CV_8UC4);
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			int index = (y * width + x) * 4;
			rgbImage.at<cv::Vec4b>(y, x)[3] = 255 - cmykData[index];
			rgbImage.at<cv::Vec4b>(y, x)[2] = 255 - cmykData[index + 1];
			rgbImage.at<cv::Vec4b>(y, x)[1] = 255 - cmykData[index + 2];
			rgbImage.at<cv::Vec4b>(y, x)[0] = 255 - cmykData[index + 3];
		}
	}
	return rgbImage;
}

// 将 RGB 图像转换为 CMYK 并保存为 TIFF 文件
void saveAsCMYKTIFF(const cv::Mat& rgbImage, const std::string& outputFilename, float dpi = 100.0f) {
	TIFF* tif = TIFFOpen(outputFilename.c_str(), "w");
	if (!tif) {
		std::cerr << "无法打开 TIFF 文件进行写入: " << outputFilename << std::endl;
		return;
	}
	int width = rgbImage.cols;
	int height = rgbImage.rows;
	TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, width);
	TIFFSetField(tif, TIFFTAG_IMAGELENGTH, height);
	TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 4);
	TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
	TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
	TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
	TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_SEPARATED);	
	TIFFSetField(tif, TIFFTAG_INKSET, INKSET_CMYK);
	TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH);//分辨率单位为英寸
	TIFFSetField(tif, TIFFTAG_XRESOLUTION, dpi);//x分辨率
	TIFFSetField(tif, TIFFTAG_YRESOLUTION, dpi);//y分辨率

	std::vector<unsigned char> cmykData(width * height * 4);
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			cv::Vec4b pixel = rgbImage.at<cv::Vec4b>(y, x);
			int index = (y * width + x) * 4;
			cmykData[index] = 255 - pixel[3];
			cmykData[index + 1] = 255 - pixel[2];
			cmykData[index + 2] = 255 - pixel[1];
			cmykData[index + 3] = 255 - pixel[0];
		}
	}
	for (int y = 0; y < height; ++y) {
		TIFFWriteScanline(tif, &cmykData[y * width * 4], y, 0);
	}
	TIFFClose(tif);
}

// 字节序转换（EXIF 数据使用大端序，本地可能是小端序）
uint16_t swap16(uint16_t value) {
	return (value << 8) | (value >> 8);
}

uint32_t swap32(uint32_t value) {
	return ((value << 24) & 0xFF000000) |
		((value << 8) & 0x00FF0000) |
		((value >> 8) & 0x0000FF00) |
		((value >> 24) & 0x000000FF);
}

// 读取 JPEG 图像
bool getJpegDPI(std::string imagePath, int& xDPI) {
	FILE* fp = fopen(imagePath.c_str(), "rb");
	if (!fp) return false;

	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&cinfo);
	jpeg_stdio_src(&cinfo, fp);
	jpeg_read_header(&cinfo, TRUE);

	xDPI  = 96.0f;

	if (cinfo.density_unit == 1) {
		xDPI = cinfo.X_density;
	}
	else if (cinfo.density_unit == 2) {
		xDPI = cinfo.X_density * 2.54f;
	}

	jpeg_destroy_decompress(&cinfo);
	fclose(fp);
	return true;
}
bool getImageDPI(const string& path, int& dpiX)
{
	// 1. 先读 EXIF
	FILE* fp = fopen(path.c_str(), "rb");
	fseek(fp, 0, SEEK_END);
	long size = ftell(fp);
	rewind(fp);
	unsigned char* buf = new unsigned char[size];
	fread(buf, 1, size, fp);
	fclose(fp);

	easyexif::EXIFInfo exif;
	int code = exif.parseFrom(buf, size);
	delete[] buf;

	// 2. 如果 EXIF 有有效值
	if (code == 0 && exif.XResolution > 1) {
		dpiX = exif.XResolution;
		return true;
	}

	// 3. EXIF 无效 → 按文件类型分别处理
	// 3a. TIFF 文件 → 通过 libtiff 读取 DPI
	string path_lower = path;
	transform(path_lower.begin(), path_lower.end(), path_lower.begin(), ::tolower);
	if (path_lower.find(".tif") != string::npos) {
		TIFF* tif = TIFFOpen(path.c_str(), "r");
		if (tif) {
			float x_res = 0;
			TIFFGetField(tif, TIFFTAG_XRESOLUTION, &x_res);
			TIFFClose(tif);
			if (x_res > 1) {
				dpiX = (int)(x_res + 0.5);
				return true;
			}
		}
		dpiX = 96; // TIFF fallback
		return true;
	}

	// 3b. 非 TIFF → 用 libjpeg
	return getJpegDPI(path, dpiX);
}
std::vector<unsigned char> read_jpeg(const char* filename, int& width, int& height, int & dpi_x, int & dpi_y) {
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;
	FILE* infile;
	if ((infile = fopen(filename, "rb")) == NULL) {
		std::cerr << "无法打开文件: " << filename << std::endl;
		return std::vector<unsigned char>();
	}
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&cinfo);
	jpeg_stdio_src(&cinfo, infile);
	jpeg_read_header(&cinfo, TRUE);
	jpeg_start_decompress(&cinfo);

	width = cinfo.output_width;
	height = cinfo.output_height;
	dpi_x = cinfo.X_density;
	dpi_y = cinfo.Y_density;
	int num_components = cinfo.output_components;
	std::vector<unsigned char> image_data(width * height * num_components);
	unsigned char* row_pointer[1];
	while (cinfo.output_scanline < cinfo.output_height) {
		row_pointer[0] = &image_data[cinfo.output_scanline * width * num_components];
		jpeg_read_scanlines(&cinfo, row_pointer, 1);
	}
	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);
	fclose(infile);
	return image_data;
}
// 保存 JPEG 图像并设置 DPI
void save_jpeg_with_dpi(const char* filename, const std::vector<unsigned char>& image_data, int width, int height, int quality, int dpi_x, int dpi_y) {
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	FILE* outfile;
	if ((outfile = fopen(filename, "wb")) == NULL) {
		std::cerr << "无法打开文件: " << filename << std::endl;
		return;
	}
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);
	jpeg_stdio_dest(&cinfo, outfile);
	cinfo.image_width = width;
	cinfo.image_height = height;
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_RGB;
	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, quality, TRUE);
	// 设置 DPI
	cinfo.X_density = dpi_x;
	cinfo.Y_density = dpi_y;
	cinfo.density_unit = 1; // 1 表示每英寸点数
	jpeg_start_compress(&cinfo, TRUE);
	JSAMPROW row_pointer[1];
	int row_stride = width * 3;
	while (cinfo.next_scanline < cinfo.image_height) {
		row_pointer[0] = const_cast<unsigned char*>(&image_data[cinfo.next_scanline * row_stride]);
		jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}
	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);
	fclose(outfile);
}

int changedpi(const char* input_filename, int new_dpi_x, int new_dpi_y) {
	int width, height;
	int quality = 100;
	int dpi_x = 0;
	int	dpi_y = 0;
	int channels = 1;
	//cv::Mat src = cv::imread("tt.jpg");
	// 读取 JPEG 图像
	std::vector<unsigned char> image_data = read_jpeg(input_filename, width, height, dpi_x, dpi_y);
	if (image_data.empty()) 
		return 1;
	// 保存图像并修改 DPI
	save_jpeg_with_dpi(input_filename, image_data, width, height, quality, new_dpi_x, new_dpi_y);
	std::cout << "图像已处理，新 DPI 设置为 " << new_dpi_x << "x" << new_dpi_y << std::endl;
	return 0;
}

cv::Mat resize(cv::Mat src, double scale) {
	// 检查图像是否成功加载
	if (src.empty()) {
		std::cerr << "Error: Loading image" << std::endl;
	}
	// 创建一个Mat对象用于存储缩放后的图像
	cv::Mat dst;
	// 缩放因子
	// 缩放图像
	cv::resize(src, dst, cv::Size(), scale, scale, cv::INTER_LANCZOS4); 
	return dst;
}

void EnsureFoldExistA(string fold_path, bool bool_hide) {
	if (CreateDirectoryA(fold_path.c_str(), NULL)) //判断是否存在，否则创建
	{
	}
}

// 自然排序比较函数：1_1 < 2_1 < 10_1 < 11_1
bool naturalCompare(const string& a, const string& b) {
	size_t i = 0, j = 0;
	while (i < a.size() && j < b.size()) {
		// 如果是数字
		if (isdigit(a[i]) && isdigit(b[j])) {
			size_t iStart = i, jStart = j;
			// 取出完整数字
			while (i < a.size() && isdigit(a[i])) i++;
			while (j < b.size() && isdigit(b[j])) j++;
			string numA = a.substr(iStart, i - iStart);
			string numB = b.substr(jStart, j - jStart);
			// 数字长度长的更大
			if (numA.size() != numB.size())
				return numA.size() < numB.size();
			// 长度相同逐位比
			return numA < numB;
		}
		// 非数字直接比字符
		if (a[i] != b[j])
			return a[i] < b[j];
		i++;
		j++;
	}
	return a.size() < b.size();
}

vector<std::string> listFiles(std::string dirPath) {
	WIN32_FIND_DATAA FindFileData;
	HANDLE hFind = INVALID_HANDLE_VALUE;
	DWORD dwError = 0;
	vector<std::string> path_vector;
	string dirPath_temp = dirPath;
	dirPath_temp += "*.jpg";
	// 指定要搜索的目录和文件名模式，例如 "*.*" 表示所有文件"C:\\path\\to\\directory\\*.*"
	hFind = FindFirstFileA(dirPath_temp.c_str(), &FindFileData);
	if (hFind == INVALID_HANDLE_VALUE) {
		printf("FindFirstFile failed (%d)\n", GetLastError());
		return path_vector;
	}
	else {
		do {
			path_vector.push_back(FindFileData.cFileName);
			// 打印文件名
			//printf("%s\n", FindFileData.cFileName);
		} while (FindNextFileA(hFind, &FindFileData) != 0);
		dwError = GetLastError();
		if (dwError != ERROR_NO_MORE_FILES) {
			printf("FindNextFile failed (%d)\n", dwError);
		}
	}
	FindClose(hFind);
	// 关键：自然数字排序
	sort(path_vector.begin(), path_vector.end(), naturalCompare);
	for (std::string &Str : path_vector) {
		Str = dirPath + Str;
	}
	return path_vector;
}

vector<std::string> listTiffFiles(std::string dirPath) {
	WIN32_FIND_DATAA FindFileData;
	HANDLE hFind = INVALID_HANDLE_VALUE;
	DWORD dwError = 0;
	vector<std::string> path_vector;
	string dirPath_temp = dirPath;
	dirPath_temp += "*.tif";
	// 指定要搜索的目录和文件名模式，例如 "*.*" 表示所有文件"C:\\path\\to\\directory\\*.*"
	hFind = FindFirstFileA(dirPath_temp.c_str(), &FindFileData);
	if (hFind == INVALID_HANDLE_VALUE) {
		printf("FindFirstFile failed (%d)\n", GetLastError());
		return path_vector;
	}
	else {
		do {
			path_vector.push_back(dirPath + FindFileData.cFileName);
			// 打印文件名
			//printf("%s\n", FindFileData.cFileName);
		} while (FindNextFileA(hFind, &FindFileData) != 0);
		dwError = GetLastError();
		if (dwError != ERROR_NO_MORE_FILES) {
			printf("FindNextFile failed (%d)\n", dwError);
		}
	}
	FindClose(hFind);
	return path_vector;
}

#include <unordered_set>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/core.hpp>

// 自定义虚线绘制函数
void drawDashedPoly(cv::Mat& img, const std::vector<cv::Point>& points, const cv::Scalar& color,
	int thickness = 1, int dashLength = 10, int gapLength = 5) {
	double total_length = 0;
	for (size_t i = 0; i < points.size(); ++i) {
		const cv::Point& p1 = points[i];
		const cv::Point& p2 = points[(i + 1) % points.size()];  // 闭合多边形
		// 计算线段方向
		double dx = p2.x - p1.x;
		double dy = p2.y - p1.y;
		double length = std::sqrt(dx * dx + dy * dy);
		double stepX = dx / length;
		double stepY = dy / length;
		int index = 0;
		// 分段绘制虚线
		double drawn = 0;
		while (drawn + 1 < length) {
			index++;
			if (index > 1000)
				break;
			double start = drawn;
			int need_length = 0;
			if ((int)total_length % (dashLength + gapLength) < dashLength)
				need_length = dashLength - ((int)total_length % (dashLength + gapLength));
			else
				need_length = dashLength + gapLength - ((int)total_length % (dashLength + gapLength));
			double end = std::min(drawn + need_length, length);

			cv::Point startPt(p1.x + start * stepX, p1.y + start * stepY);
			cv::Point endPt(p1.x + end * stepX + 0.5, p1.y + end * stepY + 0.5);

			if ((int)total_length % (dashLength + gapLength) < dashLength)
			{
				cv::line(img, startPt, endPt, color, thickness);
			}
			total_length += cv::norm(startPt - endPt);
			drawn += cv::norm(startPt - endPt);  // 跳过间隔
		}
	}
}

vector<Point> expand_polygon(const vector<Point> &pList, float SAFELINE = 20.0f) {// 扩展多边形 按逆时针顺序排列 already ordered by anticlockwise
	// 1. vertex set
	// pList
	// 2. edge set and normalize it
	vector<Point> out;
	vector<Point2f> dpList, ndpList;
	int count = pList.size();
	for (int i = 0; i < count; i++) {
		int next = (i == (count - 1) ? 0 : (i + 1));
		dpList.push_back(pList.at(next) - pList.at(i));
		float unitLen = 1.0f / sqrt(dpList.at(i).dot(dpList.at(i)));
		ndpList.push_back(dpList.at(i) * unitLen);
		//cout << "i=" << i << ",pList:" << pList.at(next) << "," << pList.at(i) << ",dpList:" << dpList.at(i) << ",ndpList:" << ndpList.at(i) << endl;
	}
	// 3. compute Line
	//SAFELINE 负数为内缩， 正数为外扩。 需要注意算法本身并没有检测内缩多少后折线会自相交，那不是本代码的示范意图
	for (int i = 0; i < count; i++) {
		int startIndex = (i == 0 ? (count - 1) : (i - 1));
		int endIndex = i;
		float sinTheta = ndpList.at(startIndex).cross(ndpList.at(endIndex));
		Point2f orientVector = ndpList.at(endIndex) - ndpList.at(startIndex);//i.e. PV2-V1P=PV2+PV1
		Point2f temp_out;
		temp_out.x = pList.at(i).x + SAFELINE / sinTheta * orientVector.x;
		temp_out.y = pList.at(i).y + SAFELINE / sinTheta * orientVector.y;
		if (temp_out.y < 0)
			temp_out.y = 0;
		if (temp_out.x < 0)
			temp_out.x = 0;
		out.push_back(temp_out);
	}
	reverse(out.begin(), out.end());//反转 即白变黑，黑变白 每个像素中的每个通道的值进行反转
	//cout<<endl<<"out:"<<out<<endl;
	return out;
}

//void dfs(vector<int>& nums, vector<int>& path, vector<bool>& used, vector<vector<int>>& ans, int len, int depth) {
//	if (depth == len) {
//		ans.push_back(path);//一条路走完时depth==len,并且把这一条路的元素保存
//		return;
//	}
//	for (int i = 0; i < len; ++i) {
//		if (used[i]) {//如果这个元素用过的话就跳过
//			continue;
//		}
//		//nums[i]没有用过才会走到这
//		used[i] = true;//标记已使用
//		path.push_back(nums[i]);//记录元素到path中
//		dfs(nums, path, used, ans, len, depth + 1);//深度+1，继续递归
//		path.erase(path.end() - 1);//递归返回后，删除该元素
//		used[i] = false;//清除已使用标记
//	}
//}
//vector<vector<int>> permute(vector<int>& nums) {
//	vector<int> path;//用于记录一条支路上的所有元素
//	vector<bool> used(nums.size(), 0);//用以path记录元素时判断该元素有没有已经被记录
//	vector<vector<int>> ans;//记录全排列的数组
//	int len = nums.size();
//	dfs(nums, path, used, ans, len, 0);
//	// 步骤1：对 ans 排序（让重复的排列相邻）
//	sort(ans.begin(), ans.end());
//	// 步骤2：用 unique 移动重复元素到末尾，erase 删除重复部分
//	ans.erase(unique(ans.begin(), ans.end()), ans.end());
//	return ans;
//}

// 全排列
// 给大数组元素分配分组ID：同一小数组的元素ID相同，未分组元素ID唯一（用负数避免冲突）
unordered_map<int, int> assignGroupIds(const vector<int>& bigNums, const vector<vector<int>>& groups) {
	unordered_map<int, int> numToGroup;
	int groupId = 0;

	// 1. 给每个分组分配相同的ID
	for (const auto& group : groups) {
		for (int num : group) {
			// 确保同一元素只分配一次ID（若元素在多个分组，以第一个分组为准）
			if (numToGroup.find(num) == numToGroup.end()) {
				numToGroup[num] = groupId;
			}
		}
		groupId++;
	}
	// 2. 给未分组的元素分配唯一ID（用负数，避免与分组ID冲突）
	int uniqueId = -1;
	for (int num : bigNums) {
		if (numToGroup.find(num) == numToGroup.end()) {
			numToGroup[num] = uniqueId;
			uniqueId--;
		}
	}
	return numToGroup;
}
void dfs(const vector<int>& bigNums, const unordered_map<int, int>& numToGroup,
	vector<int>& path, vector<bool>& used, vector<vector<int>>& ans) {
	// 递归终止：路径长度等于大数组长度，记录结果
	if (path.size() == bigNums.size()) {
		ans.push_back(path);
		return;
	}
	// 记录当前层已使用的分组ID（核心去重：同一分组ID在当前层只能用一次）
	unordered_map<int, bool> usedGroupIds;
	for (int i = 0; i < bigNums.size(); ++i) {
		if (used[i]) continue; // 跳过已使用的元素
		int currentNum = bigNums[i];
		int currentGroupId = numToGroup.at(currentNum);
		// 关键去重：当前分组ID已在当前层使用，跳过
		if (usedGroupIds[currentGroupId]) continue;
		// 选择当前元素
		used[i] = true;
		path.push_back(currentNum);
		usedGroupIds[currentGroupId] = true; // 标记该分组ID当前层已使用
		// 递归下一层
		dfs(bigNums, numToGroup, path, used, ans);
		// 回溯：撤销选择
		path.pop_back();
		used[i] = false;
	}
}
// 主函数：大数组 + 多个小数组（分组）-> 去重后的全排列
vector<vector<int>> permuteWithGroups(const vector<int>& bigNums, const vector<vector<int>>& groups) {
	vector<int> path;
	vector<bool> used(bigNums.size(), false);
	vector<vector<int>> ans;
	// 步骤1：给每个元素分配分组ID
	auto numToGroup = assignGroupIds(bigNums, groups);
	// 步骤2：回溯生成去重后的全排列
	dfs(bigNums, numToGroup, path, used, ans);
	return ans;
}

//void combinate(int iPos, int iProc, int iTol, int iKey, int data[], int des[], vector<vector<int>>& cns) {
//	if (iProc > iTol) {
//		return;
//	}
//	if (iPos == iKey) {
//		vector<int> cnt_n;
//		for (int i = 0; i < iKey; i++) {
//			cnt_n.push_back(des[i]);
//		}
//		cns.push_back(cnt_n);//一条路走完时depth==len,并且把这一条路的元素保存
//		return;
//	}
//	else {
//		combinate(iPos, iProc + 1, iTol, iKey, data, des, cns);
//		des[iPos] = data[iProc];
//		combinate(iPos + 1, iProc + 1, iTol, iKey, data, des, cns);
//	}
//}
//void combinate(int iPos, int iProc, int iTol, int iKey, int data[], int des[], vector<vector<int>>& cns) {
//	if (iProc > iTol) {
//		return;
//	}
//	if (iPos == iKey) {
//		vector<int> cnt_n;
//		for (int i = 0; i < iKey; i++) {
//			cnt_n.push_back(des[i]);
//		}
//		cns.push_back(cnt_n);
//		return;
//	}
//	else {
//		// 分支1：不选当前 iProc 位置的元素，直接递归下一个
//		combinate(iPos, iProc + 1, iTol, iKey, data, des, cns);
//		// 剪枝逻辑：跳过重复元素（核心去重）
//		// 条件：1. iProc > 0（避免越界）；2. 当前元素 == 前一个元素（重复）；
//		//      3. 前一个元素未被选择（说明是重复分支，跳过）
//		if (iProc > 0 && data[iProc] == data[iProc - 1]) {
//			// 判断前一个元素是否未被选择：iProc-1 未被选的标志是「当前 iPos 仍在原位置」
//			// （因如果选了 iProc-1，iPos 会+1；未选则 iPos 不变，此时 iProc 已+1，说明跳过了 iProc-1）
//			return;
//		}
//		// 分支2：选择当前 iProc 位置的元素，存入 des 后递归
//		des[iPos] = data[iProc];
//		combinate(iPos + 1, iProc + 1, iTol, iKey, data, des, cns);
//	}
//}

//组合排列
// 步骤1：给所有元素分配等价标识（同一分组标识相同，未分组标识唯一）
unordered_map<int, int> assignEquivMark(const vector<int>& bigNums, const vector<vector<int>>& groups) {
	unordered_map<int, int> numToMark;
	int mark = 0;
	// 同一分组的元素分配相同标识（同一元素出现在多个分组，以第一个为准）
	for (const auto& group : groups) {
		for (int num : group) {
			if (numToMark.find(num) == numToMark.end()) {
				numToMark[num] = mark;
			}
		}
		mark++;
	}
	// 未分组的元素分配唯一负数标识（避免与分组标识冲突）
	int uniqueMark = -1;
	for (int num : bigNums) {
		if (numToMark.find(num) == numToMark.end()) {
			numToMark[num] = uniqueMark;
			uniqueMark--;
		}
	}
	return numToMark;
}

// 步骤2：生成所有长度的组合（k=1到n，无重复组合，元素顺序与原数组一致）
void generateAllCombinations(const vector<int>& bigNums, int start, vector<int>& path, vector<vector<int>>& allCombos) {
	if (!path.empty()) {
		allCombos.push_back(path);
	}
	if (start >= bigNums.size()) 
		return;
	for (int i = start; i < bigNums.size(); ++i) {
		path.push_back(bigNums[i]);
		generateAllCombinations(bigNums, i + 1, path, allCombos);
		path.pop_back(); // 回溯
	}
}
// 步骤3：组合去重（等价元素组成的组合视为重复，仅保留一个）
vector<vector<int>> comboDedupByEquiv(const vector<vector<int>>& allCombos, const unordered_map<int, int>& numToMark) {
	vector<vector<int>> uniqueCombos;
	unordered_set<string> seenEquivSets; // 存储排序后的等价标识集合字符串，用于快速判重
	for (const auto& combo : allCombos) {
		// 1. 将组合转换为等价标识集合
		vector<int> equivMarks;
		for (int num : combo) {
			equivMarks.push_back(numToMark.at(num));
		}
		// 2. 排序标识集合（确保顺序无关，如[0,1]和[1,0]视为同一集合）
		sort(equivMarks.begin(), equivMarks.end());
		// 3. 转换为字符串，便于哈希判重
		string equivSetStr;
		for (size_t i = 0; i < equivMarks.size(); ++i) {
			equivSetStr += to_string(equivMarks[i]);
			if (i != equivMarks.size() - 1) {
				equivSetStr += ",";
			}
		}
		// 4. 未出现过的标识集合，保留原组合
		if (seenEquivSets.find(equivSetStr) == seenEquivSets.end()) {
			seenEquivSets.insert(equivSetStr);
			uniqueCombos.push_back(combo);
		}
	}
	return uniqueCombos;
}
// 核心函数：通过参数输入大数组和分组，返回去重后的所有长度组合
vector<vector<int>> getUniqueCombos(const vector<int>& bigNums, const vector<vector<int>>& groups) {
	vector<vector<int>> finalResult;
	if (bigNums.empty()) {
		return finalResult;
	}
	// 1. 生成所有长度的组合（k=1到bigNums.size()）
	vector<vector<int>> allCombos;
	vector<int> comboPath;
	generateAllCombinations(bigNums, 0, comboPath, allCombos);
	// 2. 分配元素等价标识
	auto numToMark = assignEquivMark(bigNums, groups);
	// 3. 组合去重（去掉仅等价元素不同的重复组合）
	finalResult = comboDedupByEquiv(allCombos, numToMark);
	return finalResult;
}

// 在轮廓外部添加线条
void drawExternalLine(cv::Mat& image, const std::vector<std::vector<cv::Point>>& contours,
	float distance = 10.0f, const cv::Scalar& color = cv::Scalar(0, 0, 255),
	int thickness = 2) {
	for (const auto& contour : contours) {
		// 计算外部线条的轮廓
		std::vector<cv::Point> expandedContour = expandContour(contour, distance);
		// 绘制原始轮廓(蓝色)
		cv::drawContours(image, std::vector<std::vector<cv::Point>>{contour}, -1,
			cv::Scalar(255, 0, 0), 1);
		// 绘制外部线条(红色)
		cv::drawContours(image, std::vector<std::vector<cv::Point>>{expandedContour}, -1,
			color, thickness);
	}
}

// 去除字符串首尾空格（辅助函数）
std::string Trim(const std::string& str) {
	std::string res = str;
	// 去除开头空格
	res.erase(res.begin(), std::find_if(res.begin(), res.end(), [](unsigned char ch) {
		return !std::isspace(ch);
	}));
	// 去除结尾空格
	res.erase(std::find_if(res.rbegin(), res.rend(), [](unsigned char ch) {
		return !std::isspace(ch);
	}).base(), res.end());
	return res;
}
// 核心：解析 UTF-8 编码的 INI 文件，返回 分组→(键→值) 的映射
std::map<std::string, std::map<std::string, std::string>> ParseIni(const std::string& iniPath) {
	std::map<std::string, std::map<std::string, std::string>> iniData;
	std::ifstream file(iniPath, std::ios::in | std::ios::binary); // 二进制模式避免编码转换
	if (!file.is_open()) {
		std::cerr << "错误：无法打开 INI 文件！路径：" << iniPath << std::endl;
		return iniData;
	}
	std::string currentSection = "";
	std::string line;
	// 跳过 UTF-8 BOM（如果有）
	char bom[3] = { 0 };
	file.read(bom, 3);
	if (!(bom[0] == 0xEF && bom[1] == 0xBB && bom[2] == 0xBF)) {
		file.seekg(0, std::ios::beg); // 无 BOM 则回到文件开头
	}
	// 逐行解析
	while (std::getline(file, line)) {
		std::string trimmedLine = Trim(line);
		// 跳过空行和注释（; 开头）
		if (trimmedLine.empty() || trimmedLine[0] == ';') {
			continue;
		}
		// 解析分组：[Setting]
		if (trimmedLine[0] == '[' && trimmedLine.back() == ']') {
			currentSection = Trim(trimmedLine.substr(1, trimmedLine.size() - 2));
			continue;
		}
		// 解析键值对：key=value
		size_t eqPos = trimmedLine.find('=');
		if (eqPos == std::string::npos) {
			continue; // 无 = 号，跳过
		}
		std::string key = Trim(trimmedLine.substr(0, eqPos));
		std::string value = Trim(trimmedLine.substr(eqPos + 1));
		if (!key.empty()) {
			iniData[currentSection][key] = value;
		}
	}
	file.close();
	return iniData;
}
// 辅助函数：string 转 int
int StringToInt(const std::string& str, int defaultValue = 0) {
	try {
		return std::stoi(str);
	}
	catch (...) {
		return defaultValue;
	}
}
// 辅助函数：string 转 bool
bool StringToBool(const std::string& str, bool defaultValue = false) {
	if (str == "true" || str == "TRUE" || str == "1") return true;
	if (str == "false" || str == "FALSE" || str == "0") return false;
	return defaultValue;
}

//对比图片
// 计算两点间欧氏距离（像素距离）
double calcPointDistance(const Point& p1, const Point& p2) {
	int dx = p1.x - p2.x;
	int dy = p1.y - p2.y;
	return sqrt(dx * dx + dy * dy); // 欧氏距离（像素级）
}
// 判断两组点是否「相同」（每个点都有对应点，距离≤5像素，双向唯一匹配）
bool isPointGroupEqual(const vector<Point>& groupA, const vector<Point>& groupB, double maxDist = 5.0) {
	// 1. 点数量不同，直接不匹配
	if (groupA.size() != groupB.size()) {
		return false;
	}
	int n = groupA.size();
	if (n == 0) { // 空点组跳过
		return false;
	}
	// 2. 双向唯一匹配：A→B 且 B→A 都能找到唯一对应点
	vector<bool> usedB(n, false); // 标记B组点是否已被匹配
	vector<bool> usedA(n, false); // 标记A组点是否已被匹配
	// 第一步：A组每个点找B组未被匹配的对应点（距离≤maxDist）
	for (int i = 0; i < n; ++i) {
		bool found = false;
		for (int j = 0; j < n; ++j) {
			if (!usedB[j] && calcPointDistance(groupA[i], groupB[j]) <= maxDist) {
				usedB[j] = true; // 标记B组第j个点已匹配
				found = true;
				break;
			}
		}
		if (!found) { // A组有一个点找不到对应点，直接不匹配
			return false;
		}
	}
	// 第二步：B组每个点找A组未被匹配的对应点（确保双向匹配，避免单向对应）
	for (int j = 0; j < n; ++j) {
		bool found = false;
		for (int i = 0; i < n; ++i) {
			if (!usedA[i] && calcPointDistance(groupB[j], groupA[i]) <= maxDist) {
				usedA[i] = true; // 标记A组第i个点已匹配
				found = true;
				break;
			}
		}
		if (!found) { // B组有一个点找不到对应点，直接不匹配
			return false;
		}
	}
	// 3. 所有点都双向匹配成功，视为相同点组
	return true;
}
// 主函数：全量检查所有点组，找出每组的匹配组
void findAllGroupMatches(const vector<vector<Point>>& conPolyzero180, map<int, vector<int>> &matchResults, double maxDist = 5.0) {
	int groupCount = conPolyzero180.size();
	if (groupCount < 2) {
		cout << "点组数量不足（至少需要2组）！" << endl;
		return;
	}
	// 存储结果：key=基准组序号（从1开始），value=与该组匹配的所有组序号
	//map<int, vector<int>> matchResults;
	// 初始化结果容器（每个组默认匹配列表为空）
	for (int i = 0; i < groupCount; ++i) {
		matchResults[i + 1] = {};
	}
	// 遍历所有两两组合（i < j，避免重复计算：第i组和第j组只需比较一次）
	for (int i = 0; i < groupCount; ++i) {
		for (int j = i + 1; j < groupCount; ++j) {
			const vector<Point>& groupI = conPolyzero180[i];
			const vector<Point>& groupJ = conPolyzero180[j];
			if (isPointGroupEqual(groupI, groupJ, maxDist)) {
				// 两组匹配：互相加入对方的匹配列表
				int j_ = j;
				if (j > groupCount / 2-1)
					j_ = j - groupCount / 2;
				matchResults[i + 1].push_back(j_ + 1);
				matchResults[j_ + 1].push_back(i + 1);
			}
		}
	}
	//// 输出所有结果（按基准组分类）—— 替换结构化绑定，用迭代器遍历
	//cout << "=== 所有点组匹配结果（最大允许距离：" << maxDist << "像素）===" << endl;
	//// 遍历map：用map<int, vector<int>>::iterator
	//for (map<int, vector<int>>::iterator it = matchResults.begin(); it != matchResults.end(); ++it) {
	//	int baseIdx = it->first; // 基准组序号
	//	vector<int> matchIdxs = it->second; // 匹配组序号列表
	//	const vector<Point>& baseGroup = conPolyzero180[baseIdx - 1];
	//	cout << "\n基准组" << baseIdx << "：包含 " << baseGroup.size() << " 个点" << endl;
	//	if (matchIdxs.empty()) {
	//		cout << "  → 无匹配的点组" << endl;
	//	}
	//	else {
	//		cout << "  → 匹配的点组序号：";
	//		for (int idx : matchIdxs) {
	//			cout << idx << " ";
	//		}
	//		cout << endl;
	//	}
	//}
	//// 可选：输出「孤立组」（无任何匹配组）—— 同样用迭代器
	//cout << "\n=== 孤立点组（无任何匹配对象）===" << endl;
	//bool hasIsolated = false;
	//for (map<int, vector<int>>::iterator it = matchResults.begin(); it != matchResults.end(); ++it) {
	//	int baseIdx = it->first;
	//	vector<int> matchIdxs = it->second;
	//	if (matchIdxs.empty()) {
	//		cout << "组" << baseIdx << endl;
	//		hasIsolated = true;
	//	}
	//}
	//if (!hasIsolated) {
	//	cout << "无孤立点组（所有组都有匹配对象）" << endl;
	//}
}

//获取轮廓
bool getContours(Mat & dst, vector<Mat> & img_element, vector<vector<Point>> & contours, int & y_start, cv::Mat & foreground, int & jpg_index, bool bool_last_one, int & last_y_pose, double * progress, int pthcount, bool m_MoreFivepic, bool *bool_run, int & None_end_y, string *error_info, int paper_width)
{
	if (*bool_run == false)
		return true;
	vector<vector<Point>> conPoly(contours.size());//找轮廓点
	vector<Rect> boundRect(contours.size());//画外框
	int min_width = 0xffffff;
	int min_width_ = 0xffffff;
	int max_width = 0;
	for (int i = 0; i < contours.size(); i++)
	{
		double peri = arcLength(contours[i], true);//计算闭合轮廓的周长
		approxPolyDP(contours[i], conPoly[i], 0.00001 * peri, true);//找到轮廓点
		boundRect[i] = boundingRect(conPoly[i]);
		int width_ = boundRect[i].width / 2;
		if (boundRect[i].width / 2 > 800)
			width_ = 800;
		min_width_ = min(boundRect[i].width, min_width_);//width_
		//min_width = min(boundRect[i].width - 800, min_width);//width_
		min_width = min(boundRect[i].width, min_width);//width_
		if (min_width < 550)
			min_width = 550;
		//max_width = max(boundRect[i].width - width_, max_width);
		//if (max_width < 200)
		//	max_width = 200;
	}
	vector<vector<Point>> conPolyzero;
	vector<vector<Point>> conPolyzero180;
	vector<vector<Point>> conPolyvertex;//顶
	vector<vector<Point>> conPolyvertex180;
	vector<bool> m_pic_canright;
	conPolyzero = conPoly;
	conPolyzero180 = conPoly;
	int max_y_all = 0;
	for (int i = 0; i < conPoly.size(); i++)
	{
		int max_y = 0;
		int max_x = 0;
		int min_x = 0;
		int min_y = 0;
		// 平移向量
		cv::Point translation_vector(boundRect[i].x, boundRect[i].y); // 将轮廓平移 (50, 50) 个像素
		for (size_t j = 0; j < conPolyzero[i].size(); j++)
		{
			conPolyzero[i][j] -= translation_vector;
			max_y = max(max_y, conPolyzero[i][j].y);
			max_x = max(max_x, conPolyzero[i][j].x);
			min_y = min(min_y, conPolyzero[i][j].y);
			min_x = min(min_x, conPolyzero[i][j].x);
		}
		if (max_x > dst.cols/2)
			m_pic_canright.push_back(false);
		else
			m_pic_canright.push_back(true);
		max_y_all = max(max_y_all, max_y + boundRect[i].y);
		conPolyvertex.push_back(vector<Point>());
		for (size_t j = 0; j < conPolyzero[i].size(); j++)
		{
			if (max_y == conPolyzero[i][j].y || max_x == conPolyzero[i][j].x ||
				min_y == conPolyzero[i][j].y || min_x == conPolyzero[i][j].x)
				conPolyvertex[i].push_back(conPolyzero[i][j]);
		}
		conPolyzero180[i] = conPolyzero[i];
		translation_vector.x = max_x;	
		translation_vector.y = max_y;
		max_y = 0;
		max_x = 0;
		min_x = 0;
		min_y = 0;
		for (size_t j = 0; j < conPolyzero180[i].size(); j++)
		{
			conPolyzero180[i][j] = translation_vector - conPolyzero180[i][j];
			max_y = max(max_y, conPolyzero180[i][j].y);
			max_x = max(max_x, conPolyzero180[i][j].x);
			min_y = min(min_y, conPolyzero180[i][j].y);
			min_x = min(min_x, conPolyzero180[i][j].x);
		}
		max_y_all = max(max_y_all, max_y + boundRect[i].y);
		conPolyvertex180.push_back(vector<Point>());
		for (size_t j = 0; j < conPolyzero180[i].size(); j++)
		{
			if (max_y == conPolyzero180[i][j].y || max_x == conPolyzero180[i][j].x ||
				min_y == conPolyzero180[i][j].y || min_x == conPolyzero180[i][j].x)
				conPolyvertex180[i].push_back(conPolyzero180[i][j]);
		}	
		//reverse(conPolyzero180[i].begin(), conPolyzero180[i].end());
	}
	int is_min_index = 0;
	vector<int> nums; vector<int> data; int des[5];
	//在全排列之前先判断有没有一样的图形，如果有的话，在排列时去除
	vector<vector<Point>> conPolyvertex_All;//顶
	for (int i = 0; i < conPolyzero180.size(); i++)
		conPolyvertex_All.push_back(conPolyvertex[i]);
	for (int i = 0; i < conPolyzero180.size(); i++)
		conPolyvertex_All.push_back(conPolyvertex180[i]);
	map<int, vector<int>> matchResults;
	findAllGroupMatches(conPolyvertex_All, matchResults);
	for (size_t i = 0; i < conPoly.size(); i++){
		nums.push_back(i);
		data.push_back(i);
	}
	vector<vector<int>> have_changeData_list;
	vector<int> overturn_Nochange;
	for (map<int, vector<int>>::iterator it = matchResults.begin(); it != matchResults.end(); ++it) {
		vector<int> have_changeData;
		int baseIdx = it->first; // 基准组序号	
		if (baseIdx > conPolyvertex_All.size() / 2 - 1)
			continue;
		vector<int> matchIdxs = it->second; // 匹配组序号列表
		for (int &id : matchIdxs) {
			if (find(have_changeData.begin(), have_changeData.end(), id-1) == have_changeData.end()) {
				if (id != baseIdx) {//不同元素
					have_changeData.push_back(id - 1);
					if (find(have_changeData.begin(), have_changeData.end(), baseIdx - 1) == have_changeData.end()) {
						have_changeData.push_back(baseIdx - 1);
					}
				}
				else {//如果元素相同，说明图型反转后形状一样，则不考虑翻转
					if (find(overturn_Nochange.begin(), overturn_Nochange.end(), baseIdx) == overturn_Nochange.end()) {
						data.erase(data.begin() + baseIdx - 1 - overturn_Nochange.size());
						overturn_Nochange.push_back(baseIdx);
					}
				}
				//nums[id - 1] = baseIdx - 1;
				//data[id - 1] = baseIdx;
				//if (id == baseIdx)
				//	overturn_Nochange.push_back(id);
			}
		}
		if(!have_changeData.empty())	
			have_changeData_list.push_back(have_changeData);
		//const vector<Point>& baseGroup = conPolyzero180[baseIdx - 1];
	}
	vector<vector<int>> ans = permuteWithGroups(nums, have_changeData_list);//全排列
	vector<vector<int>> cns = getUniqueCombos(data, have_changeData_list);
	if (ans.size() == 120)
	{
		//cns = { {0},{1} ,{2} ,{3} ,{4} };
		cns.resize(3);
	}
	else if (ans.size() == 60)
	{
		//cns = { {0},{1} ,{2} ,{3} ,{4} };
		cns.resize(3);
		for (int i = 0; i < 3; i++)
		{
			vector<int> cns_;
			cns_.push_back(i);
			cns.push_back(cns_);
		}
	}
	vector<int> cns_;
	cns.push_back(cns_);
	//for (int i = 0; i < conPoly.size() + 1; i++)
	//	combinate(0, 0, conPoly.size(), i, data, des, cns);//组合排列combinate
	int end_y_allmin = 0xffffff; //所有排列方式的总y
	int end_y_min = 0xffffff;
	int d_index_cns = 0; //每种正反可能最小长度排列的索引
	int d_index_ans = 0; //每种排列可能最小长度排列的索引
	std::map<vector<pair<int, int>>, vector<vector<Point>>> results;//所有单元的最小长度结果
	cv::Mat target = dst.clone();
	string	jpg_name = "D:\\new\\foreground";
	jpg_name += ".jpg";
	//cv::imwrite(jpg_name,  target);
	int dst_y_end = y_start;
	bool collision = false;//碰撞 
	bool collision_big = false;//大碰撞 跳变大
	bool collision_ = false;//碰撞
	bool collision_less = false;//碰撞	
	bool collision_x_canput = false;
	bool LessLenth = false;//最短长度 
	int end_y = 0; //每种排列方式的总y
	int end_y_max = 0; //最大总y
	int end_y_d = 0;//每种排列方式的最后一个单元的y
	int end_y_backup = 0;
	int end_y_x_noUse = 0;//留白数量
	vector<int> y_pose_vector;
	int x; int x1;
	uchar blue; uchar blue2;
	size_t pxi; size_t pyi;
	int px_start; int px_end;
	int py_start; int py_end; 
	bool m_isrightmost = false;
	bool m_isrightCantPut = false;
	//if (dst_y_end > dst.rows)
	//	dst_y_end = dst.rows;
	//int dst_y_start = y_start;
	//if (dst_y_end > dst_y_start)
	//{
	//	Rect rect(0, dst_y_start, target.cols, target.rows - dst_y_start - 1);//dst_y_end - dst_y_start
	//	dst(rect).copyTo(target(rect));
	//}
	for (size_t d_ = 0; d_ < cns.size(); d_++)//所有正反可能
	{
		for (size_t is = 0; is < ans.size(); is++)//所有排列方式
		{
			if (*bool_run == false)
				return true;
			if (dst_y_end > dst.rows)
				dst_y_end = dst.rows;
			int dst_y_start = y_start;
			if (dst_y_end > dst_y_start)
			{
				Rect rect(0, dst_y_start, target.cols, target.rows - dst_y_start - 1);//dst_y_end - dst_y_start
				dst(rect).copyTo(target(rect));
			}
			//target.forEach<uchar>([](uchar& pixel, const int* position) -> void {        pixel = 0;      });
			collision = false;//碰撞 
			collision_x_canput = false;//碰撞 
			collision_big = false;//大碰撞 跳变大
			collision_ = false;//碰撞
			collision_less = false;//碰撞
			LessLenth = false;//最短长度 
			end_y = 0; //每种排列方式的总y
			end_y_d = 0;//每种排列方式的最后一个单元的y
			end_y_backup = 0;
			y_pose_vector.clear();
			if (progress) {
				*progress += 1.0f / ans.size() / cns.size()/ pthcount;
			}
			int Y_pic_last = 0;
			for (int l = 0; l < ans[is].size(); l++)//单个排列方式中的所有元素
			{
				int i = ans[is][l];
				std::vector<cv::Point> points;
				std::vector<cv::Point> pointsvertex;//顶点
				if (jpg_index == 1 && is == 0 && d_ == 5 /*&& !m_MoreFivepic*/ ) {
					int oo = 0;
					oo++;
				}
				for (int y = y_start; y < target.rows - boundRect[i].height; y = (collision_big ? (collision_ ? (collision_less ? y += 5 : y + 20) : y + 100) : y + 400))//start = 0;从上往下放
				{
					collision_x_canput = false;
					for (x = target.cols - boundRect[i].width; x >= 0; x -= 1)//从右往左放至碰撞
					{
						if ((x < min_width && x > 10 && ans.size() != 1 && min_width < dst.cols / 2) && min_width + boundRect[i].width)//不满足最小宽度
						{
							x1 = 0;
							x = 0;
						}
						//else if (x + boundRect[i].width > target.cols)	
						//{	
						//	x1 = 0;	
						//	x = 0;	
						//}	
						else
							x1 = x;
						cv::Point translation_vector(x1, y); // 将轮廓平移 (50, 50) 个像素
						collision = false;
						LessLenth = false;
						auto it = find(cns[d_].begin(), cns[d_].end(), i);
						if (it != cns[d_].end()){
							pointsvertex = conPolyvertex180[i];
							points = conPolyzero180[i]; 
						}
						else{
							points = conPolyzero[i];//conPolyvertex[i];// 
							pointsvertex = conPolyvertex[i];
						}
						for (cv::Point &point : pointsvertex) {	
							point += translation_vector;  // 每个点的x坐标增加dx
							{
								blue = target.at<uchar>(point.y, point.x); // 获取蓝色通道值
								if (blue)
								{
									if (point.x - 60 >= 0 && target.at<uchar>(point.y, point.x - 60) && x - 60 > min_width) {
										x -= 60;
									}
									collision = true;
								}
								break;
							}
						}
						if (collision == false)
							for (cv::Point &point : points) {
								point += translation_vector;  // 每个点的x坐标增加dx
								blue = target.at<uchar>(point.y, point.x); // 获取蓝色通道值
								if (blue)
								{
									if (point.x - 60 >= 0 && target.at<uchar>(point.y, point.x - 60) && x - 60 > min_width) {
										x -= 60;
									}
									collision = true;
									break;
								}
							}
						// 检查掩码是否重叠
						int px = points.rbegin()->x; int py = points.rbegin()->y;

						if (collision == false)
						{
							for (cv::Point &point : points) {
								if (py == point.y)//同行
								{
									px_start = min(px, point.x);
									px_end = max(px, point.x);
									for (pxi = px_start; pxi < px_end; pxi += 10)
									{
										blue = target.at<uchar>(point.y, pxi); // 获取蓝色通道值
										if (blue) {
											collision = true;
											break;
										}
									}
								}
								else if (px == point.x)//同列
								{
									py_start = min(py, point.y);
									py_end = max(py, point.y);
									for (pyi = py_start; pyi < py_end; pyi += 10)
									{
										blue = target.at<uchar>(pyi, point.x); // 获取蓝色通道值
										if (blue) {
											collision = true;
											break;
										}
									}
								}
								px = point.x;
								py = point.y;

								if (collision)
									break;
							}
						}
						if (collision == false)//没有碰撞就放回上一次按小的再一排一次
						{
							if (!collision_big)//如果没碰撞，且是第一次碰撞都没有触发
							{
								collision_big = true;
								if (y == 0)//如果y==0 最优排列录入
								{
									collision_big = false;
									LessLenth = true;
									std::vector<pair<int, int>> index_less;
									index_less.emplace_back(d_, is);
									results[index_less].push_back(points);
									y_pose_vector.push_back(y + boundRect[i].height);
								}
								else//如果y!=0  则y上移
								{
									y -= 400;
									if (y < 0)
										y = 0;
									x = 0;
									collision = true;
								}
							}
							else//如果没碰撞，且第一次碰撞触发过
							{
								if (!collision_)//如果没有第二次触发 则返回上一次的排版位置并减小减少距离，再次测试
								{
									collision_ = true;
									y -= 100;
									if (y < 0)
										y = 0;
									x = 0;
									collision = true;
								}
								else//如果触发过第二次碰撞 
								{
									if (!collision_less)//没有触发第三次碰撞 则再次返回上一次的排版位置并减小减少距离，再次测试
									{
										collision_less = true;
										y -= 20;
										if (y < 0)
											y = 0;
										x = 0;
										collision = true;
									}
									else//如果触发过第三次碰撞 最优排列录入
									{
										//如果图片排列在靠右侧，图片右侧最小宽度小于最小图片宽度，则放在最右侧，向下沿
										m_isrightmost = false;
										m_isrightCantPut = false;
										bool m_morelastY = false;
										int Y_lastend = y_start + max_y_all * 2;
										int Y_end_now = 0;
										int Y_start_now = 65535;
										for (cv::Point &point : points) {
											if (!m_isrightmost) {
												if (point.x > target.cols - 5)
													m_isrightmost = true;
												if (!m_isrightCantPut) {
													if (point.x > target.cols - min_width_ + 100)
														m_isrightCantPut = true;
												}
												if (point.y > Y_end_now)
													Y_end_now = point.y;
												if (point.y < Y_start_now)
													Y_start_now = point.y;
											}
										}
										int height_now = Y_end_now - Y_start_now;
										if (height_now + Y_pic_last > Y_lastend)
											m_morelastY = true;
										if (!m_isrightmost && m_isrightCantPut && bool_last_one == false && ans.size() != 1 && !m_morelastY && m_pic_canright.at(i))
										{
											for (cv::Point &point : points) {
												point -= translation_vector;  // 每个点的x坐标增加dx
											}
											int rightmost_x = target.cols - boundRect[i].width - 1;
											for (int rightmost_y = y; rightmost_y < target.rows - boundRect[i].height - 1; rightmost_y++)
											{
												uchar blue_rightmost = false;
												cv::Point translation_vector2(rightmost_x, rightmost_y); // 将轮廓平移 (50, 50) 个像素
												for (cv::Point &point : points) {
													point += translation_vector2;  // 每个点的x坐标增加dx
													blue = target.at<uchar>(point.y, point.x); // 获取蓝色通道值
													if (blue)
														blue_rightmost = true;
												}
												int px2 = points.rbegin()->x; int py2 = points.rbegin()->y;
												if (blue_rightmost == false){
													for (cv::Point &point : points) {
														if (py2 == point.y){
															px_start = min(px2, point.x);
															px_end = max(px2, point.x);
															for (pxi = px_start; pxi < px_end; pxi += 50){
																blue = target.at<uchar>(point.y, pxi); // 获取蓝色通道值
																if (blue) {
																	blue_rightmost = true;
																	break;
																}
															}
														}
														else if (px2 == point.x){
															int py_start = min(py2, point.y);
															int py_end = max(py2, point.y);
															for (pyi = py_start; pyi < py_end; pyi += 50){
																blue = target.at<uchar>(pyi, point.x); // 获取蓝色通道值
																if (blue) {
																	blue_rightmost = true;
																	break;
																}
															}
														}
														px2 = point.x;
														py2 = point.y;
														if (blue_rightmost)
															break;
													}
												}
												if (blue_rightmost)
												{
													for (cv::Point &point : points) {
														point -= translation_vector2;  // 每个点的x坐标增加dx
													}
												}
												else
													break;
											}
										}
										//else	
										{
											collision_big = false;
											collision_ = false;
											collision_less = false;
											LessLenth = true;
											std::vector<pair<int, int>> index_less;
											index_less.emplace_back(d_, is);
											y_pose_vector.push_back(y + boundRect[i].height);
											results[index_less].push_back(points);
										}
									}
								}
							}
						}
						if (LessLenth)
							break;
					}
					if (LessLenth)
					{
						end_y = max(end_y, (int)y + boundRect[i].height);
						end_y_d = (int)y + boundRect[i].height;
						break;
					}
				}
				if (LessLenth)	
				{	
					if (points.size() == 0)	
						break;	
					cv::fillPoly(target, points, cv::Scalar(255));
					for (cv::Point &point : points) {
						if (point.y > Y_pic_last)
							Y_pic_last = point.y;
					}
				}	
				//if (jpg_index == 1 && is == 0 && d_ == 5  /*&& !m_MoreFivepic  is>89&&is<100&&d_==6*/) {
				//	jpg_name = "D:\\new\\foreground";
				//	jpg_name += to_string(d_);
				//	jpg_name += "_" + to_string(is);
				//	jpg_name += ".jpg";
				//	cv::imwrite(jpg_name, target);
				//}
			}
			std::vector<pair<int, int>> index_less;
			index_less.emplace_back(d_, is);

			if (bool_last_one == false) {
				for (size_t ypv = 0; ypv < y_pose_vector.size(); ypv++)
				{
					if (y_pose_vector[ypv] != end_y)
						end_y_backup = max(end_y_backup, y_pose_vector[ypv]);
				}
			}
			else {
				end_y_backup = end_y;
 			}
			if (results[index_less].size() == conPoly.size())
			{
				int end_y_x_noUse_tmp = 0;//留白数量
				for (int x_ = 10; x_ < target.cols; x_ += 50)
				{
					int y_max = y_start + max_y_all * 3;// last_y_pose + max_y_all * 2;// end_y + 200;// + 100;
					if (y_max > foreground.rows - 1)
						y_max = foreground.rows - 1;
					int end_y_x_noUse_tmp2 = 0;//留白数量
					for (int y_ = y_max; y_ > y_start; y_ -= 50)//last_y_pose
					{
						blue = target.at<uchar>(y_, x_); // 获取蓝色通道值，判断谁的留白多
						if (!blue) {
							if (x_ > target.cols - min_width)//+ 200
							{
								blue = true;
								break;
							}
							for (int x_2 = x_; x_2 < x_ + min_width; x_2 += 200)//-400
							{
								blue2 = target.at<uchar>(y_, x_2); // 获取倒数第十行的蓝色通道值，判断谁的留白多
								if (blue2)
								{
									blue = true;
									break;
								}
							}
						}
						if (!blue)
							end_y_x_noUse_tmp2++;
						else {
							end_y_x_noUse_tmp += end_y_x_noUse_tmp2;
							break;
						}
					}
				}
				if (bool_last_one == true/* && m_MoreFivepic == false*/)
				{
					if(end_y_allmin > end_y_backup)
					{
						is_min_index = is;
						d_index_cns = d_;
						d_index_ans = is;
						end_y_min = (end_y + end_y_backup) / 2;
						end_y_allmin = end_y;
						end_y_x_noUse = end_y_x_noUse_tmp;
						//jpg_name = "D:\\new\\foreground";
						//jpg_name += to_string(d_);
						//jpg_name += "_" + to_string(is);
						//jpg_name += ".jpg";
						//cv::imwrite(jpg_name, target);
					}
				}
				else {
					if (end_y_x_noUse_tmp > end_y_x_noUse)//=
					{
						is_min_index = is;
						d_index_cns = d_;
						d_index_ans = is;
						end_y_min = (end_y + end_y_backup) / 2;
						end_y_allmin = end_y;
						end_y_x_noUse = end_y_x_noUse_tmp;
						//if (jpg_index == 1 /*&& !m_MoreFivepic&& d_ == 5 && is == 4   is>89&&is<100&&d_==6*/) 
						//{
						//	jpg_name = "D:\\new\\foreground";
						//	jpg_name += to_string(d_);
						//	jpg_name += "_" + to_string(is);
						//	jpg_name += ".jpg";
						//	cv::imwrite(jpg_name, target);
						//}
					}
					else {
						// 删除最后一个元素的步骤
						if (!results.empty()) {  // 必须先判断非空，避免访问空容器
							// 1. 获取最后一个元素的迭代器：prev(end()) 是 end() 的前一个位置
							auto last_iter = std::prev(results.end());
							// 2. 按迭代器删除
							results.erase(last_iter);
						}
					}
				}
			}
			dst_y_end = end_y;
			end_y_max = max(end_y_max, end_y);
			//if (y_start != 0) {
			//	cv::Rect roi(0, 0, foreground.cols, 12000);
			//	cv::imwrite(jpg_name, target);
			//}
		}
	}
	if (end_y_min == 0xffffff)
		return false;
	std::vector<cv::Point> points;
	int max_y_old = 0;//判断是否省料，如果没有省料则直接复制
	for (int i = 0; i < contours.size(); i++)
	{
		if (max_y_old < boundRect[i].height + boundRect[i].y + 20)
			max_y_old = boundRect[i].height + boundRect[i].y + 20;
	}
	if (*bool_run)
	{
		std::vector<pair<int, int>> index_less;
		index_less.emplace_back(d_index_cns, d_index_ans);
		std::map<vector<pair<int, int>>, vector<vector<Point>>>::iterator it;//所有单元的最小长度结果
		it = results.find(index_less);
		for (int l = 0; l < it->second.size(); l++)
		{
			int i = ans[d_index_ans][l];
			points = it->second.at(l);
			cv::fillPoly(dst, points, cv::Scalar(255));
			Rect rect = boundingRect(points);
			for (cv::Point &point : points) {
				cv::Point translation_vector(rect.x, rect.y); // 将轮廓平移 (50, 50) 个像素
				point -= translation_vector;
			}
			cv::Mat copy_src = img_element[i].clone();
			if (rect.x < 0)
				rect.x = 0;
			if (rect.y < 0)
				rect.y = 0;
			if (rect.width > copy_src.cols)
				rect.width = copy_src.cols;
			else if (rect.width < copy_src.cols)
				copy_src.cols = rect.width;
			if (rect.height > copy_src.rows)
				rect.height = copy_src.rows;
			else if (rect.height < copy_src.rows)
				copy_src.rows = rect.height;

			cv::Mat mask = cv::Mat::zeros(rect.size(), CV_8UC1);
			cv::fillPoly(mask, points, cv::Scalar(255));
			if (std::count(cns[d_index_cns].begin(), cns[d_index_cns].end(), i )) {
				cv::flip(copy_src, copy_src, -1);
			}
			copy_src.copyTo(foreground(rect), mask);
			//cv::Rect roi2(0, 0, target.cols, 50000);
			//cv::imwrite(jpg_name, foreground(roi2));
		}
		if (!m_MoreFivepic)
		{
			if (end_y_allmin < None_end_y)
				end_y_allmin = None_end_y;
			y_start = /*end_y_min*/end_y_allmin - 4500;
			if (y_start < 0)
				y_start = 0;
			last_y_pose = end_y_allmin;
		}
		else
			None_end_y = end_y_allmin;

	}
	return true;
}

bool getContours2(Mat & dst, vector<Mat> & img_element, vector<vector<Point>> & contours, int & y_start, cv::Mat & foreground, int & jpg_index, int bool_last_one, int & last_y_pose, double * progress, bool m_MoreFivepic, bool *bool_run, int & None_end_y, string *error_info, int paper_width)
{
	string	jpg_name = "kk";
	jpg_name += "_";
	jpg_name += ".jpg";
	vector<vector<Point>> conPoly(contours.size());//找轮廓点
	vector<vector<Point>> results;
	vector<Rect> boundRect(contours.size());//画外框
	int pic_maxY = 0;
	for (int i = 0; i < contours.size(); i++)
	{
		double peri = arcLength(contours[i], true);//计算闭合轮廓的周长
		approxPolyDP(contours[i], conPoly[i], 0.00001 * peri, true);//找到轮廓点
		//conPoly[i] = insertPointsWithFixedSpacing(conPoly[i]);
		for (cv::Point &point : conPoly[i]) {
			if (point.x < 0)
				point.x = 0;
			if (point.y < 0)
				point.y = 0;
			if (point.x > dst.cols - 1)
				point.x = dst.cols - 1;
			if (point.y > dst.rows - 1)
				point.y = dst.rows - 1;
			if (point.y > pic_maxY)
				pic_maxY = point.y;
		}
		boundRect[i] = boundingRect(conPoly[i]);
		boundRect[i].y += last_y_pose;
	}
	//先尝试整体上移
	cv::Mat target = dst.clone();
	bool collision_ = false;
	vector < vector<Point>> points_(contours.size());
	cv::Point translation_vector_last(0, last_y_pose); // 将轮廓平移 (50, 50) 个像素	
	int down_Y = 0;
	if (target.rows - pic_maxY > 20)
		down_Y = 20;
	else if(target.rows != pic_maxY)
		down_Y = target.rows - pic_maxY - 1;
	for (int y = -down_Y; y < 10000; y++)//向上
	{
		cv::Point translation_vector(0, y); // 将轮廓平移 (50, 50) 个像素		
		for (int i = 0; i < contours.size(); i++)
		{
			points_[i] = conPoly[i];
			for (cv::Point &point : points_[i]) {
				point += translation_vector_last;  // 上一张图位置
				point -= translation_vector;  // 每个点的x坐标增加dx
				if (point.y >= target.rows) {
					point.y = target.rows - 1;
					collision_ = true;
				}
				if (point.x >= target.cols) {
					point.x = target.cols - 1;
					collision_ = true;
				}
				if (point.y < 0) {
					point.y = 0;
					collision_ = true;
				}
				if (point.x < 0) {
					point.x = 0;
					collision_ = true;
				}
			}
			if (!collision_)
			{
				for (cv::Point &point : points_[i]) {
					uchar blue = target.at<uchar>(point.y, point.x); // 获取通道值
					if (blue) {
						collision_ = true;
						break;
					}
				}
				int px = points_[i].rbegin()->x; int py = points_[i].rbegin()->y;

				for (cv::Point &point : points_[i]) {
					if (py == point.y)//同行
					{
						int px_start = min(px, point.x);
						int px_end = max(px, point.x);
						for (size_t pxi = px_start; pxi < px_end; pxi += 10)
						{
							uchar blue = target.at<uchar>(point.y, pxi); // 获取蓝色通道值
							if (blue) {
								collision_ = true;
								break;
							}
						}
					}
					else if (px == point.x)//同列
					{
						int py_start = min(py, point.y);
						int py_end = max(py, point.y);
						for (size_t pyi = py_start; pyi < py_end; pyi += 10)
						{
							uchar blue = target.at<uchar>(pyi, point.x); // 获取蓝色通道值
							if (blue) {
								collision_ = true;
								break;
							}
						}
					}
					px = point.x;
					py = point.y;
				}
			}
		}
		//for (int i = 0; i < contours.size(); i++)
		//{
		//	cv::fillPoly(dst, points_[i], cv::Scalar(255));
		//	cv::imwrite(jpg_name, dst);
		//}
		if (collision_){
			cv::Point translation_vector_one(0, 11); // 将轮廓平移 (50, 50) 个像素		
			for (int i = 0; i < contours.size(); i++){
				for (cv::Point &point : points_[i]) {
					point -= translation_vector_last;  // 每个点的x坐标增加dx
					point += translation_vector_one;  // 每个点的x坐标增加dx
				}
			}
		}
		else{
			for (int i = 0; i < contours.size(); i++)
				for (cv::Point &point : points_[i]) {
					point += translation_vector;  // 每个点的x坐标增加dx
				}
		}
		if (collision_){
			if (y == -20)
				y = 0;
			else{
				for (int i = 0; i < contours.size(); i++)
					conPoly[i] = points_[i];
				break;
			}
		}
	}

	Mat dst_;
	int end_y_allmin = 0;
	for (int i = 0; i < contours.size(); i++)
	{
		dst_ = dst.clone();
		std::vector<cv::Point> points = conPoly[i];
		for (cv::Point &point : points) {
			point += translation_vector_last;  // 每个点的x坐标增加dx
		}
		for (int j = i; j < contours.size() - 1; j++)//除了要移动的图片其他设为背景
		{
			std::vector<cv::Point> points_ = conPoly[j + 1];
			for (cv::Point &point : points_) {
				point += translation_vector_last;  // 每个点的x坐标增加dx
			}
			cv::fillPoly(dst_, points_, cv::Scalar(255));
			//cv::imwrite(jpg_name, dst_);
		}
		target = dst_.clone();
		bool collision_x = false;//碰撞 
		bool collision_y = false;//碰撞 
		int x = 0; int y = 0;

		if (boundRect[i].x != 0)//向右
		{
			int minx = 20;
			if (boundRect[i].x < 20)
				minx = boundRect[i].x;
			for (x = -minx + 1; x < dst_.cols - boundRect[i].x - boundRect[i].width; x++)//向右
			{
				cv::Point translation_vector(x, 0); // 将轮廓平移 (50, 50) 个像素	
				for (cv::Point &point : points) {
					point += translation_vector;  // 每个点的x坐标增加dx
					if (point.y >= target.rows) {
						point.y = target.rows - 1;
						collision_x = true;
					}
					if (point.x >= target.cols) {
						point.x = target.cols - 1;
						collision_x = true;
					}
					if (point.y < 0) {
						point.y = 0;
						collision_x = true;
					}
					if (point.x < 0) {
						point.x = 0;
						collision_x = true;
					}
				}
				for (cv::Point &point : points)
				{
					uchar blue = target.at<uchar>(point.y, point.x); // 获取蓝色通道值
					if (blue) {
						collision_x = true;
						break;
					}
				}
				// 检查掩码是否重叠
				int px = points.rbegin()->x; int py = points.rbegin()->y;
				if (collision_x == false)
					for (cv::Point &point : points) {
						if (py == point.y)//同行
						{
							int px_start = min(px, point.x);
							int px_end = max(px, point.x);
							for (size_t pxi = px_start; pxi < px_end; pxi += 10)
							{
								uchar blue = target.at<uchar>(point.y, pxi); // 获取蓝色通道值
								if (blue) {
									collision_x = true;
									break;
								}
							}
						}
						else if (px == point.x)//同列
						{
							int py_start = min(py, point.y);
							int py_end = max(py, point.y);
							for (size_t pyi = py_start; pyi < py_end; pyi += 10)
							{
								uchar blue = target.at<uchar>(pyi, point.x); // 获取蓝色通道值
								if (blue) {
									collision_x = true;
									break;
								}
							}
						}
						px = point.x;
						py = point.y;
						if (collision_x)
							break;
					}
				for (cv::Point &point : points) {
					point -= translation_vector;  // 每个点的x坐标增加dx
				}
				if (collision_x)
					break;
			}
		}
		if (x != 0)
		{
			for (cv::Point &point : points) {
				cv::Point translation_vector(x - 1, 0); // 将轮廓平移 (50, 50) 个像素	
				point += translation_vector;  // 每个点的x坐标增加dx
			}
		}
		bool bool_find = false;
		int repeate_count = 20;
		std::vector<cv::Point> temp = points;
		for (size_t x = 0; x < repeate_count; x++)
		{
			uchar blue = 0;
			for (cv::Point &point : points) {
				if (point.x < 0) {
					blue = 255;
					break;
				}
				blue = dst.at<uchar>(point.y, point.x); // 获取蓝色通道值
				if (blue)
					break;
			}
			if (blue == 0) {
				bool_find = true;
				break;
			}
			cv::Point translation_vector(-3, 0); // 将轮廓平移 (50, 50) 个像素
			for (cv::Point &point : points) {
				point += translation_vector;  // 每个点的x坐标增加dx
			}
		}
		if (bool_find == false)
		{
			points = temp;
			for (size_t x = 0; x < repeate_count; x++)
			{
				uchar blue = 0;
				for (cv::Point &point : points) {
					if (point.x >= dst.cols) {
						blue = 255;
						break;
					}
					blue = dst.at<uchar>(point.y, point.x); // 获取蓝色通道值
					if (blue)
						break;
				}
				if (blue == 0) {
					bool_find = true;
					break;
				}
				cv::Point translation_vector(3, 0); // 将轮廓平移 (50, 50) 个像素
				for (cv::Point &point : points) {
					point += translation_vector;  // 每个点的x坐标增加dx
				}
			}
		}

		if (bool_find == false)
			points = temp;
		//if (boundRect[i].y > 100)
		{
			target = dst_.clone();
			for (y = 100; y < 10000; y++)//向上
			{
				cv::Point translation_vector(0, -y);
				for (cv::Point &point : points) {
					point += translation_vector;  // 每个点的x坐标增加dx
					if (point.y >= target.rows) {
						point.y = target.rows - 1;
						collision_y = true;
					}
					if (point.x >= target.cols) {
						point.x = target.cols - 1;
						collision_y = true;
					}
					if (point.y < 0) {
						point.y = 0;
						collision_y = true;
					}
					if (point.x < 0) {
						point.x = 0;
						collision_y = true;
					}
				}
				if (!collision_y)
					for (cv::Point &point : points) {
						if(point.x < 0 || point.y < 0){
							collision_y = true;
							break;
						}
						uchar blue = target.at<uchar>(point.y, point.x); // 获取蓝色通道值
						if (blue) {
							collision_y = true;
							break;
						}
					}
				// 检查掩码是否重叠
				int px = points.rbegin()->x; int py = points.rbegin()->y;

				if (collision_y == false)
					for (cv::Point &point : points) {
						if (py == point.y)//同行
						{
							int px_start = min(px, point.x);
							int px_end = max(px, point.x);
							for (size_t pxi = px_start; pxi < px_end; pxi += 10)
							{
								uchar blue = target.at<uchar>(point.y, pxi); // 获取蓝色通道值
								if (blue) {
									collision_y = true;
									break;
								}
							}
						}
						else if (px == point.x)//同列
						{
							int py_start = min(py, point.y);
							int py_end = max(py, point.y);
							for (size_t pyi = py_start; pyi < py_end; pyi += 10)
							{
								uchar blue = target.at<uchar>(pyi, point.x); // 获取蓝色通道值
								if (blue) {
									collision_y = true;
									break;
								}
							}
						}
						px = point.x;
						py = point.y;
						if (collision_y)
							break;
					}
				//if (i == 1){
				//	cv::fillPoly(target, points, cv::Scalar(255));
				//	//cv::imwrite(jpg_name, target);
				//}
				for (cv::Point &point : points) {
					point -= translation_vector;  // 每个点的x坐标增加dx
				}
				if (collision_y)
				{
					if (y == 100)
						y = 20;
					else if (y == 20)
						y = -20;
					else if (y == -20)
						y = 0;
					else
						break;
				}
			}
		}
		if (y != 0)
		{
			cv::Point translation_vector(0, 1 - y); // 将轮廓平移 (50, 50) 个像素	
			for (cv::Point &point : points) {
				point += translation_vector;  // 每个点的x坐标增加dx
			}
		}
		//if (bool_find == false)
		{
			points = temp;
			for (size_t y = 0; y < repeate_count; y++)
			{
				uchar blue = 0;
				for (cv::Point &point : points) {
					if (point.y < 0) {
						blue = 255;
						break;
					}
					blue = dst.at<uchar>(point.y, point.x); // 获取蓝色通道值
					if (blue)
						break;
				}
				if (blue == 0) {
					bool_find = true;
					break;
				}
				cv::Point translation_vector(0, -3); // 将轮廓平移 (50, 50) 个像素
				for (cv::Point &point : points) {
					point += translation_vector;  // 每个点的x坐标增加dx
				}
			}
		}

		//if (bool_find == false)
		{
			points = temp;
			for (size_t y = 0; y < repeate_count; y++)
			{
				uchar blue = 0;
				for (cv::Point &point : points) {
					if (point.y > dst.rows) {
						blue = 255;
						break;
					}
					blue = dst.at<uchar>(point.y, point.x); // 获取蓝色通道值
					if (blue)
						break;
				}
				if (blue == 0) {
					bool_find = true;
					break;
				}
				cv::Point translation_vector(0, 3); // 将轮廓平移 (50, 50) 个像素
				for (cv::Point &point : points) {
					point += translation_vector;  // 每个点的x坐标增加dx
				}
			}
		}
		cv::fillPoly(target, points, cv::Scalar(255));
		cv::fillPoly(dst, points, cv::Scalar(255));
		//cv::imwrite(jpg_name, dst);
		Rect rect = boundingRect(points);
		if (rect.x < 0)
			rect.x = 0;
		if (rect.y < 0)
			rect.y = 0;
		if (rect.width > foreground.cols - rect.x)
			rect.width = foreground.cols - rect.x;
		if (rect.height > foreground.rows - rect.y)
			rect.height = foreground.rows - rect.y;
		cv::Mat copy_src = img_element[i].clone();
		if (rect.width > copy_src.cols)
			rect.width = copy_src.cols;
		else if (rect.width < copy_src.cols)
			copy_src.cols = rect.width;
		if (rect.height > copy_src.rows)
			rect.height = copy_src.rows;
		else if (rect.height < copy_src.rows)
			copy_src.rows = rect.height;
		cv::Mat mask = cv::Mat::zeros(rect.size(), CV_8UC1);
		cv::Point translation_vector(rect.x, rect.y); // 将轮廓平移 (50, 50) 个像素	
		for (cv::Point &point : points) {
			point -= translation_vector;  // 每个点的x坐标增加dx
		}
		cv::fillPoly(mask, points, cv::Scalar(255));
		//cv::imwrite(jpg_name, mask);
		copy_src.copyTo(foreground(rect), mask);
		//cv::imwrite(jpg_name, foreground);
		end_y_allmin = max(end_y_allmin, rect.y + rect.height);
	}
	if (end_y_allmin < None_end_y)
		end_y_allmin = None_end_y;
	y_start = /*end_y_min*/end_y_allmin - 1500;
	last_y_pose = end_y_allmin;
	if (y_start < 0)
		y_start = 0;
	//*progress += 1;
	return true;
}

vector<string> split(const string &s, const string &seperator) {
	vector<string> result;
	typedef string::size_type string_size;
	string_size i = 0;
	while (i != s.size()) {
		//找到字符串中首个不等于分隔符的字母；
		int flag = 0;
		while (i != s.size() && flag == 0) {
			flag = 1;
			for (string_size x = 0; x < seperator.size(); ++x)
				if (s[i] == seperator[x]) {
					++i;
					flag = 0;
					break;
				}
		}
		//找到又一个分隔符，将两个分隔符之间的字符串取出；
		flag = 0;
		string_size j = i;
		while (j != s.size() && flag == 0) {
			for (string_size x = 0; x < seperator.size(); ++x)
				if (s[j] == seperator[x]) {
					flag = 1;
					break;
				}
			if (flag == 0)
				++j;
		}
		if (i != j) {
			result.push_back(s.substr(i, j - i));
			i = j;
		}
	}
	return result;
}

#define ERROR 65576
//度数转换
double DegreeTrans(double theta)
{
	double res = theta / CV_PI * 180;
	return res;
}
//逆时针旋转图像degree角度（原尺寸）
void rotateImage(Mat src, Mat& img_rotate, double degree)
{
	cv::Size dst_sz(src.cols, src.rows);        //保持图像大小一致
	cv::Point2f center(static_cast<float>(src.cols / 2.), static_cast<float>(src.rows / 2.));       //指定旋转中心
	cv::Mat rot_mat = cv::getRotationMatrix2D(center, degree, 1.0); //获取旋转矩阵（2x3矩阵）
	cv::warpAffine(src, img_rotate, rot_mat, dst_sz);
}

int resize(double scale = 0.9) {
	// 加载原始图像
	cv::Mat src = cv::imread("tt.jpg");
	// 检查图像是否成功加载
	if (src.empty()) {
		std::cerr << "Error: Loading image" << std::endl;
		return -1;
	}
	// 创建一个Mat对象用于存储缩放后的图像
	cv::Mat dst;
	// 缩放因子

	// 缩放图像
	cv::resize(src, dst, cv::Size(), scale, scale, cv::INTER_LANCZOS4);
	// 保存缩放后的图像
	cv::imwrite("scaled_image.jpg", dst);
	return 0;
}

// 封装的函数，用于上下各填充一行空白像素
cv::Mat padImageTopBottom(const cv::Mat& inputImage, int element_interval, const cv::Scalar& fillValue = cv::Scalar(255, 255, 255)) {
	int top = element_interval;				// 顶部填充1行
	int bottom = element_interval;			// 底部填充1行
	int left = element_interval;			// 左侧不填充
	int right = element_interval;			// 右侧不填充
	int borderType = cv::BORDER_CONSTANT;	// 常量填充

	cv::Mat paddedImage;
	cv::copyMakeBorder(inputImage, paddedImage, top, bottom, left, right, borderType, fillValue);
	return paddedImage;
}

bool fetchContours(Mat & img, vector<vector<Point>> & contours, vector<Mat> & img_element, int & jpg_index, int element_interval, double pixMax_width, vector<bool> bool_Selest, vector<int> colors)//提取轮廓 图片 寻遍
{
	Mat imgGray, imgBlur, imgCanny, imgDil;//灰色，模糊，精明， ，imgErode腐蚀
	cvtColor(img, imgGray, COLOR_BGR2GRAY);					//imshow("Image", img);			图像中不同的色彩空间进行转换 单通道灰度图
	GaussianBlur(imgGray, imgBlur, Size(3, 3), 3, 0);		//imshow("Image", img);			高斯滤波 
	Canny(imgBlur, imgCanny, 75, 100);						//imshow("Image", img);			边缘检测	
	Mat kernel = getStructuringElement(MORPH_RECT, Size(3, 3));	//原本(3,3)因为有相近图片改成(3,2)		返回进一步传递给可以erosion（腐蚀）、dilate（膨胀）或morphologyEx的结构元素，用于图形学操作。
	dilate(imgCanny, imgDil, kernel);						//imshow("Image", img);			膨胀函数
	//cv::imwrite("output_image22.jpg", imgDil);			//								输出到指定文件
	//findContours(imgDil, contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);//找轮廓
	double ratio = pixMax_width / img.cols - 1;  //比例
	vector<vector<cv::Point>> contours_temp;//找轮廓点
	vector<vector<cv::Point>> contours_temp_;//找轮廓点
	vector<Vec4i>  hierarchy;
	findContours(imgDil, contours_temp_, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);//找轮廓
	for (int i = 0; i < contours_temp_.size(); i++)//识别出来的图片面积小于10000时忽略
	{
		vector<cv::Point> conPoly;
		double peri = arcLength(contours_temp_[i], true);//计算闭合轮廓的周长
		approxPolyDP(contours_temp_[i], conPoly, 0.00001 * peri, true);//找到轮廓点
		Rect rect1;
		rect1 = boundingRect(conPoly);
		if (rect1.width * rect1.height > 10000)
		{
			contours_temp.push_back(contours_temp_[i]);
		}
	}

	for (int i = 0; i < contours_temp.size(); i++)
	{
		vector<cv::Point> conPoly;
		double peri = arcLength(contours_temp[i], true);//计算闭合轮廓的周长
		approxPolyDP(contours_temp[i], conPoly, 0.00001 * peri, true);//找到轮廓点
		Rect rect1;
		rect1 = boundingRect(conPoly);
		//if (rect1.area() > 100000)
		{
			contours.push_back(contours_temp[i]);
			//cv::Rect rect(insertpose[is_min_index*conPoly.size() + i].x, insertpose[is_min_index*conPoly.size() + i].y, boundRect[ans[is_min_index][i]].width, boundRect[ans[is_min_index][i]].height);
			//cv::Mat copy_dst(rect1.size(), CV_8UC3);
			cv::Mat copy_dst;
			if (picType == 1)
			{
				cv::Mat copy_dst2(rect1.size(), CV_8UC3);
				copy_dst = copy_dst2;
				copy_dst.setTo(cv::Scalar(255, 255, 255));//全白
			}
			else if (picType == 2)
			{
				cv::Mat copy_dst2(rect1.size(), CV_8UC4);
				copy_dst = copy_dst2;
				copy_dst.setTo(cv::Scalar(255, 255, 255, 255));//全白
			}
			else  return false;
			// 方案一：使用setTo方法（最优）
			vector<cv::Point> points_element;
			cv::Point translation_vector(rect1.x, rect1.y); // 将轮廓平移 (50, 50) 个像素
			for (cv::Point &point : contours_temp[i]) {
				point -= translation_vector;
			}
			cv::Mat mask = cv::Mat::zeros(Rect(0, 0, rect1.width, rect1.height).size(), CV_8UC1);//创建全零矩阵
			cv::fillPoly(mask, contours_temp[i], cv::Scalar(255));
			img(rect1).clone().copyTo(copy_dst, mask);
			contours[i] = expandContour(contours[i], element_interval);
			//translation_vector = cv::Point(rect1.x*ratio, 0); // 将轮廓平移 (50, 50) 个像素
			//if (rect1.x < 10)
			//	rect1.x = 10 - rect1.x;
			//if (rect1.y < 10)
			//	rect1.y = 10 - rect1.y;
			translation_vector = cv::Point(rect1.x, rect1.y); // 将轮廓平移 (50, 50) 个像素
			for (cv::Point &point : contours[i]) {
				point += translation_vector;
			}
			if (picType == 1)
				img_element.push_back(padImageTopBottom(copy_dst, element_interval, cv::Scalar(255, 255, 255)));
			else if (picType == 2)
				img_element.push_back(padImageTopBottom(copy_dst, element_interval, cv::Scalar(255, 255, 255, 255)));
			translation_vector.x = element_interval;
			translation_vector.y = element_interval;

			for (cv::Point &point : contours_temp[i]) {
				point += translation_vector;
			}
			if (bool_Selest.at(3))//添加分割线
			{
				cv::Scalar color_arrary[8];
				color_arrary[0] = cv::Scalar(0, 0, 255);//rgb,这里是bgr
				color_arrary[1] = cv::Scalar(0, 255, 85);
				color_arrary[2] = cv::Scalar(255, 85, 0);
				color_arrary[3] = cv::Scalar(0, 255, 255);
				color_arrary[4] = cv::Scalar(0, 0, 0);
				color_arrary[5] = cv::Scalar(255, 255, 255);
				color_arrary[6] = cv::Scalar(203, 192, 255);
				color_arrary[7] = cv::Scalar(128, 0, 128);
				std::vector<cv::Point> expanded = expand_polygon(contours_temp[i], element_interval * 4 / 5);
				// 绘制虚线
				if (bool_Selest.at(4))//隔一个
				{
					if (jpg_index % 2 == 0)
					{
						int index = jpg_index / 2;
						index = index%10;
						drawDashedPoly(img_element[i], expanded, color_arrary[colors.at(index)], 2, element_interval * 2, element_interval * 2);  // 蓝色虚线
					}
				}
				else {
					int index = jpg_index;
					index = index % 10;
					drawDashedPoly(img_element[i], expanded, color_arrary[colors[index]], 2, element_interval * 2, element_interval * 2);  // 蓝色虚线
				}
			}
		}
	}
	if (contours.size() > 5)
		return false;
	return true;
}

// 函数用于垂直拼接多张图像
cv::Mat vconcatImages(const std::vector<cv::Mat>& images) {
	if (images.empty()) {
		return cv::Mat();
	}
	// 找到所有图像中的最大宽度
	int maxWidth = 0;
	for (const auto& image : images) {
		if (image.cols > maxWidth) {
			maxWidth = image.cols;
		}
	}
	// 存储调整大小后的图像
	std::vector<cv::Mat> resizedImages;
	for (const auto& image : images) {
		cv::Mat resized;
		if (image.cols < maxWidth) {
			// 如果图像宽度小于最大宽度，进行填充
			if (picType == 1)
				cv::copyMakeBorder(image, resized, 0, 0, 0, maxWidth - image.cols, cv::BORDER_CONSTANT, cv::Scalar(255, 255, 255));
			else if (picType == 2)
				cv::copyMakeBorder(image, resized, 0, 0, 0, maxWidth - image.cols, cv::BORDER_CONSTANT, cv::Scalar(255, 255, 255, 255));
		}
		else {
			resized = image.clone();
		}
		resizedImages.push_back(resized);
	}
	// 拼接调整大小后的图像
	cv::Mat result;
	cv::vconcat(resizedImages, result);
	return result;
}
//寻边
bool PatrolBorder(Mat & img, Mat & dst, int & y_start, int & index, cv::Mat & foreground, int & end_y, bool bool_last_one, double * progress, 
		int element_interval, bool *bool_run, string *error_info, int img_width, int paper_width, vector<bool> bool_Selest, vector<int> colors)
{
	if (*bool_run == false)
		return false;
	vector<vector<Point>>  contours;
	vector<Mat> img_element;
	bool sort_succeed = false;
	int None_end_y = 0;
	if (fetchContours(img, contours, img_element, index, element_interval, img_width, bool_Selest, colors) == false)
	{
		vector<int> arr;//从大到小排序
		vector<int> arr_ymin;//ymin从小到大排序
		vector<int> index_;//从大到小排序
		vector<int> img_width_;
		vector<int> img_height_;
		for (int i = 0; i < contours.size(); i++)
		{
			Point point;
			int max_x = 0; int max_y = 0; 
			int min_x = 10000; int min_y = 10000;
			int width = 0; int height = 0;
			for (cv::Point &point : contours[i]) {
				if (point.x < min_x)
					min_x = point.x;
				if (point.y < min_y)
					min_y = point.y;
				if (point.x > max_x)
					max_x = point.x;
				if (point.y > max_y)
					max_y = point.y;
			}
			arr_ymin.push_back(min_y);
			for (cv::Point &point : contours[i]) {
				if (point.x - min_x > width)
					width = point.x - min_x;
				if (point.y - min_y > height)
					height = point.y - min_y;
			}
			img_width_.push_back(width);
			img_height_.push_back(height);
			arr.push_back(max_y);//按面积max_y*max_x 按位置左右max_x 上下min_y
			index_.push_back(i);
		}
		vector<vector<Point>>  contours_;
		vector<Mat> img_element_;
		vector<int> index_xiangsi;//新生成的顺序
		vector<int> index_xiangsi_out;//已经排过的数据
		//贪心排列，先按宽度从大到校，选择最大的裁片和纸张剩余宽度能放下的四个图片，如果图片少于四张，再想宽度以上补余，直到所有裁片排完
		sort(index_.begin(), index_.end(), [img_height_, index_](int a, int b) {//排列
			//return img_width_[index_[a]] > img_width_[index_[b]];//	return arr_y[index_[a]] < arr_y[index_[b]];
			return img_height_[index_[a]] > img_height_[index_[b]];//	return arr_y[index_[a]] < arr_y[index_[b]];
		});
		for (int i = 0; i < index_.size(); i++)
		{
			auto it_i = find(index_xiangsi_out.begin(), index_xiangsi_out.end(), index_[i]);
			if (it_i == index_xiangsi_out.end())
				index_xiangsi.push_back(index_[i]);
			int k = 0; int width_now = img.cols - img_width_[index_[i]];
			for (int j = i + 1; j < index_.size(); j++)
			{
				it_i = find(index_xiangsi_out.begin(), index_xiangsi_out.end(), index_[j]);
				if (img_width_[index_[j]] <= width_now && it_i == index_xiangsi_out.end() && k < 4)
				{
					index_xiangsi.push_back(index_[j]);
					index_xiangsi_out.push_back(index_[j]);
					k++;
					width_now -= img_width_[index_[j]];
				}
			}
			while (k < 4 && index_.size() != index_xiangsi.size())//剩余图片少于四张
			{
				for (int j = index_.size() - 1; j > i && k < 4; j--)
				{
					it_i = find(index_xiangsi_out.begin(), index_xiangsi_out.end(), index_[j]);
					if (it_i == index_xiangsi_out.end())
					{
						index_xiangsi.push_back(index_[j]);
						index_xiangsi_out.push_back(index_[j]);
						k++;
					}
				}
			}
		}
		////如果图形相似，则放在一起排列
		//sort(index_.begin(), index_.end(), [arr, index_](int a, int b) {//排列
		//	return arr[index_[a]] < arr[index_[b]];//	return arr_y[index_[a]] < arr_y[index_[b]];
		//});
		//for (int  i = 0; i < index_.size(); i++)
		//{
		//	auto it_i = find(index_xiangsi_out.begin(), index_xiangsi_out.end(), index_[i]);
		//	if(it_i == index_xiangsi_out.end())
		//		index_xiangsi.push_back(index_[i]);
		//	for (int j = i+1; j < index_.size(); j++)	
		//	{	
		//		it_i = find(index_xiangsi_out.begin(), index_xiangsi_out.end(), index_[j]);
		//		if (img_width_[index_[i]] == img_width_[index_[j]] && img_width_[index_[i]] == img_width_[index_[j]] && it_i == index_xiangsi_out.end())
		//		{	
		//			index_xiangsi.push_back(index_[j]);
		//			index_xiangsi_out.push_back(index_[j]);
		//		}
		//	}	
		//}
		vector<int>().swap(index_);
		vector<int>().swap(index_xiangsi_out);
		//不排序，除了最左边只往上移和左移
		if (0)		//if (paper_width >= img_width)
		{
			for (int i = 0; i < contours.size(); i++)
			{
				contours_.push_back(contours[index_xiangsi[i]]);
				img_element_.push_back(img_element[index_xiangsi[i]]);
			}
			img_element = img_element_;
			contours = contours_;
			*progress += 1;
			sort_succeed = getContours2(dst, img_element, contours, y_start, foreground, index, bool_last_one, end_y, progress, false, bool_run, None_end_y, error_info, paper_width);
		}
		else {
			//从上到下，以五个一排序，超过五个再排一次
			//Mat dst_next;
			//dst.copyTo(dst_next);
			//cv::Mat foreground_next;
			//foreground.copyTo(foreground_next);
			for (int i = 0; i < ((contours.size()-1) / 5) + 1; i++)//五个一次
			{
				bool None_end = false;
				vector<vector<Point>>  contours_next;
				vector<Mat>  img_element_next;
				for (int j = 0; j < 5; j++)
				{
					if (contours.size() > 5 * i + j)
					{
						contours_next.push_back(contours[index_xiangsi[5 * i + j]]);
						img_element_next.push_back(img_element[index_xiangsi[5 * i + j]]);
					}
				}
				if (i != (contours.size() - 1) / 5)//不是最后一次
					None_end = true;
				sort_succeed = getContours(dst/*_next*/, img_element_next, contours_next, y_start, foreground/*_next*/, index, bool_last_one, end_y, progress, (contours.size()-1) / 5 +1, None_end, bool_run, None_end_y, error_info, paper_width);
				//if (!sort_succeed)
				//	break;
				///*if (sort_succeed && */if(!None_end)
				//{
					//string	jpg_name = "kk";
					//cv::imwrite(jpg_name + "1.jpg", dst);
					//cv::imwrite(jpg_name + "2.jpg", foreground);
					//cv::Rect roi(0, end_y, dst.cols, dst.rows);//foreground.cols
					//cv::Mat mask = cv::Mat::zeros(roi.size(), CV_8UC1);
					//dst_next.setTo(cv::Scalar(255, 255, 255), mask);
					//foreground_next.setTo(cv::Scalar(255, 255, 255), mask);
					//cv::imwrite(jpg_name + "3.jpg", dst);
					//cv::imwrite(jpg_name + "4.jpg", foreground);
					//foreground_next.copyTo(foreground);
					//dst_next.copyTo(dst);
					//cv::imwrite(jpg_name + "5.jpg", dst);
					//cv::imwrite(jpg_name + "6.jpg", foreground);
					//return true;
				//}
			}
			return true;
		}
	}
	else
		sort_succeed = getContours(dst, img_element, contours, y_start, foreground, index, bool_last_one, end_y, progress, 1, false, bool_run, None_end_y, error_info, paper_width);
	if (!sort_succeed && *bool_run)
	{
		//*progress -= 1;
		getContours2(dst, img_element, contours, y_start, foreground, index, bool_last_one, end_y, progress, false, bool_run, None_end_y, error_info, paper_width);
	}
	return true;
}

void JpgProcess(string output_path, string output_path_Child, vector<std::string> path_vector, int start_index, int count, 
		int jpg_index, bool * bool_end_status,double * total_height, double * total_end, double * progress, bool *bool_run, int * XResolution, int element_interval,
		int paper_width ,string *error_info, double * save_height, int Ratio_less, vector<bool> bool_Selest, vector<int> colors) {
	// 获取当前线程 TID（Windows 平台，格式为 0xXXXX）
	DWORD tid = GetCurrentThreadId();
	// 打印线程信息（控制台 + VS 输出窗口）
	std::cout << ("[%s] 线程启动，TID: 0x%X\n", to_string(start_index), tid);
	std::vector<cv::Mat> img;
	int height = 0;
	int height_ = 0;
	int dpi_x = 0;
	int max_width = 0;
	if (*bool_run == false)
		return;
	for (size_t i = 0; i < count; i++)
	{
		cv::Mat src;
		if ((path_vector)[i + start_index].find(".tif") == -1)
			src = imread((path_vector)[i + start_index]);	// 读取 JPEG 图像
		else {
			src = readCMYKImage((path_vector)[i + start_index]);
		}
		//img.push_back(src);
		img.push_back(padImageTopBottom(src, element_interval));
	}
	for (size_t x = 0; x < count; x++)
	{
		getImageDPI((path_vector)[x + start_index].c_str(), dpi_x);
		if (dpi_x == 0)
			dpi_x = Ratio_less;
		if(dpi_x != *XResolution && dpi_x != 0)
			img[x] = resize(img[x], (double)*XResolution / (double)dpi_x);
		max_width = max(max_width, img[x].size().width+4);//50避免添加图片间隔后过窄	+4给图片加粗
	}
	//string str = "最大max_width:" + to_string(max_width);
	//*error_info = str;
	paper_width = paper_width * *XResolution / 25.4;// 
	//if (paper_width > max_width){
	//	max_width = paper_width;
	//}
	//else if(paper_width != 0 && paper_width < max_width){
	//	int PaperWidth = max_width / (*XResolution) * 25.4 ;
	//	string str = "生成图片target";
	//	str += to_string(start_index + count);
	//	str += "的图片宽度大于输入纸张宽度，最小纸张宽度为";
	//	str += std::to_string(PaperWidth);
	//	str += "mm。";
	//	*error_info = str;
	//}
	for (size_t i = 0; i < count; i++)
	{
		if (paper_width != 0 && img[i].cols >= paper_width) {
			height += img[i].rows * img[i].cols / paper_width;// maxWidth;
			if(img[i].cols > paper_width)
				height_ += img[i].rows; //= 65000;
			else 
				height_ += img[i].rows * img[i].cols / max_width;
		}
		else {
			height += img[i].rows * img[i].cols / max_width;
			height_ += img[i].rows; //= 65000;
		}
	}
	if (paper_width != 0)
		max_width = paper_width;
	height_ += 5000;
	int y_start = 0;
	cv::Mat target(height_, max_width, CV_8UC1);//height//paper_width
	cv::Mat foreground;
	if (picType == 1)
	{
		target.setTo(cv::Scalar(0, 0, 0));//测试
		cv::Mat copy_dst(height_, max_width, CV_8UC3, cv::Scalar(0, 0, 0));//height
		foreground = copy_dst;
		// 方案一：使用setTo方法（最优）
		foreground.setTo(cv::Scalar(255, 255, 255));
	}
	else if (picType == 2)
	{
		target.setTo(cv::Scalar(0));
		cv::Mat copy_dst(height_, max_width, CV_8UC4);//height
		foreground = copy_dst;
		// 方案一：使用setTo方法（最优）
		foreground.setTo(cv::Scalar(255, 255, 255, 255));
	}
	else return;
	int end_y = 0;
	bool success = true;
	for (int i = 0; i < count; i++)
		success = PatrolBorder(img[i], target, y_start, i, foreground, end_y, i == count - 1, progress, // || i == count - 2
			element_interval, bool_run, error_info, max_width, paper_width, bool_Selest, colors);
	if (success == false)
	{
		//foreground = vconcatImages(img);
	}
	else
	{
		if (end_y <= -1 || end_y > 65535)
			end_y = height;
		cv::Rect roi;
		if (paper_width != 0)
			roi = Rect(2, 0, max_width-4, end_y);//foreground.cols
		else
			roi = Rect(element_interval+2, 0, max_width - (element_interval+2) * 2, end_y);//foreground.cols
		// 提取剪切区域
		foreground = foreground(roi);
	}
	if (end_y == -1)
		end_y = height;
	if (*bool_run == false)
		return;

	//路径
	string	jpg_name = output_path + output_path_Child;
	string delim = "\\";
	std::vector<std::string> elems = split(jpg_name, delim);
	string output_path_;
	for (int i = 0; i < elems.size(); i++)
	{
		output_path_ += elems.at(i) + "\\";
		EnsureFoldExistA(output_path_, true);
	}
	//添加文字
	if (*bool_run && bool_Selest.at(2))
	{
		string str;
		if (output_path_Child.size() > 2)
			str = split(output_path_Child, delim).at(0);
		else
		{
			string str2 = split(output_path, delim).at(elems.size() - 1);
			str = split(str2, "(").at(0);
		}
		DrawTitle(str, foreground.cols, start_index);
		cv::Mat title_image = cv::imread("./title_imp/"+std::to_string(start_index) +".bmp");
		if (!str.empty() && picType == 1 && title_image.rows != 0) {
			std::vector<cv::Mat> img_vector;
			img_vector.push_back(foreground);
			img_vector.push_back(title_image);
			foreground = vconcatImages(img_vector);
		}
	}

	double saveheight = (height - end_y) / *XResolution * 25.4 / 1000;
	std::cout << start_index << " 长度(m): " << (double)(height) / *XResolution * 25.4 / 1000 << " 省料(m)：" << saveheight << std::endl;
	*total_height += (height) / *XResolution * 25.4 / 1000;
	*total_end += end_y / *XResolution * 25.4 / 1000;
	if (saveheight < 0)
		saveheight = 0;
	*save_height += saveheight;//
	//*save_height = (*total_height - *total_end);//
	//if (*save_height < 0)
	//	*save_height = 0;
	if (*bool_run && bool_Selest.at(1)) {
		// 写入内容（末尾自动添加换行符 '\n'，避免内容挤在一行）
		//输出文件夹省料统计
		// 打开模式：ios::app（追加）+ ios::out（写入），二进制模式避免换行符转换（可选）
		// 检查文件是否成功打开（避免路径错误、权限问题等）
		// 使用宽字符路径打开，解决 Windows 上中文文件名乱码问题
		std::ofstream file(Utf8ToWstring(jpg_name + "省料统计.txt"), std::ios::out | std::ios::app);
		if (!file.is_open()) {
			std::cerr << "错误：无法打开文件！路径：" << jpg_name + "省料统计.txt" << std::endl;
		}
		file << " 图片target" + to_string(start_index + count) + " 长度(m): " << (double)(height) / *XResolution * 25.4 / 1000
			<< " 省料(m)：" << saveheight << std::endl;
		// 关闭文件（ofstream 析构时会自动关闭，但手动关闭更规范）
		file.close();
	}
	jpg_name += "target";
	jpg_name += to_string(start_index + count);
	//保存图片
	if (*bool_run)
	{
		if (picType == 1)
		{
			jpg_name += ".jpg";
			cv::imwrite(jpg_name, foreground);
			changedpi(jpg_name.c_str(), *XResolution, *XResolution);//测试
		}
		else if (picType == 2)
		{
			jpg_name += ".tif";
			saveAsCMYKTIFF(foreground, jpg_name, (float)(*XResolution));
		}
	}

	if (*bool_run)
		*bool_end_status = true;
}

// 新函数，用于获取所有文件夹中的 JPG 文件并生成 map
void getAllJpgFilesInFolders(const std::string& rootDir, std::map<std::string, std::vector<std::string>>& folderJpgMap, string& cFileName) {
	vector<string> files;
	//文件句柄
	intptr_t hFile = 0;
	//文件信息
	_finddata_t fileinfo;
	std::vector<std::string> jpgFiles;
	string p = rootDir + "\\*.*";
	if ((hFile = _findfirst(p.c_str(), &fileinfo)) != -1) {
		do {
			if ((fileinfo.attrib & _A_SUBDIR)) { //比较文件类型是否是文件夹
				if (strcmp(fileinfo.name, ".") != 0 && strcmp(fileinfo.name, "..") != 0) {
					files.push_back(p.assign(rootDir).append(fileinfo.name).append("\\"));
					cFileName = p.assign(cFileName).append(fileinfo.name).append("\\");
					//递归搜索
					getAllJpgFilesInFolders(p.assign(rootDir).append(fileinfo.name).append("\\"), folderJpgMap, cFileName);
				}
			}
			else {
				//files.push_back(p.assign(rootDir).append("\\").append(fileinfo.name));
				string folderPath = rootDir;// +"\\";
				if (picType != 2)
				{
					jpgFiles = listFiles(folderPath);
					if (!jpgFiles.empty()) {
						folderJpgMap[cFileName] = jpgFiles;
						picType = 1;
					}
				}
				if (picType != 1)
				{
					jpgFiles = listTiffFiles(folderPath);
					if (!jpgFiles.empty()) {
						folderJpgMap[cFileName] = jpgFiles;
						picType = 2;
					}
				}
				if (!jpgFiles.empty())
					break;
				//jpgFiles.push_back(p.assign(rootDir).append("\\").append(fileinfo.name));
			}
			//if (jpgFiles.size() != 0)
			//{
			//	folderJpgMap[cFileName] = jpgFiles;
			//}
		} while (_findnext(hFile, &fileinfo) == 0); //寻找下一个，成功返回0，否则-1
		int pos = cFileName.rfind("\\");
		if (pos != -1)
			cFileName.erase(pos);
		pos = cFileName.rfind("\\");
		if (pos != -1)
			cFileName.erase(pos + 1);
		else
			cFileName.clear();
		_findclose(hFile);
	}
}
//#pragma comment(lib,"jpeg-static.lib")
// 最近邻插值缩放函数
// 最近邻插值缩放函数
void nearestNeighborResize(unsigned char *src, unsigned char *dst, int srcWidth, int srcHeight, int dstWidth, int dstHeight, int numChannels) {
	if (src == NULL || dst == NULL || srcWidth <= 0 || srcHeight <= 0 || dstWidth <= 0 || dstHeight <= 0 || numChannels <= 0) {
		fprintf(stderr, "Invalid input parameters for nearestNeighborResize.\n");
		return;
	}
	for (int y = 0; y < dstHeight; y++) {
		for (int x = 0; x < dstWidth; x++) {
			int srcX = (x * srcWidth) / dstWidth;
			int srcY = (y * srcHeight) / dstHeight;
			if (srcX < 0) srcX = 0;
			if (srcX >= srcWidth) srcX = srcWidth - 1;
			if (srcY < 0) srcY = 0;
			if (srcY >= srcHeight) srcY = srcHeight - 1;
			for (int c = 0; c < numChannels; c++) {
				dst[(y * dstWidth + x) * numChannels + c] = src[(srcY * srcWidth + srcX) * numChannels + c];
			}
		}
	}
}

// 缩放 JPEG 图像函数
void resizeJPEG(const char *inputFileName, const char *outputFileName, double ratio) {
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;
	FILE *infile;
	JSAMPARRAY buffer;
	int row_stride;
	// 初始化 JPEG 错误处理
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&cinfo);
	// 打开输入文件
	if ((infile = fopen(inputFileName, "rb")) == NULL) {
		perror("Failed to open input file");
		return;
	}
	// 指定输入文件
	jpeg_stdio_src(&cinfo, infile);
	// 读取 JPEG 文件头
	if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
		fprintf(stderr, "Error reading JPEG header.\n");
		jpeg_destroy_decompress(&cinfo);
		fclose(infile);
		return;
	}
	// 开始解压缩
	if (jpeg_start_decompress(&cinfo) != TRUE) {
		fprintf(stderr, "Error starting JPEG decompression.\n");
		jpeg_destroy_decompress(&cinfo);
		fclose(infile);
		return;
	}
	// 计算每行的字节数
	row_stride = cinfo.output_width * cinfo.output_components;
	// 分配内存用于存储解压缩后的图像数据
	buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, cinfo.output_height);
	if (buffer == NULL) {
		fprintf(stderr, "Memory allocation for buffer failed.\n");
		jpeg_destroy_decompress(&cinfo);
		fclose(infile);
		return;
	}
	// 读取图像数据
	for (int i = 0; i < cinfo.output_height; i++) {
		(void)jpeg_read_scanlines(&cinfo, &buffer[i], 1);
	}
	// 结束解压缩

	// 计算缩放后的宽度和高度
	int newWidth = (int)(cinfo.output_width * ratio);
	int newHeight = (int)(cinfo.output_height * ratio);
	// 分配内存用于存储缩放后的图像数据
	unsigned char *resizedImage = (unsigned char *)malloc(newWidth * newHeight * cinfo.output_components);
	if (resizedImage == NULL) {
		fprintf(stderr, "Memory allocation for resizedImage failed.\n");
		jpeg_destroy_decompress(&cinfo);
		return;
	}
	// 分配内存用于存储解压缩后的图像数据，将其存储为连续的一维数组
	unsigned char *srcImageData = (unsigned char *)malloc(cinfo.output_width * cinfo.output_height * cinfo.output_components);
	if (srcImageData == NULL) {
		fprintf(stderr, "Memory allocation for srcImageData failed.\n");
		jpeg_destroy_decompress(&cinfo);
		return;
	}
	// 将 buffer 中的数据复制到连续的内存块 srcImageData 中
	for (int i = 0; i < cinfo.output_height; i++) {
		for (int j = 0; j < row_stride; j++) {
			if (i * row_stride + j < cinfo.output_width * cinfo.output_height * cinfo.output_components) {
				srcImageData[i * row_stride + j] = buffer[i][j];
			}
			else {
				fprintf(stderr, "Memory access out of bounds when copying image data.\n");
				break;
			}
		}
	}
	(void)jpeg_finish_decompress(&cinfo);
	// 关闭输入文件
	fclose(infile);
	// 进行缩放操作
	nearestNeighborResize(srcImageData, resizedImage, cinfo.output_width, cinfo.output_height, newWidth, newHeight, cinfo.output_components);
	// 初始化 JPEG 压缩结构体
	struct jpeg_compress_struct cinfo_compress;
	struct jpeg_error_mgr jerr_compress;
	FILE *outfile;
	cinfo_compress.err = jpeg_std_error(&jerr_compress);
	jpeg_create_compress(&cinfo_compress);
	// 打开输出文件
	if ((outfile = fopen(outputFileName, "wb")) == NULL) {
		perror("Failed to open output file");
		free(resizedImage);
		free(srcImageData);
		jpeg_destroy_decompress(&cinfo);
		return;
	}
	// 指定输出文件
	jpeg_stdio_dest(&cinfo_compress, outfile);
	// 设置压缩参数
	cinfo_compress.image_width = newWidth;
	cinfo_compress.image_height = newHeight;
	cinfo_compress.input_components = cinfo.output_components;
	cinfo_compress.in_color_space = cinfo.out_color_space;
	jpeg_set_defaults(&cinfo_compress);
	jpeg_set_quality(&cinfo_compress, 80, TRUE);
	jpeg_start_compress(&cinfo_compress, TRUE);
	// 开始压缩
	//if (jpeg_start_compress(&cinfo_compress, TRUE) != TRUE) {
	//	fprintf(stderr, "Error starting JPEG compression.\n");
	//	jpeg_destroy_compress(&cinfo_compress);
	//	free(resizedImage);
	//	free(srcImageData);
	//	fclose(outfile);
	//	return;
	//}
	// 写入扫描线
	row_stride = newWidth * cinfo_compress.input_components;
	JSAMPARRAY buffer_compress = (*cinfo_compress.mem->alloc_sarray)((j_common_ptr)&cinfo_compress, JPOOL_IMAGE, row_stride, 1);
	for (int i = 0; i < newHeight; i++) {
		for (int j = 0; j < row_stride; j++) {
			buffer_compress[0][j] = resizedImage[i * row_stride + j];
		}
		(void)jpeg_write_scanlines(&cinfo_compress, buffer_compress, 1);
	}
	// 结束压缩
	jpeg_finish_compress(&cinfo_compress);
	// 关闭输出文件
	fclose(outfile);
	// 释放内存
	jpeg_destroy_compress(&cinfo_compress);
	jpeg_destroy_decompress(&cinfo);
	free(resizedImage);
	free(srcImageData);
}


int picType = 0;//1 png 三通道  2 tiff 四通道

//删除指定文件夹下所有目录
void RemoveAllFiles(string wstrDir)
{
	if (wstrDir.empty())
		return;
	HANDLE hFind;
	WIN32_FIND_DATAA findData;
	string wstrTempDir = wstrDir + ("\\*");;
	hFind = FindFirstFileA(wstrTempDir.c_str(), &findData);
	if (hFind == INVALID_HANDLE_VALUE)
	{
		return;
	}
	do
	{
		// 忽略"."和".."两个结果
		if (wstrTempDir.find(".") != std::string::npos || wstrTempDir.find("..") != std::string::npos)
			continue;
		string wstrFileName;
		wstrFileName.assign(wstrDir);
		wstrFileName.append("\\");
		wstrFileName.append(findData.cFileName);
		if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)// 是否是目录
		{
			RemoveAllFiles(wstrFileName.c_str());
		}
		else
		{
			DeleteFileA(wstrFileName.c_str());
		}
	} while (FindNextFileA(hFind, &findData));
	FindClose(hFind);
	RemoveDirectoryA(wstrDir.c_str());
}
#include <codecvt>  // 用于 UTF-8 编码转换
#include <locale>   // 本地化支持
// 保存多行宽字符串到 data.txt（UTF-8 编码，带 BOM）
bool saveLinesToUtf8File(const std::vector<std::string>& lines, bool withBOM = true) {
	// 1. 以二进制模式打开文件（关键：避免系统自动转换编码/换行符）
	std::ofstream ofs("main.txt", std::ios::binary);
	if (!ofs.is_open()) {
		std::cerr << "无法打开 data.txt 进行写入！" << std::endl;
		return false;
	}

	//// 2. 可选：写入 UTF-8 BOM（让记事本识别编码）
	//if (withBOM) {
	//	const unsigned char utf8Bom[3] = { 0xEF, 0xBB, 0xBF };
	//	ofs.write(reinterpret_cast<const char*>(utf8Bom), 3);
	//}

	// 3. 写入多行数据（保留原始 UTF-8 编码）
	for (size_t i = 0; i < lines.size(); ++i) {
		// 直接写入 string 中的字节（假设已是 UTF-8 编码）
		ofs.write(lines[i].c_str(), lines[i].size());

		// 除最后一行外，添加换行符 \n（跨平台兼容）
		if (i != lines.size() - 1) {
			ofs.put('\n');
		}
	}
	ofs.close();
	return true;
}

// 功能：修改 TXT 文件中指定行（行号从 0 开始）的内容
bool modifyTxtLine(const std::string &filePath, int lineNum, const std::string &newContent) {
	// 1. 读取所有行到容器
	std::ifstream inFile(filePath);
	if (!inFile.is_open()) {
		std::cerr << "文件打开失败（读）！" << std::endl;
		return false;
	}

	std::vector<std::string> lines;
	std::string line;
	while (std::getline(inFile, line)) { // 逐行读取（保留空行）
		lines.push_back(line);
	}
	inFile.close();

	// 2. 检查行号有效性
	if (lineNum < 0 || lineNum >= lines.size()) {
		std::cerr << "行号无效！总行数：" << lines.size() << "，目标行号：" << lineNum << std::endl;
		return false;
	}

	// 3. 修改目标行
	lines[lineNum] = newContent;

	// 4. 重新写入文件
	std::ofstream outFile(filePath, std::ios::trunc); // 覆盖写入
	if (!outFile.is_open()) {
		std::cerr << "文件打开失败（写）！" << std::endl;
		return false;
	}

	for (const auto &l : lines) {
		outFile << l << '\n'; // 写入每行（注意：原文件换行符可能是 \r\n，这里统一用 \n，跨平台兼容）
	}
	outFile.close();

	return true;
}
void LayoutDAO(LayoutInfo * layoutinfo) {
	//CWatchGuard<bool> cw3(&(layoutinfo->bool_run), true, false);
	std::string iniPath = "config.ini"; // 相对路径（与 exe 同目录）或绝对路径（如 "D:/config.ini"）

// 解析 INI 文件（UTF-8 编码，支持带/无 BOM）
	auto iniData = ParseIni(iniPath);

	// 检查是否解析到 [Setting] 分组
	if (iniData.find("Setting") == iniData.end()) {
		std::cerr << "错误：未找到 [Setting] 分组！" << std::endl;
		system("pause");
	}
	auto& settingGroup = iniData["Setting"];

	// 1. 分辨率配置
	int Ratio[3];
	Ratio[0] = StringToInt(settingGroup["less_resolution_ratio"], 100);   // less
	Ratio[1] = StringToInt(settingGroup["max_resolution_ratio"], 100);    // max
	Ratio[2] = StringToInt(settingGroup["other_resolution_ratio"], 100);  //other
	int ratioMode = StringToInt(settingGroup["other_resolution_ratio_mode"], 0);

	// 2. 布尔值配置
	vector<bool> bool_Selest ;
	for (int i = 1; i <= 5; ++i) {
		std::string key = "bool_Selest" + std::to_string(i);
		bool_Selest.push_back(StringToBool(settingGroup[key], false));
	}

	// 3. 颜色配置
	vector<int> colors;
	for (int i = 1; i <= 10; ++i) {
		std::string key = "Color" + std::to_string(i);
		colors.push_back(StringToInt(settingGroup[key], 0));
	}

	layoutinfo->bool_run = true;
	layoutinfo->total_height = 0;
	layoutinfo->save_height = 0;
	layoutinfo->double_progress_real = 0;
	std::string intput_path(layoutinfo->intput_path, layoutinfo->intput_length);
	std::string output_path(layoutinfo->output_path, layoutinfo->output_length);
	string str0 = "working:1";
	string str1 = "intput_path:"      + intput_path ;
	string str2 = "output_path:"      + output_path ;
	string str3 = "loop_count:"       + std::to_string(layoutinfo->loop_count)		 ;
	string str4 = "operate_count:"    + std::to_string(layoutinfo->operate_count)	 ;
	string str5 = "element_interval:" + std::to_string(layoutinfo->element_interval) ;
	string str6 = "paper_width:"      + std::to_string(layoutinfo->paper_width)	 ;
    // 准备多行数据（含中文、英文等 Unicode 字符）
    std::vector<std::string> dataLines = {
		str0	,
		str1	,
		str2	,
		str3	,
		str4	,
		str5	,
		str6
    };

	// 保存到 data.txt
    bool success = saveLinesToUtf8File(dataLines);
	if (intput_path == "")
	{
		layoutinfo->error_info = "输入路径为空";
		layoutinfo->bool_run = false;
		return;
	}
	else
	{
		if (intput_path.length() > layoutinfo->intput_length && layoutinfo->intput_length != 0)
			intput_path.erase(layoutinfo->intput_length);
		//int pos = intput_path.rfind("\\");
		//if (pos != -1)
		//	intput_path.erase(pos);
		//intput_path += "\\";
	}
	if (output_path == "")
	{
		layoutinfo->error_info = "输出路径为空";
		layoutinfo->bool_run = false;
		return;
	}
	else
	{
		if(output_path.length() > layoutinfo->output_length && layoutinfo->output_length != 0)
			output_path.erase(layoutinfo->output_length);
		//int pos = output_path.rfind("\\");
		//if (pos != -1)
		//	output_path.erase(pos);
		//output_path += "\\";
	}
	string delim = "\\";
	std::vector<std::string> elems = split(intput_path, delim);
	std::vector<std::string> elems2 = split(output_path, delim);
	string output_path_;
	for (int i = 0; i < elems2.size(); i++)
	{
		if (elems.at(i) != elems2.at(i))
		{
			string outpath = output_path;
			int pos = outpath.rfind("\\");
			if (pos != -1)
				outpath.erase(pos);
			if(layoutinfo->bool_RemoveFile)
				RemoveAllFiles(outpath);
			EnsureFoldExistA(outpath, true);
			break;
		}
	}
	string cFileName;
	std::map<std::string, std::vector<std::string>> folderJpgMap;
	getAllJpgFilesInFolders(intput_path, folderJpgMap, cFileName);
	for (auto it = folderJpgMap.begin(); it != folderJpgMap.end(); it++)
	{
		if (!layoutinfo->bool_run)
			break;
		std::string str_outpath_less = it->first;
		vector<std::string> path_vector = it->second;
		time_t now1 = time(0);   // 获取当前时间
		char* dt = ctime(&now1); // 转换为字符串形式

		std::cout << "起始时间: " << dt << std::endl;
		//resize(0.1);
		double d[24] = { 0 };//一组中每张图片的排版进度
		int resolution_[24] = { 0 };//每一组图片最小分辨率
		int loop_count = layoutinfo->loop_count;
		int operate_count[24] = { 0 };//每一组图片排版数量
		double total_end = 0;
		double total_height = 0;
		int jpg_index = 0;
		int element_interval = layoutinfo->element_interval * 100 / 25/*.4*/;// 10;
		double double_progress = 0;
		bool canRol = false;
		int jpg_count = path_vector.size();
		int maxWidth = 0;
		//if (path_vector.size() > loop_count * operate_count)
		//	jpg_count = loop_count * operate_count;
		size_t l = 0;
		int jpg_index_tmp = 0;
		std::vector<cv::Mat> img_all;
		for (size_t x = 0; x < path_vector.size(); x++)
		{
			cv::Mat src;
			if ((path_vector)[x + jpg_index].find(".tif") == -1)
				src = imread((path_vector)[x + jpg_index]);
			else {
				src = readCMYKImage((path_vector)[x + jpg_index]);
			}
			img_all.push_back(padImageTopBottom(src, element_interval));
		}
		for (size_t x = 0; x < path_vector.size(); x++)
		{
			int dpi_x;
			getImageDPI((path_vector)[x + jpg_index].c_str(), dpi_x);
			bool MoreMaxwidth = false;
			//判断图片裁片宽度是否超过排版设置宽度
			Mat imgGray, imgBlur, imgCanny, imgDil;//灰色，模糊，精明， ，imgErode腐蚀
			cvtColor(img_all[x], imgGray, COLOR_BGR2GRAY);					//imshow("Image", img);			图像中不同的色彩空间进行转换 单通道灰度图
			GaussianBlur(imgGray, imgBlur, Size(3, 3), 3, 0);		//imshow("Image", img);			高斯滤波 
			Canny(imgBlur, imgCanny, 75, 100);						//imshow("Image", img);			边缘检测	
			Mat kernel = getStructuringElement(MORPH_RECT, Size(3, 3));	//imshow("Image", img);		返回进一步传递给可以erosion（腐蚀）、dilate（膨胀）或morphologyEx的结构元素，用于图形学操作。
			dilate(imgCanny, imgDil, kernel);						//imshow("Image", img);			膨胀函数
			//cv::imwrite("output_image22.jpg", imgDil);			//								输出到指定文件
			vector<vector<cv::Point>> contours_temp;//找轮廓点
			vector<vector<cv::Point>> contours_temp_;//找轮廓点
			vector<Vec4i>  hierarchy;
			findContours(imgDil, contours_temp, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);//找轮廓
			for (int i = 0; i < contours_temp.size(); i++)//识别出来的图片面积小于10000时忽略
			{
				vector<cv::Point> conPoly;
				double peri = arcLength(contours_temp[i], true);//计算闭合轮廓的周长
				approxPolyDP(contours_temp[i], conPoly, 0.00001 * peri, true);//找到轮廓点
				Rect rect1;
				rect1 = boundingRect(conPoly);
				if (rect1.width * rect1.height > 10000)
				{
					contours_temp_.push_back(contours_temp[i]);
				}
			}
			if (contours_temp_.size() > 30)
			{
				string str = "图片" + path_vector.at(x);
				str += "中有超过30个裁片，已退出这次排版，请检查图片";
				layoutinfo->error_info = str;
				path_vector.erase(path_vector.begin() + x);
				img_all.erase(img_all.begin() + x);
				MoreMaxwidth = true;
			}
			if (layoutinfo->paper_width != 0 && !MoreMaxwidth) {
				for (int i = 0; i < contours_temp_.size(); i++)//识别出来的图片面积小于10000时忽略
				{
					vector<cv::Point> conPoly;
					double peri = arcLength(contours_temp_[i], true);//计算闭合轮廓的周长
					approxPolyDP(contours_temp_[i], conPoly, 0.00001 * peri, true);//找到轮廓点
					Rect rect1;
					rect1 = boundingRect(conPoly);
					if (rect1.width * rect1.height > 10000 && rect1.width > layoutinfo->paper_width * dpi_x / 25.4)
					{
						string str = "图片" + path_vector.at(x);
						str += "中有单个裁片大于排版宽度，已退出这次排版，请检查图片";
						layoutinfo->error_info = str;
						path_vector.erase(path_vector.begin() + x);
						img_all.erase(img_all.begin() + x);
						MoreMaxwidth = true;
						break;
					}
				}
			}
			if (MoreMaxwidth) {
				x--;
			}
		}
		if (path_vector.size() == 0)
		{
			string str = "已没有排版图片，请重试";
			layoutinfo->error_info = str;
			break;
		}
		for (l = 0; jpg_index < path_vector.size();l++)//jpg_count; l++)
		{
			if (!layoutinfo->bool_run)
				break;
			jpg_index_tmp = jpg_index;
			canRol = false;
			if (ratioMode == 0)//最大值
				resolution_[l] = 0;
			else if (ratioMode == 1)//最小值
				resolution_[l] = 1000;
			int operate_count_tmp = layoutinfo->operate_count;
			while (!canRol)
			{
				for (int i = 0; i < loop_count&&jpg_index < path_vector.size(); i++, jpg_index += operate_count_tmp)
				{
					std::vector<cv::Mat> img;
					int height = 0;
					for (size_t x = 0; x < min(operate_count_tmp, (int)path_vector.size() - jpg_index); x++)
					{
						img.push_back(img_all.at(x + jpg_index));
						//img.push_back(img_all.at(0));
						//img_all.erase(img_all.begin() + 0);
					}

					int dpi_x = 0;
					vector<int> dpi_x_vector;
					for (size_t x = 0; x < min(operate_count_tmp, (int)path_vector.size() - jpg_index); x++)
					{
						getImageDPI((path_vector)[x + jpg_index].c_str(), dpi_x);
						if (dpi_x == 0)
							dpi_x = Ratio[0];
						dpi_x_vector.push_back(dpi_x);
						if (ratioMode == 2)//固定值
							resolution_[l] = Ratio[2];
						else if (ratioMode == 0)//最大值
							resolution_[l] = max(resolution_[l], dpi_x);
						else if (ratioMode == 1)//最小值
							resolution_[l] = min(resolution_[l], dpi_x);
					}
					if (resolution_[l] > Ratio[1])
						resolution_[l] = Ratio[1];
					for (size_t x = 0; x < min(operate_count_tmp, (int)path_vector.size() - jpg_index); x++)
					{
						if (dpi_x_vector[x] != resolution_[l] && dpi_x_vector[x] != 0)
							img[x] = resize(img[x], (double)resolution_[l] / (double)dpi_x_vector[x]);
						if (maxWidth < img[x].size().width)
							maxWidth = img[x].size().width;
					}

					for (size_t x = 0; x < min(operate_count_tmp, (int)path_vector.size() - jpg_index); x++)
					{
						if (img[x].cols >= layoutinfo->paper_width * resolution_[l] / 25.4 && layoutinfo->paper_width != 0)
							height += img[x].rows * img[x].cols / (layoutinfo->paper_width * resolution_[l] / 25.4);// maxWidth;
						else
							height += img[x].rows * img[x].cols / maxWidth;
					}

					//std::vector<cv::Mat> img;
					//int height = 0;
					//for (size_t i = 0; i < min(operate_count, (int)path_vector.size() - jpg_index); i++)
					//{
					//	cv::Mat src;
					//	if ((path_vector)[i + jpg_index].find(".tif") == -1)
					//		src = imread((path_vector)[i + jpg_index]);
					//	else {
					//		src = readCMYKImage((path_vector)[i + jpg_index]);
					//	}
					//	img.push_back(padImageTopBottom(src, element_interval));
					//	height += img[i].rows * img[i].cols / img[0].size().width;
					//}
					if (height > 60000)
					{
						operate_count_tmp--;
						i = 0;
						jpg_index = jpg_index_tmp;
						canRol = false;
						break;
					}
					else {
						operate_count[l] = operate_count_tmp;
						canRol = true;
					}
				}
			}
			if (operate_count_tmp != layoutinfo->operate_count)
			{
				string str = "第";
				str += std::to_string(l + 1);
				str += "组图片的排版数量";
				str += std::to_string(layoutinfo->operate_count);
				str += "个长度不满足图片最长长度，最多排版数量为";
				str += std::to_string(operate_count_tmp) + "个。";
				layoutinfo->error_info = str;
			}
		}
		double loop = l;
		int operate_allcount_last = 0 ;
		jpg_index = 0;
		bool bool_end_status[24] = { true };
		for (size_t l = 0; jpg_index < path_vector.size(); l++)
		{
			vector<int> operate_list;
			if (!layoutinfo->bool_run)
				break;
			for (size_t i = 0; i < loop_count; i++)
				bool_end_status[i] = true;
			for (int i = 0; i < loop_count&&jpg_index < path_vector.size(); i++, jpg_index += operate_count[l])
			{
				if (!layoutinfo->bool_run)
					break;
				bool_end_status[i] = false;
				std::thread pth(&JpgProcess, output_path, str_outpath_less, path_vector, jpg_index, min(operate_count[l], (int)path_vector.size() - jpg_index), 
					i, &bool_end_status[i], &layoutinfo->total_height, &layoutinfo->total_end, &d[i], &layoutinfo->bool_run, &resolution_[l], element_interval,
					layoutinfo->paper_width, &layoutinfo->error_info, &layoutinfo->save_height, Ratio[0], bool_Selest, colors);
				pth.detach();

				if(path_vector.size() - jpg_index > 0)
					operate_list.push_back(min(operate_count[l], (int)path_vector.size() - jpg_index));
			}
			while (layoutinfo->bool_run)
			{
				bool bool_sum = true;
				for (size_t i = 0; i < loop_count; i++)
				{
					bool_sum &= bool_end_status[i];
				}
				if (bool_sum)
					break;
				else
					Sleep(100);
				//layoutinfo->run_time =  layout_ac.End();
				double progress = 1;
				for (int i = 0; i < operate_list.size(); i++)
				{
					progress = min(progress, (d[i]- operate_allcount_last) / operate_list.at(i) / loop + l / loop);
				}
				layoutinfo->double_progress_real = progress;
			}
			operate_allcount_last += operate_count[l];
		}
		//layoutinfo->total_end += total_end;
		if (layoutinfo->bool_run == false || layoutinfo->save_height < 0)
		{
			layoutinfo->save_height = 0;
		}
		time_t now2 = time(0);   // 获取当前时间
		dt = ctime(&now2); // 转换为字符串形式
		//layoutinfo->resolution = 100;
		std::cout << "结束时间: " << dt << std::endl;
		std::cout << "总耗时: " << now2 - now1 << "秒 " << std::endl;
		std::cout << layoutinfo->total_end << " 总长度(m): " << (double)(layoutinfo->total_height)  << " 总省料(m)：" << (double)(layoutinfo->total_height - layoutinfo->total_end)  << std::endl;//  / layoutinfo->resolution * 25.4 / 1000

		if (layoutinfo->bool_run == false)
			break;
	}
	Sleep(3000);
	//ofs.open("main.txt", ios::out);
	//ofs << "working:0" << std::endl;
	//ofs.close();
	modifyTxtLine("main.txt",0, "working:0");
	layoutinfo->bool_run = false;
	layoutinfo->bool_dll_run = false;
}

void colbesselPoint(int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3, double xout, double yout)
{
	float temp; double t1;
	for (int t = 0; t < 100; t += 1)
	{
		temp = 1 - t / 100.;
		t1 = t / 100.;
		xout = x0 * temp * temp * temp + 3 * x1 * t1 * temp * temp + 3 * x2 * t1 * t1 * temp + x3 * t1 * t1 * t1;
		yout = y0 * temp * temp * temp + 3 * y1 * t1 * temp * temp + 3 * y2 * t1 * t1 * temp + y3 * t1 * t1 * t1;
	}
}

//#include <vld.h>  // 仅这一行，VLD 会自动处理调试逻辑，无需 _DEBUG 宏




//void main() {
//	LayoutInfo layoutinfo;
//
//	layoutinfo.loop_count = 3;
//	layoutinfo.operate_count = 6;
//	layoutinfo.paper_width = 1600;//1580 
//	layoutinfo.element_interval = 2.5;
//	layoutinfo.total_height = 0;
//	layoutinfo.total_end = 0;
//
//	layoutinfo.intput_path = "e:\\wqww\\105平板布\\";
//	layoutinfo.intput_length = 18;
//	layoutinfo.output_path = "e:\\ws_dtx\\105平板布(排版后)\\";
//	layoutinfo.output_length = 28;
//	layoutinfo.bool_run = true;
//	layoutinfo.bool_RemoveFile = false;
//	std::thread pth(&LayoutDAO, &layoutinfo);			pth.detach();
//	int starttime = 0;
//	while (layoutinfo.bool_run)
//	{
//		Sleep(1000);
//		starttime++;
//		//////////if (starttime == 240){
//		//////////	layoutinfo.bool_run = false;
//		//////////	std::cout << "结束。";
//		//////////}
//		std::cout << "完成百分百: " << layoutinfo.double_progress_real << std::endl;
//	}
//	while (1)
//	{
//		Sleep(1);
//	}
//}

