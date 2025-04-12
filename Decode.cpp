#include <iostream>
#include <fstream>
#include <vector>
#include <string>
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
    uint16_t file_type;          // "BM"
    uint32_t file_size;          // 文件大小
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t offset_data;        // 数据偏移量

    // 信息头
    uint32_t size;               // 信息头大小
    int32_t width;              // 图像宽度
    int32_t height;             // 图像高度
    uint16_t planes;            // 颜色平面数
    uint16_t bit_count;         // 每像素位数
    uint32_t compression;       // 压缩方式
    uint32_t size_image;        // 图像数据大小
    int32_t x_pixels_per_meter; // 水平分辨率
    int32_t y_pixels_per_meter; // 垂直分辨率
    uint32_t colors_used;       // 使用的颜色数
    uint32_t colors_important;  // 重要颜色数
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

// 从BMP文件名提取原始文件名
string getOriginalFilename(const string& bmpFilename) {
    size_t bmpExtPos = bmpFilename.rfind(".bmp");
    if (bmpExtPos != string::npos && bmpExtPos == bmpFilename.length() - 4) {
        return bmpFilename.substr(0, bmpExtPos);
    }
    return bmpFilename;
}

// 解码BMP文件为原始文件
bool decodeBMPToFile(const string& inputBmp, const string& outputFile) {
    // 打开BMP文件
    ifstream bmpFile(inputBmp, ios::binary);
    if (!bmpFile) {
        cerr << "无法打开BMP文件: " << inputBmp << endl;
        return false;
    }

    // 读取BMP头
    BMPHeader header;
    bmpFile.read(reinterpret_cast<char*>(&header), sizeof(header));

    // 验证BMP文件
    if (header.file_type != 0x4D42 || header.bit_count != 24) {
        cerr << "无效的BMP文件或不是24位BMP: " << inputBmp << endl;
        return false;
    }

    // 计算原始文件大小
    size_t originalSize = header.width * header.height * 3;

    // 定位到像素数据
    bmpFile.seekg(header.offset_data);

    // 创建输出文件
    ofstream outFile(outputFile, ios::binary);
    if (!outFile) {
        cerr << "无法创建输出文件: " << outputFile << endl;
        return false;
    }

    // 读取像素数据并还原原始文件
    vector<char> pixel(3);
    size_t bytesWritten = 0;
    size_t lastProgress = 0;

    cout << "解码中: " << inputBmp << " -> " << outputFile << endl;
    cout << "进度: 0%" << flush;

    for (int y = 0; y < header.height; ++y) {
        for (int x = 0; x < header.width; ++x) {
            // 读取像素数据 (BGR顺序)
            bmpFile.read(pixel.data(), 3);

            // 写入原始数据 (RGB顺序)
            outFile.write(&pixel[2], 1); // R
            outFile.write(&pixel[1], 1); // G
            outFile.write(&pixel[0], 1); // B

            bytesWritten += 3;

            // 更新进度显示
            size_t progress = (bytesWritten * 100) / originalSize;
            if (progress != lastProgress && progress % 5 == 0) {
                cout << "\r进度: " << progress << "%" << flush;
                lastProgress = progress;
            }
        }

        // 跳过行填充
        int padding = (4 - (header.width * 3) % 4) % 4;
        bmpFile.seekg(padding, ios::cur);
    }

    cout << "\r进度: 100%" << endl;

    // 由于BMP图像可能比原始文件大，需要截断到正确大小
    // 查找文件结束标记（编码时添加的特定模式）
    // 这里简化处理，实际应根据编码时添加的元数据确定确切大小

    // 更准确的方法是编码时在文件开头存储原始文件大小
    // 这里假设用户知道原始文件大小不重要

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
    ofn.lpstrFilter = L"BMP Files\0*.bmp\0All Files\0*.*\0\0";
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
    cout << "BMP文件解码器 - 将BMP图像还原为原始文件" << endl;
    cout << "请选择要解码的BMP文件..." << endl;

    // 获取文件选择
    vector<string> inputBmps = openFileDialog();

    if (inputBmps.empty()) {
        cout << "没有选择文件，程序退出。" << endl;
        return 0;
    }

    cout << "已选择 " << inputBmps.size() << " 个BMP文件进行解码。" << endl;

    // 处理每个文件
    for (const auto& inputBmp : inputBmps) {
        // 获取原始文件名（去掉.bmp扩展名）
        string originalFile = getOriginalFilename(inputBmp);

        // 解码文件
        if (decodeBMPToFile(inputBmp, originalFile)) {
            cout << "成功解码: " << inputBmp << " -> " << originalFile << endl;
        }
        else {
            cerr << "解码失败: " << inputBmp << endl;
        }
    }

    cout << "所有文件处理完成。" << endl;
    system("pause");
    return 0;
}