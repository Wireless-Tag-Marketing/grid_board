# 内置字体

两个字体均直接打包进固件，可在没有文件系统和 SD 卡时通过 FreeType 加载。字形只为当前消息生成缓存；不预先生成所有汉字的位图。

| 文件 | 来源 | SHA-256 |
| --- | --- | --- |
| `NotoSansSC-Regular.otf` | [notofonts/noto-cjk，Sans/SubsetOTF/SC](https://github.com/notofonts/noto-cjk/blob/main/Sans/SubsetOTF/SC/NotoSansSC-Regular.otf) | `faa6c9df652116dde789d351359f3d7e5d2285a2b2a1f04a2d7244df706d5ea9` |
| `NotoEmoji-Regular.ttf` | 沿用 `../Grid_Board/grid_board_app/assets/fonts/` 的原字体；[Noto Emoji 上游](https://github.com/googlefonts/noto-emoji) | `b57ed895ae9d09ba7b4b19c343a75cf39aad57156c3450e339d9d11556d7edc1` |

OFL 1.1 许可证随同保留于 `OFL-NotoSans.txt` 和 `OFL-NotoEmoji.txt`。Noto Sans SC 未再次裁剪，实际可显示范围以字体 cmap 为准。Emoji 使用单色轮廓，不是彩色表情；不执行多码点组合塑形。
