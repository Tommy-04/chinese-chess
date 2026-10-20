# STM32MP157 中国象棋：LVGL + 阿里云 TCP 中转

这是面向正点原子 STM32MP157 开发板（标配7寸 800×480 RGB framebuffer）的纯 C 中国象棋工程。界面使用 LVGL 8.3，棋盘规则在两端本地校验，联机时只传输坐标和结算消息，不传画面。

当前版本包含：

- 本地练习：同屏双人轮流走棋，无 AI、不联网。
- 云端对战：两块板都作为 TCP 客户端连接阿里云 ECS，输入相同房间号后自动配对。
- 局域网主机调试：主机监听本机端口，便于没有云服务器时做底层联调。
- 车、马、象、士、将、炮、兵规则；蹩马腿、塞象眼、炮架、兵过河、将帅照面和己方被将军校验。
- JSON 换行分帧协议，使用随工程提供的 cJSON。

## 目录

```text
Chinese/
  main.c                 STM32MP157 Linux framebuffer 入口
  Makefile               开发板构建文件
  lv_conf.h              LVGL 配置（800×480、中文字体）
  lv_drv_conf.h          framebuffer / evdev 配置
  lvgl/                   LVGL 8.3 源码
  lv_drivers/             Linux framebuffer、evdev 驱动
  src/chess.c/.h          棋盘数据和规则
  src/net.c/.h            TCP + cJSON 客户端/直连网络层
  src/ui.c/.h             LVGL 页面和交互
  server/relay_server.c   阿里云 ECS 房间中转服务
  src/chess_test.c        规则回归测试
```

## 部署阿里云中转服务

在 ECS（Ubuntu/Debian/CentOS 均可）执行：

```bash
cd server
make
./relay_server 8888
```

同时在 ECS 安全组和系统防火墙放行 TCP `8888`。如果要让服务在 Workbench 断开或服务器重启后自动恢复，上传 `server/xiangqi-relay.service` 后执行：

```bash
sudo cp /opt/xiangqi/server/xiangqi-relay.service /etc/systemd/system/xiangqi-relay.service
sudo systemctl daemon-reload
sudo systemctl enable --now xiangqi-relay
sudo systemctl status xiangqi-relay --no-pager
```

查看实时日志：

```bash
sudo journalctl -u xiangqi-relay -f
```

联机前先确认服务和端口已经就绪：

```bash
sudo systemctl is-active xiangqi-relay
sudo ss -ltnp | grep ':8888'
sudo ufw allow 8888/tcp       # 仅在启用 ufw 时执行
```

再从板端所在网络测试 ECS 公网端口；Windows 可执行：

```powershell
Test-NetConnection <ECS公网IP> -Port 8888
```

只有 `TcpTestSucceeded : True` 后，才进入 LVGL 的云端对战页面。若能 ping 通但 TCP 失败，优先检查 ECS 安全组、系统防火墙和 `xiangqi-relay` 服务状态。

如果此前在终端前台运行了 `./relay_server`，先按 `Ctrl+C` 停止它，再启用 systemd，避免 `8888` 端口被占用。当前示例是明文 TCP，适合课程设计和联调，不包含 TLS、账号鉴权或持久化。

## 构建 STM32MP157 版本

在 Linux 交叉编译环境中，使用 STM32MP1 专用硬浮点工具链：

```bash
make CC=arm-none-linux-gnueabihf-gcc
```

如果工具链名称不同，替换 `CC` 即可。生成文件为 `chinese_xiangqi`。`lv_conf.h`、`lv_drv_conf.h` 和 `lv_drivers` 只在编译阶段使用，板端运行时部署生成的 `chinese_xiangqi` 即可；程序默认打开 `/dev/fb0` 和 `/dev/input/event0`（正点原子 GT911 触摸默认就是 event0）。触摸设备不是 `event0` 时，用 `evtest` 命令查到对应触摸节点，修改 `lv_drv_conf.h` 的 `EVDEV_NAME` 后重新编译。

`lv_font_simsun_16_cjk.c` 已按当前 UI 和棋局文案生成，新增中文界面文字后需要重新生成该字体资源。

云端对战页面默认填入 `8.163.43.95`，也可以手动改成实际的阿里云 ECS 公网 IP。

板端启动前确认：

```bash
chmod +x chinese_xiangqi
./chinese_xiangqi
```

主菜单选择“联机对战 → 云端对战（阿里云）”，输入 ECS 公网 IP、`8888` 和房间号。两块板输入同一个房间号即可配对，服务器先连接的一方执红。

## 协议

每条消息以 `\n` 分隔，例如：

```json
{"type":"join","room":"xiangqi-demo"}
{"type":"hello","version":1,"color":"red"}
{"type":"move","from":[1,7],"to":[1,0]}
{"type":"result","winner":"red"}
{"type":"disconnect"}
```

服务器只负责配对和转发，不修改棋盘。收到的走棋消息仍需经过本地 `chess_is_legal_move` 校验，非法消息会被忽略。

## 测试

规则层、本机 TCP 回环协议测试以及 UI/中文字体对象的严格编译检查可在当前 Cygwin 或 Linux 环境运行：

```bash
make test
```

当前 Windows 环境没有 Linux 的 `linux/fb.h` / `linux/input.h` 头文件，因此不能在 Cygwin 上链接 framebuffer 最终程序；服务端和规则测试可正常编译，完整板端构建请在 Linux 交叉编译环境执行。
