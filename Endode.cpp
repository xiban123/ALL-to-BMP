#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <windows.h>
#include <commdlg.h>
#include <iomanip>
#include <sstream>
#include <locale>
#include <codecvt>

#pragma comment(lib, "comdlg32.lib")

using namespace std;

// BMP 文件头结构
#pragma pack(push, 1)
struct BMPHeader {
    // 文件头
    uint16_t file_type{ 0x4D42 };          // "BM"
    uint32_t file_size{ 0 };               // 文件大小
    uint16_t reserved1{ 0 };
    uint16_t reserved2{ 0 };
    uint32_t offset_data{ 0 };             // 数据偏移量

    // 信息头
    uint32_t size{ 40 };                   // 信息头大小
    int32_t width{ 0 };                    // 图像宽度
    int32_t height{ 0 };                   // 图像高度
    uint16_t planes{ 1 };                  // 颜色平面数
    uint16_t bit_count{ 24 };              // 每像素位数
    uint32_t compression{ 0 };             // 压缩方式
    uint32_t size_image{ 0 };              // 图像数据大小
    int32_t x_pixels_per_meter{ 0 };       // 水平分辨率
    int32_t y_pixels_per_meter{ 0 };       // 垂直分辨率
    uint32_t colors_used{ 0 };             // 使用的颜色数
    uint32_t colors_important{ 0 };        // 重要颜色数
};
#pragma pack(pop)

// 宽字符转多字节字符串
string WCharToMByte(LPCWSTR lpcwszStr) {
    string str;
    int len = WideCharToMultiByte(CP_ACP, 0, lpcwszStr, -1, NULL, 0, NULL, NULL);
    if (len <= 0) return str;

    char* pszStr = new char[len];
    WideCharToMultiByte(CP_ACP, 0, lpcwszStr, -1, pszStr, len, NULL, NULL);
    str = pszStr;
    delete[] pszStr;
    return str;
}

// 计算合适的图像尺寸
void calculateDimensions(size_t fileSize, int& width, int& height) {
    // 每个像素存储3字节 (24位)
    size_t totalPixels = (fileSize + 2) / 3;  // +2是为了确保向上取整
    width = static_cast<int>(ceil(sqrt(totalPixels)));
    height = static_cast<int>(ceil(static_cast<double>(totalPixels) / width));

    // 确保宽度是4的倍数 (BMP要求每行字节数是4的倍数)
    if ((width * 3) % 4 != 0) {
        width += 4 - (width * 3) % 4 / 3;
    }
}

// 编码文件为BMP
bool encodeFileToBMP(const string& inputFile, const string& outputFile) {
    // 打开输入文件
    ifstream inFile(inputFile, ios::binary | ios::ate);
    if (!inFile) {
        cerr << "无法打开输入文件: " << inputFile << endl;
        return false;
    }

    // 获取文件大小
    size_t fileSize = inFile.tellg();
    inFile.seekg(0);

    // 计算BMP尺寸
    int width = 0, height = 0;
    calculateDimensions(fileSize, width, height);

    // 准备BMP头
    BMPHeader header;
    header.width = width;
    header.height = height;
    header.size_image = width * height * 3;
    header.file_size = sizeof(BMPHeader) + header.size_image;
    header.offset_data = sizeof(BMPHeader);

    // 创建输出文件
    ofstream outFile(outputFile, ios::binary);
    if (!outFile) {
        cerr << "无法创建输出文件: " << outputFile << endl;
        return false;
    }

    // 写入BMP头
    outFile.write(reinterpret_cast<const char*>(&header), sizeof(header));

    // 写入像素数据
    vector<char> buffer(3);
    size_t bytesRead = 0;
    size_t totalBytes = fileSize;
    size_t lastProgress = 0;

    cout << "编码中: " << inputFile << " -> " << outputFile << endl;
    cout << "进度: 0%" << flush;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (bytesRead < fileSize) {
                // 读取3个字节
                size_t bytesToRead = min(static_cast<size_t>(3), fileSize - bytesRead);
                inFile.read(buffer.data(), bytesToRead);

                // 如果不足3个字节，填充0
                for (size_t i = bytesToRead; i < 3; ++i) {
                    buffer[i] = 0;
                }

                // 写入像素数据 (BGR顺序)
                outFile.write(&buffer[2], 1); // B
                outFile.write(&buffer[1], 1); // G
                outFile.write(&buffer[0], 1); // R

                bytesRead += bytesToRead;
            }
            else {
                // 填充剩余像素为黑色
                outFile.write("\0\0\0", 3);
            }

            // 更新进度显示
            size_t progress = (bytesRead * 100) / totalBytes;
            if (progress != lastProgress && progress % 5 == 0) {
                cout << "\r进度: " << progress << "%" << flush;
                lastProgress = progress;
            }
        }

        // 每行填充到4字节倍数
        int padding = (4 - (width * 3) % 4) % 4;
        for (int i = 0; i < padding; ++i) {
            outFile.put(0);
        }
    }

    cout << "\r进度: 100%" << endl;
    return true;
}

// 获取多个文件选择对话框
vector<string> openFileDialog() {
    vector<string> files;

    OPENFILENAME ofn;
    wchar_t szFile[1024 * 16] = { 0 };  // 多文件选择缓冲区

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.lpstrFile[0] = L'\0';
    ofn.nMaxFile = sizeof(szFile) / sizeof(szFile[0]);
    ofn.lpstrFilter = L"All Files\0*.*\0\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER | OFN_ALLOWMULTISELECT;

    if (GetOpenFileNameW(&ofn)) {
        wchar_t* p = szFile;
        wstring directory = p;
        p += directory.size() + 1;

        if (*p) {
            // 多文件选择
            while (*p) {
                wstring filename = p;
                files.push_back(WCharToMByte(directory.c_str()) + "\\" + WCharToMByte(filename.c_str()));
                p += filename.size() + 1;
            }
        }
        else {
            // 单文件选择
            files.push_back(WCharToMByte(directory.c_str()));
        }
    }

    return files;
}

int main() {
    cout << "BMP文件编码器 - 将任意文件编码为BMP图像" << endl;
    cout << "请选择要编码的文件..." << endl;

    // 获取文件选择
    vector<string> inputFiles = openFileDialog();

    if (inputFiles.empty()) {
        cout << "没有选择文件，程序退出。" << endl;
        return 0;
    }

    cout << "已选择 " << inputFiles.size() << " 个文件进行编码。" << endl;

    // 处理每个文件
    for (const auto& inputFile : inputFiles) {
        // 获取文件名（保留扩展名）
        size_t lastSlash = inputFile.find_last_of("\\/");
        string filename = (lastSlash == string::npos) ? inputFile : inputFile.substr(lastSlash + 1);

        // 生成输出文件名（添加.bmp扩展名）
        string outputFile = inputFile + ".bmp";

        // 编码文件
        if (encodeFileToBMP(inputFile, outputFile)) {
            cout << "成功编码: " << filename << " -> " << filename << ".bmp" << endl;
        }
        else {
            cerr << "编码失败: " << filename << endl;
        }
    }

    cout << "所有文件处理完成。" << endl;
    system("pause");
    return 0;
}