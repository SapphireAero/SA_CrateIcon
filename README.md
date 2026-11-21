# SA_CrateIcon

《心灵终结》3.3.6 独立箱子图标插件。已定型的箱子显示类型图标。使用半透明黑幕时，未探索区域的图标保留约 75% 亮度。使用原版黑幕时，该区域隐藏图标。

## 安装

下载 `SA_CrateIcon.dll`，放到 `gamemd.exe` 同级目录，从客户端正常开局。需要半透明黑幕时，将半透明版 `shroud.shp` 也放到同一目录。

## 构建

使用 Python 3 和 32 位 MinGW-w64（`i686-w64-mingw32`）。将工具链的 `bin` 目录加入 `PATH` 或设为 `MOE_MINGW`，然后运行 `python build.py`。DLL 输出到 `dist/`。

## 许可证

本项目采用 [MIT 许可证](LICENSE)。
