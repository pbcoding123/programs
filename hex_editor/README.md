## 十六进制编辑器

开源简洁，体积仅 2.39 MB。

### 支持操作

打开文件（Ctrl+O）、保存文件（Ctrl+S）、另存为、查找（Ctrl+F）。

直接点击位编辑，可以编辑十六进制数据和 ASCII 文本。

最大支持 4 GB 文件。

![hexeditor](https://cdn.jsdelivr.net/gh/pbcoding123/img@main/store/hexeditor.png)

### 编译命令

```batch
g++ -o hex.exe hex.cpp -O2 -std=c++14 -static-libgcc -lgdi32 -lcomctl32 -lcomdlg32 -municode -mwindows
```
