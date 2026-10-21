# CardputerZero 屏幕与按键调试

在电脑执行：

```sh
./tools/remote_debug/start.sh pi@192.168.0.120
```

输入设备 SSH 密码后，会打开 http://127.0.0.1:18765 。在终端按 Ctrl+C 关闭隧道。
设备需要 Python 3、RGB565 framebuffer，以及当前用户可无密码 sudo（现有设备已满足）。
本电脑仅需 OpenSSH 和浏览器，不需要 Python 依赖。

- 屏幕从 `/dev/fb0` 读取，320×170，约每 180ms 请求一帧，实际帧率取决于网络和编码时间。
- 按键写入设备现有 evdev 键盘通道，应用与实体键盘收到相同事件。
- 支持按下、松开、长按和组合键。窗口失焦时释放按键；断连后的残留按键在 8 秒内自动释放。
- 屏幕按钮支持方向、Enter、Esc、Tab、空格。画面不是触摸输入。
- 符号键仍受设备自己的键盘布局影响。
- HTTP 仅监听设备回环地址，通过 SSH 转发；按键请求需要页面随机令牌。
- 设备桥接进程保留运行，重连时复用。停止设备进程可通过 SSH 执行
  `sudo pkill -f '^python3 /tmp/cardputer-debug-server.py'`。
- 默认读取主屏；其他布局可直接运行 `server.py --fb /dev/fb1 --keyboard <evdev-path>`。

验证：设备画面成功取回为 PNG；经 HTTP 发送的 Shift 按下/松开在设备 evdev 中验证为 `[1, 0]`；无令牌按键请求返回 403。
