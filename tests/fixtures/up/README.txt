# REAPERMedia 剪贴板 fixture 目录
#
# basic.txt      真实剪贴板 dump（用户提供）：81 个根级 ITEM + SOURCE，无 TRACK 块，
#                UTF-8 中文路径，含变速 item（PLAYRATE 多 token）。已启用精确值断言。
#
# 待采集：
#   multi_track.txt  多轨样本（TRACKSKIP 分隔）——当前无 TRACK/TRACKSKIP 样本覆盖
#   section.txt      SECTION 反向播放样本——当前无 SECTION 样本覆盖
#
# 采集方法：REAPER 中复制带媒体条目（Copy items with media）后运行
#   build/dump_clipboard.exe tests/fixtures/up/<文件名>
