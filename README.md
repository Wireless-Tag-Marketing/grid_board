# DIY一个提词器

Grid Board Web **1.2.2**，使用 **ESP-IDF 6.1.x**，运行于 [WT99P461-S1（ESP32-P4 + ESP32-C61）](https://wiki.wireless-tag.com/docs/zh/WT99P461-S1/board_features.html)。中文稿件通过本地网页编辑，屏幕支持滚动、触摸回看和设备设置。

## 直接使用完整固件

交付文件为 `firmware/grid_board_web_v1.2.2_merged.bin`，配套 SHA-256 在 `firmware/SHA256SUMS`。镜像已合并启动程序、分区表、应用、网页和内置字体，**烧录起始地址为 `0x0`**。

| 项目 | 设置 |
| --- | --- |
| 目标芯片 | ESP32-P4，芯片版本至少 3.1 |
| Flash | 16 MB，DIO，80 MHz |
| 起始地址 | `0x0` |
| 启动程序 / 分区表 / 应用 | `0x2000` / `0x10000` / `0x30000`，已包含在同一个文件中 |

**完整镜像用于首次烧录或恢复出厂，会覆盖 NVS 中已保存的稿件和配置。** 普通升级要保留配置时，重新构建后使用 `app-flash`，见下节。

此镜像写入 **P4**。板载 **C61** 需已有支持 SPI、Wi-Fi AP 的 **ESP-Hosted 2.12.0** 从机固件；现有设备已使用该版本。P4 与 C61 为独立芯片，合并镜像不改写 C61。

在带有 esptool 的环境中烧录：

```bash
python -m esptool --chip esp32p4 --port /dev/ttyACM0 --baud 2000000 write-flash \
  --flash-mode dio --flash-freq 80m --flash-size 16MB \
  0x0 firmware/grid_board_web_v1.2.2_merged.bin
```

Windows 将端口改为实际 COM 号，例如 `COM46`；WSL 需要先通过 USB/IP 接入设备。使用图形烧录工具时，选择 ESP32-P4、上述 Flash 参数和地址 `0x0`。

校验下载文件：

```bash
cd firmware
sha256sum -c SHA256SUMS
```

## 网页与触摸操作

1. 连接设备热点 **WIRELESS-GridBoard-Web**，密码 **12345678**。系统提示“无互联网”时选择保持连接。
2. 浏览器访问 **http://192.168.4.1**，在“当前显示”输入稿件，点击“保存并显示”。页面、字体与脚本均在设备内，无需 APP、路由器、外网或 SD 卡。
3. 提词时建议关闭网格、将下落时长设为 0，并按阅读距离调整字号和滚动速度。
4. 长文可上下拖动。按住暂停，松手停 **5 秒**后从当前位置继续；到达首尾时不越界。自动滚动上下往返，首尾各停 1.5 秒。
5. **双击屏幕**打开设置，可调整字号、下落时长、滚动速度、亮度、网格及默认配置；文字通过网页编辑。菜单期间暂停，返回看板 **3 秒**后续播。
6. 未保存时返回，可选择“保存并返回 / 放弃并返回 / 继续编辑”；恢复默认与恢复出厂均有确认和取消入口。

| 参数 | 范围与说明 |
| --- | --- |
| 稿件容量 | 最多 2048 UTF-8 字节；超出一屏会黄色提示，仍可保存完整内容并滚动 |
| 字体大小 | 24–128 px；64 px 时一屏 12 列 × 5 行 |
| 下落时长 | 0–3000 ms；越小越快，0 直接显示 |
| 滚动速度 | 5–240 px/s，默认 40 px/s |
| 亮度 | 1–100% |
| 默认配置 | 独立保存稿件与样式，保存默认不会立即替换当前显示 |

“恢复已设默认”套用保存的默认稿件及样式。“恢复出厂配置”同时重置当前与默认配置，欢迎语为：

```text
欢迎使用
中文网格看板
https://wireless-tag.com
```

出厂字号 64 px、下落 600 ms、滚动 40 px/s、亮度 80%、网格开启。网址按当前列数自动换行。

保存成功后即可断电，下次上电恢复最后一次成功保存的稿件和配置。写入过程中掉电的新内容不保证保存。修改滑块只调整草稿，点击保存才写入 Flash。网页和设备菜单通过修订号避免相互覆盖，提示版本冲突时重新读取配置。

字体支持中文、英文、数字及字库覆盖的独立表情；缺字显示问号，不支持多码点组合表情或复杂文字塑形。浏览器预览使用系统字体，与设备字形可能略有差异。热点用于本地配置，不提供互联网转发或自动弹出门户。

## 从源码构建与打包

安装并使用 ESP-IDF **6.1.x**；本机默认路径为 `$HOME/.espressif/v6.1.0/esp-idf`。首次构建需要从组件仓库下载锁定版本的依赖。

```bash
# 构建并生成一个完整固件，同时核对各段数据及 SHA-256
bash tools/idf.sh package

# 仅构建
bash tools/idf.sh build

# 保留配置的升级：仅适用于本项目已有相同分区布局的设备
bash tools/idf.sh -p /dev/ttyACM0 app-flash
```

其他 SDK 路径可设置 `GRID_BOARD_IDF_PATH=/path/to/esp-idf-v6.1`。脚本使用已安装的工具环境，不修改 IDE 的 SDK 选择。保留 `dependencies.lock`，构建时自动生成 `sdkconfig`、`managed_components/` 和 `build/`。

热点名称、密码、信道可在 `bash tools/idf.sh menuconfig` 的 **Grid Board Web** 中修改。私人密码保存在本机 `sdkconfig`，不要提交或分发该文件。

## 硬件连接

| 部件 | 配置 |
| --- | --- |
| 主板 | WT99P461-S1，16 MiB Flash、32 MiB HEX PSRAM，200 MHz |
| 显示屏 | JD9165BA，1024×600，双通道 MIPI DSI，600 Mbps；LDO3 供电 2500 mV |
| 背光 / 扫描方向 | GPIO20 / GPIO26、27 均拉高 |
| 触摸 | GT911；I²C1，SDA48、SCL47，RST23、INT21，地址 0x5D |
| C61 SPI | MOSI14、MISO15、CLK18、CS19，10 MHz，模式 3 |
| C61 辅助信号 | HANDSHAKE16、DATA_READY17、RESET13，高有效复位 |

触摸复位与显示面板有关，保持先复位触摸、再初始化显示面板的顺序。设备显示和本机设置先于网络启动，C61 连接失败不会阻止已保存的稿件显示。

## 项目目录与许可

```text
main/              应用源码、网页、字体与字体许可证
components/        显示驱动
tools/             ESP-IDF 入口与完整固件打包工具
firmware/          单文件固件与 SHA-256
docs/              HTTP 接口说明与最新版 DIY 视频脚本
```

接口见 [docs/API.md](docs/API.md)，视频文案见 [DIY一个提词器脚本](docs/WT99P461-S1_自制提词器_视频分镜脚本.xlsx)。

项目许可见 [LICENSE](LICENSE)。显示驱动中的 Apache-2.0 声明和 `main/fonts/OFL-*.txt` 字体许可证保留；字体来源见 [main/fonts/README.md](main/fonts/README.md)。第三方许可汇总见 [THIRD_PARTY_NOTICES.txt](THIRD_PARTY_NOTICES.txt)。
